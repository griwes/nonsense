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

#include <json.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace nonsensed
{
class options;
class service;

class entity
{
};

struct config_result
{
    int error_code;
    std::string error_message;
};

struct property_result
{
    int status;
    std::string property_value = "";
};

#define CONFIG_DECLARE_PROPERTY(name_)                                                                       \
    property_result _handle_property_##name_(                                                                \
        nlohmann::json::reference, entity_kind, std::string, std::string, sd_bus_error *) const

#define CONFIG_DEFINE_PROPERTY(name_)                                                                        \
    property_result config::_handle_property_##name_(                                                        \
        nlohmann::json::reference entity,                                                                    \
        entity_kind kind,                                                                                    \
        std::string name,                                                                                    \
        std::string property,                                                                                \
        sd_bus_error * error) const

#define CONFIG_PROPERTY(label, name_, ...)                                                                   \
    {                                                                                                        \
        label, &config::_handle_property_##name_ __VA_ARGS__                                                 \
    }

class config
{
public:
    config(const config & other);
    config(const options & opts);

    config & operator=(const config & other) noexcept;

    void install(const service & srv, const char * dbus_path);

    std::optional<entity> try_get(entity_kind kind, std::string_view name) const noexcept;
    config_result add(
        entity_kind kind,
        std::string name,
        std::vector<parameter_value> initial_parameters) noexcept;

    DECLARE_METHOD(lock);
    DECLARE_METHOD(unlock);
    DECLARE_METHOD(get);

private:
    dbus_slot _config_slot;
    bool _mutable;

    nlohmann::json _configuration;

    void _validate_metadata() const;
    void _ensure_top_level_keys();
    void _validate_entity(entity_kind kind, std::string name) const;
    void _validate_namespace(std::string name) const;
    void _validate_network(std::string name) const;
    void _validate_interface(std::string name) const;

    template<bool Masked>
    CONFIG_DECLARE_PROPERTY(downlink_address);
    template<bool Masked>
    CONFIG_DECLARE_PROPERTY(uplink_address);
    CONFIG_DECLARE_PROPERTY(uplink_device);
    CONFIG_DECLARE_PROPERTY(uplink_entity);

    using _property_handler_t = decltype(&config::_handle_property_uplink_entity);
};
}
