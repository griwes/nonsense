/*
 * Copyright © 2019-2020 Michał 'Griwes' Dominiak
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "transaction.h"

#include "log_helpers.h"
#include "service.h"

#include <asm-generic/errno-base.h>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace nonsensed
{
DEFINE_METHOD(transaction, serialize);
DEFINE_METHOD(transaction, add);
DEFINE_METHOD(transaction, set);
DEFINE_METHOD(transaction, delete);

DEFINE_PROPERTY_GET(transaction, owner);

static const sd_bus_vtable transaction_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Serialize", "", "s", transaction::method_serialize, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Add", "sa(ss)", "", transaction::method_add, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Set", "s(ss)", "", transaction::method_set, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Delete", "s", "", transaction::method_delete, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_PROPERTY("Owner", "u", transaction::property_owner_get, 0, SD_BUS_VTABLE_PROPERTY_CONST),

    SD_BUS_VTABLE_END
};

transaction::transaction(std::uint64_t id, const service & srv, uid_t owner) : _id{ id }, _owner{ owner }
{
    std::ostringstream os;
    os.fill('0');
    os << "/info/griwes/nonsense/configuration/transactions/" << std::hex << std::setw(16) << id;

    _object_path = std::move(os).str();

    int ret = sd_bus_add_object_vtable(
        srv.bus(),
        &_bus_slot,
        _object_path.c_str(),
        "info.griwes.nonsense.Transaction",
        transaction_vtable,
        this);
    assert(!(ret < 0)); // handle this better maybe?
}

METHOD_SIGNATURE(transaction, serialize)
{
    co_return reply_status(-ENOSYS);
}

METHOD_SIGNATURE(transaction, add)
{
    int status;

    uid_t owner;
    __attribute__((cleanup(sd_bus_creds_unrefp))) sd_bus_creds * creds = nullptr;
    status = sd_bus_query_sender_creds(message, SD_BUS_CREDS_UID | SD_BUS_CREDS_AUGMENT, &creds);
    assert(status >= 0);
    co_yield log_and_reply_on_error(
        sd_bus_creds_get_uid(creds, &owner), "Failed to get the originating uid of a message");

    if (owner != _owner && owner != 0)
    {
        co_return reply_status_const(
            -EACCES,
            "info.griwes.nonsense.AccessDenied",
            "You do not have permissions to modify this transaction.");
    }

    const char * name;

    co_yield log_and_reply_on_error(sd_bus_message_read(message, "s", &name), "Failed to parse parameters");

    add operation{ name, {} };

    co_yield log_and_reply_on_error(
        sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "(ss)"), "Failed to enter container");

    while ((status = sd_bus_message_enter_container(message, SD_BUS_TYPE_STRUCT, "ss")) > 0)
    {
        const char * parameter;
        const char * value;

        co_yield log_and_reply_on_error(
            sd_bus_message_read(message, "ss", &parameter, &value), "Failed to parse parameters");

        operation.initial_parameters.push_back({ parameter, value });

        co_yield log_and_reply_on_error(sd_bus_message_exit_container(message), "Failed to exit struct");
    }
    co_yield log_and_reply_on_error(status, "Failed to enter struct");

    co_yield log_and_reply_on_error(sd_bus_message_exit_container(message), "Failed to exit container");

    _operations.push_back(operation);

    co_return reply_status(sd_bus_reply_method_return(message, ""));
}

METHOD_SIGNATURE(transaction, set)
{
    co_return reply_status(-ENOSYS);
}

METHOD_SIGNATURE(transaction, delete)
{
    co_return reply_status(-ENOSYS);
}

PROPERTY_GET_SIGNATURE(transaction, owner)
{
    assert(path == _object_path);
    assert(interface == std::string_view("info.griwes.nonsense.Transaction"));
    assert(property == std::string_view("Owner"));

    return sd_bus_message_append(reply, "u", _owner);
}
}
