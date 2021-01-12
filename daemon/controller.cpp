/*
 * Copyright © 2019-2021 Michał 'Griwes' Dominiak
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

// TODO: move the docs below into a proper documentation system.

/**
 * Nonsense runtime management d-bus interfaces
 * ============================================
 *
 * The runtime management part of Nonsense exposes objects of the following d-bus interfaces in its d-bus
 * tree:
 *
 * info.griwes.nonsense.Controller
 * ===============================
 *
 * info.griwes.nonsense.Entity
 * ===========================
 *
 * info.griwes.nonsense.Component
 * ==============================
 *
 */

/**
 * Nonsense runtime management d-bus tree objects
 * ==============================================
 *
 * The /info/griwes/nonsense object is the main interface for controlling and querying the state of
 * entities managed by Nonsense.
 *
 * Under /info/griwes/nonsense. the following sub-objects are exposed:
 *  - /info/griwes/nonsense/entity, which is the collection of currently managed entities. This subtree also
 * presents specific objects for every entity.
 *
 * /info/griwes/nonsense
 * =====================
 * Interface:
 *  - info.griwes.nonsense.Controller
 *
 */

#include "controller.h"

#include "common_definitions.h"
#include "configuration.h"
#include "log_helpers.h"
#include "service.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace nonsensed
{
DEFINE_METHOD(controller, start);
DEFINE_METHOD(controller, stop);

static const sd_bus_vtable controller_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Start", "s", "", controller::method_start, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Stop", "s", "", controller::method_stop, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

controller::controller(const options & opts, configuration & configuration_object, const service & srv)
    : _srv{ srv }, _config(configuration_object.running())
{
    const char * dbus_path = "/info/griwes/nonsense";
    int ret = sd_bus_add_object_vtable(
        srv.bus(), &_slot, dbus_path, "info.griwes.nonsense.Controller", controller_vtable, this);
    if (ret < 0)
    {
        throw std::runtime_error(
            std::string("Failed to install the Controller interface at ") + dbus_path + ": "
            + strerror(-ret));
    }
}

METHOD_SIGNATURE(controller, start)
{
    const char * name;

    co_yield log_and_reply_on_error(sd_bus_message_read(message, "s", &name), "Failed to parse parameters");

    std::optional<entity> ent = _config.try_get(name);

    if (!ent)
    {
        co_return reply_status_format(
            -ENOENT,
            "info.griwes.nonsense.NoSuchEntity",
            "Attempted to start an entity that does not exist: %s.",
            name);
    }

    co_await ent->start();
    co_return reply_status(sd_bus_reply_method_return(message, ""));
}

METHOD_SIGNATURE(controller, stop)
{
    const char * name;

    co_yield log_and_reply_on_error(sd_bus_message_read(message, "s", &name), "Failed to parse parameters");

    std::optional<entity> ent = _config.try_get(name);

    if (!ent)
    {
        co_return reply_status_format(
            -ENOENT,
            "info.griwes.nonsense.NoSuchEntity",
            "Attempted to stop an entity that does not exist: %s.",
            name);
    }

    co_await ent->stop();
    co_return reply_status(sd_bus_reply_method_return(message, ""));
}
}
