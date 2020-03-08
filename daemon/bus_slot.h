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

#pragma once

#include <systemd/sd-bus.h>

#include <utility>

namespace nonsensed
{
class dbus_slot
{
public:
    dbus_slot() = default;

    dbus_slot(dbus_slot && other) : _slot{ std::exchange(other._slot, nullptr) }
    {
    }

    auto operator&()
    {
        return &_slot;
    }

    auto operator&() const
    {
        return &_slot;
    }

    ~dbus_slot()
    {
        reset();
    }

    void reset()
    {
        sd_bus_slot_unref(_slot);
    }

private:
    sd_bus_slot * _slot = nullptr;
};
}
