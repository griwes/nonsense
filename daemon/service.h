/*
 * Copyright © 2019, 2021 Michał 'Griwes' Dominiak
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

extern "C"
{
    struct sd_bus;
}

namespace nonsensed
{
class options;
class configuration;

class service
{
public:
    service(const options & opts, configuration & config_object);

    void loop();

    sd_bus * bus() const
    {
        return _bus;
    }

    void register_bus(sd_bus * bus);
    void unregister_bus(sd_bus * bus);

private:
    int _epoll_fd = -1;
    sd_bus * _bus = nullptr;
};
}
