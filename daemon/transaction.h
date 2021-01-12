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

#include "common_definitions.h"
#include "dbus.h"

#include <variant>
#include <vector>

namespace nonsensed
{
class service;

class transaction
{
public:
    struct add
    {
        std::string name;
        std::vector<parameter_value> initial_parameters;
    };

    struct set
    {
        std::string name;
        std::vector<parameter_value> modification;
    };

    struct delete_
    {
        std::string name;
    };

    transaction(std::uint64_t id, const service & srv, uid_t owner);

    const char * object_path() const
    {
        return _object_path.c_str();
    }

    uid_t owner() const
    {
        return _owner;
    }

    const auto & operations() const
    {
        return _operations;
    }

    DECLARE_METHOD(serialize);
    DECLARE_METHOD(add);
    DECLARE_METHOD(set);
    DECLARE_METHOD(delete);

    DECLARE_PROPERTY_GET(owner);

private:
    dbus_slot _bus_slot;

    std::uint64_t _id;
    std::string _object_path;

    const uint32_t _owner;
    std::vector<std::variant<add, set, delete_>> _operations;
};
}
