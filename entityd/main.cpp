/*
 * Copyright © 2020-2021 Michał 'Griwes' Dominiak
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

#include "cleanup.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "../daemon/common_definitions.h"
#include "../daemon/log_helpers.h"

#include <systemd/sd-bus-vtable.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-id128.h>

#include <unistd.h>

#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <json.hpp>

sd_bus * bus;

std::string name;
std::unordered_map<nonsensed::component_type, nlohmann::json> current_components;

entityd::cleanup cleanups;
entityd::cleanup connection_cleanups;

void annotated_system(std::string command)
{
    std::cerr << "system(" << command << ")\n";
    auto ret = system(command.c_str());
    if (ret != 0)
    {
        std::cerr << "system(" << command << ") failed";
        throw std::runtime_error("system(" + command + ") failed");
    }
}

void setup_interfaces()
{
    entityd::cleanup clean;

    annotated_system("ip link add nu-" + name + " type veth peer nd-" + name);
    clean.add([] { system(("ip link del nu-" + name).c_str()); });
    annotated_system("ip link set nu-" + name + " up");

    cleanups.add(std::move(clean));
}

void setup_bridge()
{
    entityd::cleanup clean;

    annotated_system("ip link add nb-" + name + " type bridge");
    clean.add([] { system(("ip link del nb-" + name).c_str()); });
    annotated_system("ip link set nb-" + name + " up");
    annotated_system("ip link set nu-" + name + " master nb-" + name);

    cleanups.add(std::move(clean));
}

void setup_nft()
{
    entityd::cleanup clean;

    assert(0);

    cleanups.add(std::move(clean));
}

std::string nth_address_in_subnet(std::string_view net_value, int n, bool include_mask = true)
{
    auto sep = net_value.find('/');
    assert(sep != std::string_view::npos);

    auto network = net_value.substr(0, sep);
    auto mask = net_value.substr(sep + 1);
    assert(mask == "24" && "this logic needs updating when network addresses other than /24 are supported");

    auto last_octet = network.rfind('.');
    auto prefix = network.substr(0, last_octet);

    std::ostringstream os;
    os << prefix << '.' << n;
    if (include_mask)
    {
        os << '/' << mask;
    }

    return os.str();
}

void connect()
{
    entityd::cleanup clean;

    auto & component = current_components.at(nonsensed::component_type::network);

    auto get = [](auto && component, auto name) -> auto &
    {
        return component[name].template get_ref<std::string &>();
    };

    auto & uplink_name = get(component, ":uplink-name");

    annotated_system("ip link set nd-" + name + " netns nonsense:" + uplink_name);
    clean.add([=] {
        system(
            ("ip netns exec nonsense:" + uplink_name + " ip link set nd-" + name + " netns nonsense:" + name)
                .c_str());
    });

    if (component["role"] == "switch")
    {
        auto & net = get(component, "address");
        auto downlink_address = nth_address_in_subnet(net, 1, true);

        annotated_system(
            "ip netns exec nonsense:" + uplink_name + " ip addr add " + downlink_address + " dev nd-" + name);
        clean.add([=] {
            system(("ip netns exec nonsense:" + uplink_name + " ip addr del " + downlink_address + " dev nd-"
                    + name)
                       .c_str());
        });

        auto * uplink = &component["uplink"];
        while (!uplink->is_null() && (*uplink)["role"] == "switch")
        {
            auto & uplink_name = get(*uplink, ":uplink-name");
            auto & uplink_net = get(*uplink, "address");
            auto uplink_uplink_address = nth_address_in_subnet(uplink_net, 2, false);

            annotated_system(
                "ip netns exec nonsense:" + uplink_name + " ip route add " + net + " via "
                + uplink_uplink_address);
            clean.add(
                [=] { system(("ip netns exec nonsense:" + uplink_name + " ip route del " + net).c_str()); });

            uplink = &((*uplink)["uplink"]);
        }

        annotated_system("ip netns exec nonsense:" + uplink_name + " ip link set nd-" + name + " up");
        clean.add([=] {
            system(("ip netns exec nonsense:" + uplink_name + " ip link set nd-" + name + " down").c_str());
        });

        auto gateway = nth_address_in_subnet(net, 1, false);
        auto assigned_address = nth_address_in_subnet(net, 2, true);

        annotated_system("ip addr add " + assigned_address + " dev nb-" + name);
        clean.add([=] { system(("ip addr del " + assigned_address + " dev nb-" + name).c_str()); });
        annotated_system("ip route add default via " + gateway);
        clean.add([=] { system("ip route del default"); });
    }

    else
    {
        annotated_system(
            "ip netns exec nonsense:" + uplink_name + " ip link set nd-" + name + " master nb-"
            + uplink_name);

        // auto & assigned_address = get(component, ":assigned-address");

        auto & uplink_net = get(component["uplink"], "address");

        auto gateway = nth_address_in_subnet(uplink_net, 1, false);
        // FIXME: this is wrong, need to "dhcp" in the main process
        auto assigned_address = nth_address_in_subnet(uplink_net, 3, true);

        annotated_system("ip netns exec nonsense:" + uplink_name + " ip link set nd-" + name + " up");
        clean.add([=] {
            system(("ip netns exec nonsense:" + uplink_name + " ip link set nd-" + name + " down").c_str());
        });

        annotated_system("ip addr add " + assigned_address + " dev nu-" + name);
        clean.add([=] { system(("ip addr del " + assigned_address + " dev nu-" + name).c_str()); });
        annotated_system("ip route add default via " + gateway);
        clean.add([=] { system("ip route del default"); });
    }

    connection_cleanups.add(std::move(clean));
}

void add_network(nlohmann::json & component, sd_bus_message * message, sd_bus_error * error)
try
{
    entityd::cleanup clean;

    std::cerr << component << '\n';

    auto role = component["role"].get_ref<std::string &>();
    auto role_enum = nonsensed::known_network_roles.at(role);

    current_components.emplace(nonsensed::component_type::network, component);

    auto & external = component["external"];
    auto & default_ = component["default"];

    if (external.is_boolean() && external == true)
    {
        auto path = [&] {
            auto it = component.find("external_name");
            if (it == component.end())
            {
                return "/var/run/netns/" + name;
            }
            return it->get<std::string>();
        }();

        auto fd = open(("/var/run/netns/" + name).c_str(), 0);
        assert(fd != -1);
        setns(fd, CLONE_NEWNET);
    }
    else if (!default_.is_boolean() || default_ == false)
    {
        assert(unshare(CLONE_NEWNET) == 0);
    }

    auto full_path = "/var/run/netns/nonsense:" + name;

    std::filesystem::create_directories("/var/run/netns");
    umount(full_path.c_str());
    assert(open(full_path.c_str(), O_CREAT) != -1);
    auto ret = mount("/proc/self/ns/net", full_path.c_str(), nullptr, MS_BIND, nullptr);
    if (ret != 0)
    {
        perror("Failed to mount the network namespace under /var/run/netns");
        std::abort();
    }

    clean.add([=] { umount(full_path.c_str()); });

    cleanups.add(std::move(clean));

    switch (role_enum)
    {
        case nonsensed::network_role::root:
            break;

        case nonsensed::network_role::interface:
            assert(0);

        case nonsensed::network_role::router:
            setup_interfaces();
            setup_nft();
            connect();
            break;

        case nonsensed::network_role::switch_:
            setup_interfaces();
            setup_bridge();
            connect();
            break;

        case nonsensed::network_role::client:
            setup_interfaces();
            connect();
            break;
    }
}
catch (...)
{
    current_components.erase(nonsensed::component_type::network);
    assert(!"really need to reply to the message here...");
}

int add_component(sd_bus_message * message, void *, sd_bus_error * error)
{
    static std::unordered_map<std::string_view, nonsensed::component_type> known_components = {
        { "network", nonsensed::component_type::network }
    };

    char * type_str;
    char * config;

    int ret = sd_bus_message_read(message, "ss", &type_str, &config);
    assert(ret >= 0); // FIXME

    auto it = known_components.find(type_str);
    assert(it != known_components.end()); // FIXME
    auto type = it->second;

    if (current_components.contains(type))
    {
        sd_bus_error_set_const(
            error,
            "info.griwes.nonsense.ComponentAlreadyActive",
            "Tried to add an already active component to an entity");
        return sd_bus_reply_method_error(message, error);
    }

    auto component = nlohmann::json::parse(config);

    switch (type)
    {
        case nonsensed::component_type::network:
            add_network(component, message, error);
            break;
    }

    return sd_bus_reply_method_return(message, "b", true);
}

void shutdown()
{
    connection_cleanups.run();
    cleanups.run();
}

int handle_shutdown(sd_bus_message * msg, void *, sd_bus_error *)
{
    shutdown();
    sd_bus_reply_method_return(msg, "");
    std::exit(0);
}

static const sd_bus_vtable entityd_vtable[] = {
    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD("AddComponent", "ss", "b", add_component, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Shutdown", "", "", handle_shutdown, SD_BUS_VTABLE_UNPRIVILEGED),

    SD_BUS_VTABLE_END
};

int main(int argc, char ** argv)
try
{
    name = argv[1];
    std::cerr << name << '\n';

    sd_bus_new(&bus);

    sd_bus_set_fd(bus, STDIN_FILENO, STDIN_FILENO);
    sd_bus_set_server(bus, true, SD_ID128_NULL);

    sd_bus_start(bus);

    int ret;

    ret =
        sd_bus_add_object_vtable(bus, nullptr, "/", "info.griwes.nonsense.Entityd", entityd_vtable, nullptr);
    if (ret < 0)
    {
        throw std::runtime_error(std::string("Failed to install the Entityd interface: ") + strerror(-ret));
    }

    while (true)
    {
        ret = sd_bus_process(bus, nullptr);
        if (ret < 0)
        {
            throw std::runtime_error(std::string("Failed to process bus: ") + strerror(-ret));
        }

        if (ret > 0)
        {
            continue;
        }

        ret = sd_bus_wait(bus, -1);
        if (ret < 0)
        {
            throw std::runtime_error(std::string("Failed to wait on bus: ") + strerror(-ret));
        }
    }
}
catch (std::exception & ex)
{
    std::cerr << nonsensed::error_prefix() << "Fatal error: " << ex.what() << '\n';
    shutdown();
    return 1;
}
