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

#include "dbus.h"

extern "C"
{
    struct sd_bus_slot;
}

namespace nonsensed
{
class options;
class config;
class configuration;
class service;

class controller
{
public:
    controller(const options & opts, configuration & configuration_object, const service & srv);

    DECLARE_METHOD(start);
    DECLARE_METHOD(stop);

private:
    const service & _srv;
    sd_bus_slot * _slot = nullptr;
    config & _config;
};
}
