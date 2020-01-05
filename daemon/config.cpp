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

#include "config.h"

#include "cli.h"
#include "log_helpers.h"
#include "service.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace nonsensed
{
DEFINE_METHOD(config, lock);
DEFINE_METHOD(config, unlock);
DEFINE_METHOD(config, get);

static const sd_bus_vtable config_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Get", "yss", "s", config::method_get, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

static const sd_bus_vtable mutable_config_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Lock", "ys", "", config::method_lock, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Unlock", "ys", "", config::method_unlock, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Get", "yss", "s", config::method_get, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

config::config(const config & other) : _mutable{ true }, _configuration{ other._configuration }
{
}

config::config(const options & opts) : _mutable{ false }
{
    auto config_path = opts.configuration_file();
    std::ifstream config_stream{ std::string(config_path), std::ios::in };
    if (!config_stream)
    {
        throw std::runtime_error{ "Failed to open the configuration file " + std::string(config_path) };
    }

    _configuration = nlohmann::json::parse(config_stream);

    _validate_metadata();
    _ensure_top_level_keys();

    for (auto kind : { entity_kind::namespace_, entity_kind::network, entity_kind::interface })
    {
        for (auto && [key, _] : _configuration[kind_to_plural(kind)].get<nlohmann::json::object_t>())
        {
            _validate_entity(kind, key);
        }
    }
}

config & config::operator=(const config & other) noexcept
{
    _configuration = other._configuration;
    return *this;
}

void config::install(const service & srv, const char * dbus_path)
{
    int ret = sd_bus_add_object_vtable(
        srv.bus(),
        &_config_slot,
        dbus_path,
        _mutable ? "info.griwes.nonsense.MutableConfig" : "info.griwes.nonsenes.Config",
        _mutable ? mutable_config_vtable : config_vtable,
        this);
    if (ret < 0)
    {
        throw std::runtime_error(
            std::string("Failed to install the Config interface at ") + dbus_path + ": " + strerror(-ret));
    }
}

std::optional<entity> config::try_get(entity_kind kind, std::string_view name) const noexcept
{
    auto & tree = _configuration[kind_to_plural(kind)];
    auto it = tree.find(name);
    if (it == tree.end())
    {
        return std::nullopt;
    }

    return std::make_optional(entity());
}

config_result config::add(
    entity_kind kind,
    std::string name,
    std::vector<parameter_value> initial_parameters) noexcept
{
    if (!_mutable)
    {
        return { -EROFS, "Cannot modify an immutable configuration." };
    }

    auto & tree = _configuration[kind_to_plural(kind)];
    if (tree.count(name))
    {
        return { -EEXIST,
                 "Cannot add entity " + std::string(kind_to_prefix(kind)) + "." + name
                     + ": entity already exists." };
    }

    auto & entity = tree[name];
    entity = nlohmann::json::object();

    for (auto && [key, value] : initial_parameters)
    {
        if (entity.count(key))
        {
            tree.erase(name);
            return { -EINVAL,
                     "Invalid entity configuration of " + std::string(kind_to_prefix(kind)) + "." + name
                         + ": value for parameter " + key + " specified more than once." };
        }

        entity[key] = value;
    }

    try
    {
        _validate_entity(kind, name);
    }
    catch (std::exception & err)
    {
        tree.erase(name);
        return { -EINVAL, err.what() };
    }

    return { 0, "" };
}

METHOD_SIGNATURE(config, lock)
{
    entity_kind kind;
    const char * name;
    int status;

    status = sd_bus_message_read(message, "ys", &kind, &name);
    HANDLE_DBUS_ERROR_RET("Failed to parse parameters", status);

    auto & tree = _configuration[kind_to_plural(kind)];
    auto it = tree.find(name);
    if (it == tree.end())
    {
        sd_bus_error_setf(
            error,
            "info.griwes.nonsense.NoSuchEntity",
            "Attempted to lock an entity that does not exist: %s.%s.",
            kind_to_prefix(kind),
            name);
        return -ENOENT;
    }

    auto && entity = *it;
    auto && lock = entity[":lock"];
    if (lock.is_null())
    {
        lock = 0;
    }

    if (!lock.is_number_integer())
    {
        std::cerr << error_prefix() << "Fatal error: entity lock is not an integer.\n";
        std::exit(1);
    }

    ++lock.get_ref<nlohmann::json::number_integer_t &>();

    return sd_bus_reply_method_return(message, "");
}

METHOD_SIGNATURE(config, unlock)
{
    entity_kind kind;
    const char * name;
    int status;

    status = sd_bus_message_read(message, "ys", &kind, &name);
    HANDLE_DBUS_ERROR_RET("Failed to parse parameters", status);

    auto & tree = _configuration[kind_to_plural(kind)];
    auto it = tree.find(name);
    if (it == tree.end())
    {
        sd_bus_error_setf(
            error,
            "info.griwes.nonsense.NoSuchEntity",
            "Attempted to unlock an entity that does not exist: %s.%s.",
            kind_to_prefix(kind),
            name);
        return -ENOENT;
    }

    auto && entity = *it;
    auto && lock = entity[":lock"];

    if (!lock.is_number_integer() || lock == 0)
    {
        sd_bus_error_setf(
            error,
            "info.griwes.nonsense.NotLocked",
            "Attempted to unlock an entity that is not locked: %s.%s.",
            kind_to_prefix(kind),
            name);
        return -EINVAL;
    }

    --lock.get_ref<nlohmann::json::number_integer_t &>();

    return sd_bus_reply_method_return(message, "");
}

std::string _nth_address_in_subnet(std::string_view net_value, int n, bool include_mask = true)
{
    auto sep = net_value.find('/');
    assert(sep != std::string_view::npos);

    auto network = net_value.substr(0, sep);
    auto mask = net_value.substr(sep + 1);
    assert(mask == "24" && "this logic needs updating when network addresses other than /24 are supported");

    auto last_octet = network.rfind('.');
    auto prefix = network.substr(0, last_octet);

    std::ostringstream os;
    os << prefix << '.' << n;
    if (include_mask)
    {
        os << '/' << mask;
    }

    return os.str();
}

template<bool Masked>
CONFIG_DEFINE_PROPERTY(downlink_address)
{
    switch (kind)
    {
        // FIXME: ...I really need a proper IP address library for this
        case entity_kind::network:
        {
            auto & net = entity["address"];
            std::string_view net_value = net.get<nlohmann::json::string_t>();

            return { 0, _nth_address_in_subnet(net_value, 1, Masked) };
        }

        default:
            sd_bus_error_set_const(
                error,
                "info.griwes.nonsense.NotImplementedYet",
                "Getting the downlink address of a non-network entity is not supported yet.");
            return { -ENOSYS };
    }
}

template<bool Masked>
CONFIG_DEFINE_PROPERTY(uplink_address)
{
    switch (kind)
    {
        // FIXME: ...I really need a proper IP address library for this
        case entity_kind::network:
        {
            auto & net = entity["address"];
            std::string_view net_value = net.get<nlohmann::json::string_t>();

            return { 0, _nth_address_in_subnet(net_value, 2, Masked) };
        }

        default:
            sd_bus_error_set_const(
                error,
                "info.griwes.nonsense.NotImplementedYet",
                "Getting the uplink address of a non-network entity is not supported yet.");
            return { -ENOSYS };
    }
}

CONFIG_DEFINE_PROPERTY(uplink_device)
{
    return { 0, (kind == entity_kind::network ? "nb-" : "nu-") + name };
}

CONFIG_DEFINE_PROPERTY(uplink_entity)
{
    auto & uplink = entity["uplink"];
    std::string_view value = uplink.get<nlohmann::json::string_t>();
    auto [uplink_kind, uplink_name] = parse_prefixed_name(value);

    auto & uplink_tree = _configuration[kind_to_plural(uplink_kind)];
    auto & uplink_object = uplink_tree[uplink_name];

    return { 0,
             uplink_object["type"] == "netns-external" ? uplink_name : ("nonsense:" + std::string(value)) };
}

METHOD_SIGNATURE(config, get)
{
    entity_kind kind;
    const char * name;
    const char * property;
    int status;

    status = sd_bus_message_read(message, "yss", &kind, &name, &property);
    HANDLE_DBUS_ERROR_RET("Failed to parse parameters", status);

    auto & tree = _configuration[kind_to_plural(kind)];
    auto it = tree.find(name);
    if (it == tree.end())
    {
        sd_bus_error_setf(
            error,
            "info.griwes.nonsense.NoSuchEntity",
            "Attempted to get a property of an entity that does not exist: %s.%s.",
            kind_to_prefix(kind),
            name);
        return -ENOENT;
    }

    static std::unordered_map<std::string_view, _property_handler_t> property_handlers = {
        CONFIG_PROPERTY("downlink-address", downlink_address<false>),
        CONFIG_PROPERTY("downlink-address-masked", downlink_address<true>),
        CONFIG_PROPERTY("uplink-address-masked", uplink_address<true>),
        CONFIG_PROPERTY("uplink-device", uplink_device),
        CONFIG_PROPERTY("uplink-entity", uplink_entity)
    };

    auto handler_it = property_handlers.find(property);
    if (handler_it == property_handlers.end())
    {
        sd_bus_error_setf(
            error,
            "info.griwes.nonsense.NoSuchProperty",
            "Attempted to get a nonexistant property %s of entity %s.%s.",
            property,
            kind_to_prefix(kind),
            name);
        return -ENOENT;
    }

    auto [result, property_value] = (this->*(handler_it->second))(*it, kind, name, property, error);
    if (result != 0)
    {
        return result;
    }

    return sd_bus_reply_method_return(message, "s", property_value.c_str());
}

void config::_validate_metadata() const
{
    if (!_configuration.count("!metadata"))
    {
        throw std::runtime_error("Invalid configuration file: no !metadata section.");
    }

    auto && metadata = _configuration["!metadata"];
    if (!metadata.is_object())
    {
        throw std::runtime_error("Invalid configuration file: the !metadata section is not a JSON object.");
    }

    std::unordered_set<std::string_view> allowed_metadata_keys = { "version" };

    for (auto && [key, value] : metadata.get<nlohmann::json::object_t>())
    {
        if (allowed_metadata_keys.count(key) != 1)
        {
            throw std::runtime_error("Invalid !metadata parameter: " + key + ".");
        }
    }

    auto version_it = metadata.find("version");
    if (version_it == metadata.end())
    {
        throw std::runtime_error("Invalid configuration file: the format version has not been specified in "
                                 "the !metadata section.");
    }

    if (*version_it != 1)
    {
        throw std::runtime_error(
            "Invalid configuration file: unsupported configuration format version: "
            + std::string(*version_it) + ".");
    }
}

void config::_ensure_top_level_keys()
{
    for (auto && key : { "!metadata", "namespaces", "networks", "interfaces" })
    {
        auto it = _configuration.find(key);
        if (it == _configuration.end())
        {
            _configuration[key] = nlohmann::json::object();
            continue;
        }

        if (!it->is_object())
        {
            throw std::runtime_error(
                "Invalid configuration file: the section " + std::string(key) + " is not a JSON object.");
        }
    }
}

void config::_validate_entity(entity_kind kind, std::string name) const
{
    switch (kind)
    {
        case entity_kind::namespace_:
            _validate_namespace(name);
            return;
        case entity_kind::network:
            _validate_network(name);
            return;
        case entity_kind::interface:
            _validate_interface(name);
            return;
    }

    __builtin_unreachable();
}

void config::_validate_namespace(std::string name) const
{
    auto & entity = _configuration["namespaces"][name];

    if (entity["type"] == "netns-external")
    {
        if (entity.size() != 1)
        {
            throw std::runtime_error(
                "Invalid configuration for namespace " + name
                + ": namespace type 'netns-external' does not support any other parameters.");
        }
    }

    else
    {
        throw std::runtime_error(
            "Invalid configuration for namespace " + name + ": unknown namespace type '"
            + std::string(entity["type"]) + "'.");
    }
}

void config::_validate_network(std::string name) const
{
    auto & entity = _configuration["networks"][name];

    for (auto && [key, value] : entity.get<nlohmann::json::object_t>())
    {
        if (key == "uplink")
        {
            if (value.is_string())
            {
                auto upstream = parse_prefixed_name(value.get<nlohmann::json::string_t>());
                auto & tree = _configuration[kind_to_plural(upstream.kind)];
                if (!tree.count(upstream.name))
                {
                    throw std::runtime_error(
                        "Invalid configuration for network " + name + ": unknown upstream "
                        + std::string(value) + " specified.");
                }
            }

            else if (value.is_array())
            {
                throw std::runtime_error(
                    "Invalid configuration for network " + name
                    + ": multiple network uplinks are not supported yet.");
            }

            else
            {
                throw std::runtime_error(
                    "Invalid configuration for network " + name
                    + ": unrecognized format of the 'uplink' field.");
            }
        }
    }
}

void config::_validate_interface(std::string name) const
{
    throw std::runtime_error("Interfaces are not supported yet");
}
}
