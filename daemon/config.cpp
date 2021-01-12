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
DEFINE_METHOD(config, get);

static const sd_bus_vtable config_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Get", "yss", "s", config::method_get, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

static const sd_bus_vtable mutable_config_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("Get", "yss", "s", config::method_get, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

config::config(const config & other) : _mutable{ true }, _configuration(other._configuration)
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

    for (auto && [key, ns] : _configuration.items())
    {
        if (key == "!metadata")
        {
            continue;
        }

        _validate_entity(key, ns);
    }
}

config & config::operator=(const config & other) noexcept
{
    _configuration = other._configuration;
    return *this;
}

void config::install(service & srv, const char * dbus_path)
{
    _srv = &srv;

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

service & config::get_service() const
{
    return *_srv;
}

std::optional<entity> config::try_get(std::string_view name) noexcept
{
    auto it = _configuration.find(name);
    if (it == _configuration.end())
    {
        return std::nullopt;
    }

    return std::make_optional(entity(*this, *it, name));
}

config_result config::add(std::string name, std::vector<parameter_value> initial_parameters) noexcept
{
    if (!_mutable)
    {
        return { -EROFS, "Cannot modify an immutable configuration." };
    }

    if (_configuration.count(name))
    {
        return { -EEXIST, "Cannot add entity " + name + ": entity already exists." };
    }

    auto & entity = _configuration[name];
    entity = nlohmann::json::object();

    for (auto && [key, value] : initial_parameters)
    {
        auto * tree = &entity;
        auto start_pos = 0;

        while (auto dot_pos = key.find('.', start_pos))
        {
            if (dot_pos == std::string::npos)
            {
                break;
            }

            auto subkey = std::string_view(&key[start_pos], dot_pos - start_pos);

            if (tree->is_null())
            {
                *tree = nlohmann::json::object();
            }

            if (!tree->is_object())
            {
                return { -EINVAL,
                         "Invalid entity configuration of '" + name + "': value specified for parameter "
                             + key + ", but " + key.substr(0, dot_pos) + " is a value, not an object." };
            }

            tree = &(*tree)[std::string(subkey)];
            start_pos = dot_pos + 1;
        }

        tree = &(*tree)[key.substr(start_pos)];

        if (!tree->is_null())
        {
            _configuration.erase(name);
            return { -EINVAL,
                     "Invalid entity configuration of '" + name + "': value for parameter " + key
                         + " specified more than once." };
        }

        auto possible_object =
            nlohmann::json::parse(value, /* callback = */ nullptr, /* allow_exceptions = */ false);

        if (possible_object.is_discarded())
        {
            *tree = value;
        }
        else
        {
            *tree = std::move(possible_object);
        }
    }

    try
    {
        _validate_entity(name, entity);
    }
    catch (std::exception & err)
    {
        _configuration.erase(name);
        return { -EINVAL, err.what() };
    }

    return { 0, "" };
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

METHOD_SIGNATURE(config, get)
{
    const char * name;
    const char * property;

    co_yield log_and_reply_on_error(
        sd_bus_message_read(message, "ss", &name, &property), "Failed to parse parameters");

    assert(0);
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

    for (auto && [key, value] : metadata.get_ref<const nlohmann::json::object_t &>())
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

void config::_validate_entity(std::string_view name, nlohmann::json & ns)
{
    for (auto && [type, component] : ns.items())
    {
        auto it = known_components.find(type);
        if (it == known_components.end())
        {
            throw std::runtime_error(
                "Invalid configuration for entity " + std::string(name) + ": unknown component type '" + type
                + "'.");
        }

        switch (it->second)
        {
            case component_type::network:
                _validate_network(name, component);
                break;
        }
    }
}

void config::_validate_network(std::string_view name, nlohmann::json & component)
{
    auto role = component["role"].get_ref<std::string &>();
    auto it = known_network_roles.find(role);
    if (it == known_network_roles.end())
    {
        throw std::runtime_error(
            "Invalid configuration: unknown role '" + std::string(role)
            + "' of the network component of entity '" + std::string(name) + "'.");
    }

    component[":role"] = it->second;

    enum class network_parameter
    {
        role,
        external,
        default_,
        address,
        uplink,
    };

    static std::unordered_map<std::string_view, network_parameter> known_parameters = {
        { "role", network_parameter::role },
        { "external", network_parameter::external },
        { "default", network_parameter::default_ },
        { "address", network_parameter::address },
        { "uplink", network_parameter::uplink }
    };

    for (auto && [key, value] : component.items())
    {
        if (key.starts_with(':'))
        {
            continue;
        }

        auto it = known_parameters.find(key);
        if (it == known_parameters.end())
        {
            throw std::runtime_error(
                "Invalid configuration: uknown parameter '" + key + "' of the network component of entity '"
                + std::string(name) + "'.");
        }

        switch (it->second)
        {
            case network_parameter::role:
                // already handled above
                break;

            case network_parameter::external:
                break;

            case network_parameter::default_:
            {
                auto & default_netns = _configuration[":default-netns"];
                if (default_netns.is_null())
                {
                    default_netns = std::string(name);
                    break;
                }

                if (default_netns != name)
                {
                    throw std::runtime_error(
                        "Invalid configuration: multiple entities specified as having their network "
                        "components be the default netns: '"
                        + std::string(name) + "' and '" + std::string(default_netns) + "'.");
                }

                break;
            }

            case network_parameter::address:
                if (component[":role"] != network_role::switch_)
                {
                    throw std::runtime_error(
                        "Invalid configuration: an address is specified for the network component of "
                        "entity '"
                        + std::string(name)
                        + "', but the role of the network component role is not 'switch'.");
                }

                // TODO validate it's a valid IP address

                break;

            case network_parameter::uplink:
            {
                auto uplink_name = component["uplink"];

                auto it = _configuration.find(component["uplink"]);
                if (it == _configuration.end())
                {
                    throw std::runtime_error(
                        "Invalid configuration: unknown entity specified as the uplink of the network "
                        "component of entity '"
                        + std::string(name) + "'.");
                }

                if (!it->is_object())
                {
                    throw std::runtime_error(
                        "Invalid configuration: the entity '" + std::string(uplink_name)
                        + "' is not a JSON object.");
                }

                auto & uplink_entity = it->get_ref<nlohmann::json::object_t &>();

                auto network_it = uplink_entity.find("network");
                if (network_it == uplink_entity.end())
                {
                    throw std::runtime_error(
                        "Invalid configuration: the entity '" + std::string(uplink_name)
                        + "', specified as the uplink of the network component of entity '"
                        + std::string(name) + "', does not have a network component.");
                }

                if (network_it->second.is_string())
                {
                    assert(!"component delegation not implemented yet");
                }

                if (!network_it->second.is_object())
                {
                    throw std::runtime_error(
                        "Invalid configuration: the network component of entity '" + std::string(uplink_name)
                        + "' is not a JSON object.");
                }

                auto & uplink_net = network_it->second.get_ref<nlohmann::json::object_t &>();

                // Must check "role" and not ":role", because ":role" may not be populated yet!
                if (uplink_net["role"] == "client")
                {
                    throw std::runtime_error(
                        "Invalid configuration: the role of the network component of entity '"
                        + std::string(uplink_name)
                        + "', specified as the uplink for the network component of entity '"
                        + std::string(name) + "', is 'client', which cannot be used as an uplink.'");
                }

                // It's okay then.
                break;
            }
        }
    }
}
}
