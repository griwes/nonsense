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

#include <cstdint>
#include <map>
#include <memory>

#include "dbus.h"

namespace nonsensed
{
class service;
class config;
class transaction;

class transactions
{
public:
    transactions(config & saved, config & running);
    ~transactions();

    void install(const service & srv, const char * dbus_path);

    DECLARE_METHOD(list);
    DECLARE_METHOD(new);
    DECLARE_METHOD(commit);
    DECLARE_METHOD(discard);

private:
    config & _saved_config;
    config & _running_config;

    const service * _srv;
    dbus_slot _bus_slot;

    std::map<std::uint64_t, std::unique_ptr<transaction>> _transactions;
};
}

