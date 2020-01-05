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

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nonsensed
{
enum class entity_kind : std::uint8_t
{
    namespace_ = 1,
    network = 2,
    interface = 3
};

inline const char * kind_to_prefix(entity_kind kind)
{
    switch (kind)
    {
        case entity_kind::namespace_:
            return "ns";
        case entity_kind::network:
            return "net";
        case entity_kind::interface:
            return "int";
    }

    __builtin_unreachable();
}

inline const char * kind_to_singular(entity_kind kind)
{
    switch (kind)
    {
        case entity_kind::namespace_:
            return "namespace";
        case entity_kind::network:
            return "network";
        case entity_kind::interface:
            return "interface";
    }

    __builtin_unreachable();
}

inline const char * kind_to_plural(entity_kind kind)
{
    switch (kind)
    {
        case entity_kind::namespace_:
            return "namespaces";
        case entity_kind::network:
            return "networks";
        case entity_kind::interface:
            return "interfaces";
    }

    __builtin_unreachable();
}

inline std::optional<entity_kind> try_singular_to_kind(std::string_view singular)
{
    if (singular == "namespace")
    {
        return std::make_optional(entity_kind::namespace_);
    }
    if (singular == "network")
    {
        return std::make_optional(entity_kind::network);
    }
    if (singular == "interface")
    {
        return std::make_optional(entity_kind::interface);
    }

    return std::nullopt;
}

struct kind_and_name
{
    entity_kind kind;
    std::string name;
};

inline std::optional<kind_and_name> try_parse_prefixed_name(std::string_view prefixed)
{
    if (prefixed.starts_with("ns."))
    {
        return { { entity_kind::namespace_, std::string(prefixed.substr(3)) } };
    }

    if (prefixed.starts_with("net."))
    {
        return { { entity_kind::network, std::string(prefixed.substr(4)) } };
    }

    if (prefixed.starts_with("int."))
    {
        return { { entity_kind::interface, std::string(prefixed.substr(4)) } };
    }

    return std::nullopt;
}

inline kind_and_name parse_prefixed_name(std::string_view prefixed)
{
    auto attempt = try_parse_prefixed_name(prefixed);
    if (attempt)
    {
        return std::move(attempt.value());
    }

    throw std::runtime_error("No valid prefix found in a prefixed name " + std::string(prefixed) + ".");
}

struct parameter_value
{
    std::string parameter;
    std::string value;
};
}
