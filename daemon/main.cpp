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

#include <iostream>

#include "cli.h"
#include "configuration.h"
#include "controller.h"
#include "service.h"

int main(int argc, char ** argv)
try
{
    auto opts = nonsensed::options(argc, argv);

    auto config = nonsensed::configuration(opts);
    auto service = nonsensed::service(opts, config);
    auto control = nonsensed::controller(opts, config, service);

    service.loop();

    return 0;
}
catch (std::exception & ex)
{
    std::cerr << "Fatal error: " << ex.what() << '\n';
    return 1;
}
