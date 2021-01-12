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

#include "config.h"
#include "transactions.h"

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace nonsensed
{
class options;
class service;

class configuration
{
public:
    configuration(const options & opts);
    void install(service & srv);

    config & running()
    {
        return _running_config;
    }

    const config & running() const
    {
        return _running_config;
    }

private:
    dbus_slot _bus_slot;

    config _saved_config;
    config _running_config;
    transactions _transaction_manager;
};
}
