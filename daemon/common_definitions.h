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

#include <string>
#include <unordered_map>

namespace nonsensed
{
enum class component_type
{
    network
};

inline const std::unordered_map<std::string_view, component_type> known_components = {
    { "network", component_type::network }
};

enum class network_role
{
    root,
    interface,
    router,
    switch_,
    client
};

inline const std::unordered_map<std::string_view, network_role> known_network_roles = {
    { "root", network_role::root },
    { "interface", network_role::interface },
    { "router", network_role::router },
    { "switch", network_role::switch_ },
    { "client", network_role::client }
};

struct parameter_value
{
    std::string parameter;
    std::string value;
};
}
