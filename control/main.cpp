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
#include <string>
#include <vector>

// This is not a real attempt to write this, just a mock-up to allow testing a subset of the service units
// before starting to work on the actual client.

int main(int argc, char ** argv)
{
    std::string lock_command = "lock";
    std::string unlock_command = "unlock";
    std::string main_command = "get";
    std::vector<std::string> get_subcommands = { "net",
                                                 "net-address",
                                                 "net-address-masked",
                                                 "downlink-address",
                                                 "downlink-address-masked",
                                                 "uplink-namespace" };

    if (argv[1] == lock_command)
    {
        assert(argc == 3);
        return 0;
    }
    if (argv[1] == unlock_command)
    {
        assert(argc == 3);
        return 0;
    }

    assert(argc == 4);
    assert(argv[1] == main_command);

    if (argv[2] == get_subcommands[0])
    {
        std::cout << "192.168.17.0";
    }
    else if (argv[2] == get_subcommands[1])
    {
        std::cout << "192.168.17.2";
    }
    else if (argv[2] == get_subcommands[2])
    {
        std::cout << "192.168.17.2/24";
    }
    else if (argv[2] == get_subcommands[3])
    {
        std::cout << "192.168.17.1";
    }
    else if (argv[2] == get_subcommands[4])
    {
        std::cout << "192.168.17.1/24";
    }
    else if (argv[2] == get_subcommands[5])
    {
        std::cout << "temporary-nonsense-uplink";
    }
    else
    {
        assert(0);
    }
}
