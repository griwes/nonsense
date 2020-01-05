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

#include "cli.h"

#include <cxxopts.hpp>
#include "log_helpers.h"

namespace nonsensed
{
options::options(int argc, char ** argv)
{
    cxxopts::Options opts{ "nonsensed", "Daemon for nonsense, the namespace engine." };

    // clang-format off
    opts.add_options()
        ("h,help", "Display this message.")
        ("c,config", "Select the configuration file to use.",
            cxxopts::value<std::string>()->default_value("/etc/nonsense/nonsensed.json"));
    // clang-format on

    auto result = opts.parse(argc, argv);

    if (argc != 1)
    {
        std::cerr << error_prefix() << "Error: Unrecognized options:\n";
        for (int i = 1; i < argc; ++i)
        {
            std::cerr << error_prefix() << "    " << argv[i] << '\n';
        }
        std::exit(1);
    }

    if (result.count("help"))
    {
        std::cout << opts.help() << '\n';
        std::exit(0);
    }

    _config_file = result["config"].as<std::string>();
}

std::string_view options::configuration_file() const
{
    return _config_file;
}
}
