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

#pragma once

#include <cstdlib>
#include <string_view>

namespace nonsensed
{
inline std::string_view error_prefix()
{
    static char * nonsensed_mode = std::getenv("NONSENSED_MODE");
    static bool is_systemd_service = nonsensed_mode && std::string_view(nonsensed_mode) == "systemd_service";
    return is_systemd_service ? std::string_view("<3>") : std::string_view("");
}
}
