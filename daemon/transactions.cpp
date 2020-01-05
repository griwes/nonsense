/*
 * Copyright © 2019 Michał 'Griwes' Dominiak
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

#include "transactions.h"

#include "config.h"
#include "log_helpers.h"
#include "overloads.h"
#include "service.h"
#include "transaction.h"

#include <cassert>
#include <iostream>
#include <random>
#include <systemd/sd-bus.h>

namespace nonsensed
{
DEFINE_METHOD(transactions, list);
DEFINE_METHOD(transactions, new);
DEFINE_METHOD(transactions, commit);
DEFINE_METHOD(transactions, discard);

static const sd_bus_vtable transaction_mgr_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("List", "", "a(to)", transactions::method_list, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("New", "", "to", transactions::method_new, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Commit", "t", "", transactions::method_commit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Discard", "t", "", transactions::method_discard, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

transactions::transactions(config & saved, config & running)
    : _saved_config{ saved }, _running_config{ running }
{
}

transactions::~transactions() = default;

void transactions::install(const service & srv, const char * dbus_path)
{
    _srv = &srv;

    int ret = sd_bus_add_object_vtable(
        srv.bus(),
        &_bus_slot,
        dbus_path,
        "info.griwes.nonsense.TransactionManager",
        transaction_mgr_vtable,
        this);
    if (ret < 0)
    {
        throw std::runtime_error(
            std::string("Failed to install the TransactionManager interface at ") + dbus_path + ": "
            + strerror(-ret));
    }
}

METHOD_SIGNATURE(transactions, list)
{
    return -1;
}

METHOD_SIGNATURE(transactions, new)
{
    static std::random_device dev;
    static std::mt19937 engine(dev());
    auto rand = [] {
        using dist = std::uniform_int_distribution<std::uint64_t>;
        return dist()(engine);
    };

    int status;

    status = sd_bus_message_read(message, "");
    HANDLE_DBUS_ERROR_RET("Failed to parse parameters", status);

    std::uint64_t id = rand();
    while (_transactions.count(id))
    {
        id = rand();
    }

    uid_t owner;
    __attribute__((cleanup(sd_bus_creds_unrefp))) sd_bus_creds * creds = nullptr;
    status = sd_bus_query_sender_creds(message, SD_BUS_CREDS_UID | SD_BUS_CREDS_AUGMENT, &creds);
    assert(status >= 0);
    status = sd_bus_creds_get_uid(creds, &owner);
    HANDLE_DBUS_ERROR_RET("Failed to get the originating uid of a message", status);

    auto [it, ins] =
        _transactions.emplace(std::make_pair(id, std::make_unique<transaction>(id, *_srv, owner)));
    assert(ins);

    return sd_bus_reply_method_return(message, "to", id, it->second->object_path());
}

METHOD_SIGNATURE(transactions, commit)
{
    std::uint64_t id;
    int status;

    status = sd_bus_message_read(message, "t", &id);
    HANDLE_DBUS_ERROR_RET("Failed to parse parameters", status);

    auto it = _transactions.find(id);
    if (it == _transactions.end())
    {
        sd_bus_error_set_const(
            error,
            "info.griwes.nonsense.InvalidTransactionId",
            "The transaction ID requested to be committed is not valid.");
        return -EINVAL;
    }

    uid_t owner;
    __attribute__((cleanup(sd_bus_creds_unrefp))) sd_bus_creds * creds = nullptr;
    status = sd_bus_query_sender_creds(message, SD_BUS_CREDS_UID | SD_BUS_CREDS_AUGMENT, &creds);
    assert(status >= 0);
    status = sd_bus_creds_get_uid(creds, &owner);
    HANDLE_DBUS_ERROR_RET("Failed to get the originating uid of a message", status);

    if (owner != it->second->owner() && owner != 0)
    {
        sd_bus_error_set_const(
            error,
            "info.griwes.nonsense.AccessDenied",
            "You do not have permissions to commit this transaction.");
        return -EACCES;
    }

    auto running_copy = _running_config;

    auto visitor =
        overload{ [&](transaction::add add) {
                     auto entity = running_copy.try_get(add.kind, add.name);
                     if (entity)
                     {
                         sd_bus_error_setf(
                             error,
                             "info.griwes.nonsense.EntityAlreadyExists",
                             "The transaction attempted to create an entity that already exists: %s.%s.",
                             kind_to_prefix(add.kind),
                             add.name.c_str());
                         return -EEXIST;
                     }

                     auto [result, message] = running_copy.add(add.kind, add.name, add.initial_parameters);
                     if (result < 0)
                     {
                         sd_bus_error_set(
                             error, "info.griwes.nonsense.InvalidEntityParameters", message.c_str());
                         return result;
                     }

                     return 0;
                 },

                  [&](auto &&) {
                      sd_bus_error_set_const(
                          error,
                          "info.griwes.nonsense.NotImplementedYet",
                          "A transaction containing this kind of an operation is not implemented yet.");
                      return -ENOSYS;
                  } };

    for (auto && operation : it->second->operations())
    {
        if ((status = std::visit(visitor, operation)) != 0)
        {
            return status;
        }
    }

    _running_config = running_copy;

    sd_bus_reply_method_return(message, "");
    return 0;
}

METHOD_SIGNATURE(transactions, discard)
{
    std::uint64_t id;
    int status;

    status = sd_bus_message_read(message, "t", &id);
    HANDLE_DBUS_ERROR_RET("Failed to parse parameters", status);

    auto it = _transactions.find(id);
    if (it == _transactions.end())
    {
        sd_bus_error_set_const(
            error,
            "info.griwes.nonsense.InvalidTransactionId",
            "The transaction ID requested to be discarded is not valid.");
        return -EINVAL;
    }

    uid_t owner;
    __attribute__((cleanup(sd_bus_creds_unrefp))) sd_bus_creds * creds = nullptr;
    status = sd_bus_query_sender_creds(message, SD_BUS_CREDS_UID | SD_BUS_CREDS_AUGMENT, &creds);
    assert(status >= 0);
    status = sd_bus_creds_get_uid(creds, &owner);
    HANDLE_DBUS_ERROR_RET("Failed to get the originating uid of a message", status);

    if (owner != it->second->owner() && owner != 0)
    {
        sd_bus_error_set_const(
            error,
            "info.griwes.nonsense.AccessDenied",
            "You do not have permissions to modify this transaction.");
        return -EACCES;
    }

    _transactions.erase(id);

    return sd_bus_reply_method_return(message, "");
}
}
