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

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

// This is not a real attempt to write this, just a mock-up to allow testing a subset of the service units
// before starting to work on the actual client.

int main(int argc, char ** argv)
{
    std::string lock_command = "lock";
    std::string unlock_command = "unlock";
    std::string main_command = "get";
    std::unordered_map<std::string_view, std::unordered_map<std::string_view, std::string_view>>
        configuration = { { "net.test",
                            { { "net", "192.168.17.0" },
                              { "net-address", "192.168.17.2" },
                              { "net-address-masked", "192.168.17.2/24" },
                              { "downlink-address", "192.168.17.1" },
                              { "downlink-address-masked", "192.168.17.1/24" },
                              { "uplink-namespace", "temporary-nonsense-uplink" } } },
                          { "router",
                            { { "downlink-address", "192.168.17.1" },
                              { "uplink-address-masked", "192.168.17.17/24" },
                              { "uplink-namespace", "nonsense:net.test" },
                              { "uplink-bridge", "nb-test" } } },
                          { "net.client",
                            { { "net", "192.168.27.0" },
                              { "net-address", "192.168.27.2" },
                              { "net-address-masked", "192.168.27.2/24" },
                              { "downlink-address", "192.168.27.1" },
                              { "downlink-address-masked", "192.168.27.1/24" },
                              { "uplink-namespace", "nonsense:router" } } },
                          { "client",
                            { { "downlink-address", "192.168.27.1" },
                              { "uplink-address-masked", "192.168.27.27/24" },
                              { "uplink-namespace", "nonsense:net.client" },
                              { "uplink-bridge", "nb-client" } } } };

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

    try
    {
        std::cout << configuration.at(argv[3]).at(argv[2]);
    }
    catch (...)
    {
        std::cerr << "<3>invalid keys: " << argv[3] << ", " << argv[2] << std::endl;
    }
}
