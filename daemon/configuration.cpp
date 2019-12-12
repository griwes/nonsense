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

#include "configuration.h"
#include "service.h"

#include <systemd/sd-bus.h>

#include <stdexcept>
#include <string>

static const sd_bus_vtable configuration_vtable[] = { SD_BUS_VTABLE_START(0), SD_BUS_VTABLE_END };

namespace nonsensed
{
configuration::configuration(const options & opts)
{
}

void configuration::install(const service & srv)
{
    int ret;

    ret = sd_bus_add_object_vtable(
        srv.bus(),
        &_slot,
        "/info/griwes/nonsense/configuration",
        "info.griwes.nonsense.configuration",
        configuration_vtable,
        this);
    if (ret < 0)
    {
        throw std::runtime_error(std::string("Failed to create the configuration object: ") + strerror(-ret));
    }
}
}
