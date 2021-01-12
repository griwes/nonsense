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

#pragma once

#include "common_definitions.h"
#include "dbus.h"
#include "entity.h"
#include "function.h"

#include <json.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace nonsensed
{
class options;
class service;

class config;

struct config_result
{
    int error_code;
    std::string error_message;
};

class config
{
public:
    friend class entity;

    config(const config & other);
    config(const options & opts);

    config & operator=(const config & other) noexcept;

    void install(service & srv, const char * dbus_path);
    service & get_service() const;

    std::optional<entity> try_get(std::string_view name) noexcept;
    config_result add(std::string name, std::vector<parameter_value> initial_parameters) noexcept;

    DECLARE_METHOD(get);

private:
    service * _srv = nullptr;
    dbus_slot _config_slot;
    bool _mutable;

    nlohmann::json _configuration;

    void _validate_metadata() const;
    void _validate_entity(std::string_view name, nlohmann::json & ns);
    void _validate_network(std::string_view name, nlohmann::json & component);
};
}
