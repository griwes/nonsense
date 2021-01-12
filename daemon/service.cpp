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

#include "service.h"
#include "configuration.h"

#include <systemd/sd-bus.h>

#include <sys/epoll.h>

#include <stdexcept>
#include <string>

namespace nonsensed
{
service::service(const options & opts, configuration & config_object)
{
    _epoll_fd = epoll_create1(0);
    if (_epoll_fd == -1)
    {
        throw std::runtime_error(std::string("Failed to create an epoll fd: ") + strerror(errno));
    }

    int ret;

    ret = sd_bus_open_system(&_bus);
    if (ret < 0)
    {
        throw std::runtime_error(std::string("Failed to connect to system bus: ") + strerror(-ret));
    }

    ret = sd_bus_request_name(_bus, "info.griwes.nonsense", 0);
    if (ret < 0)
    {
        throw std::runtime_error(std::string("Failed to acquire service name: ") + strerror(-ret));
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * message = nullptr;

    ret = sd_bus_call_method(
        _bus,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "Subscribe",
        &error,
        &message,
        "");
    if (ret < 0)
    {
        throw std::runtime_error(std::string("Failed to subscribe to systemd signals: ") + strerror(-ret));
    }

    sd_bus_message_unref(message);

    register_bus(_bus);

    config_object.install(*this);
}

void service::loop()
{
    // Initialize to the main bus for the call before the first epoll_wait.
    auto event = epoll_event{ .data = epoll_data{ .ptr = _bus } };

    while (true)
    {
        auto bus = static_cast<sd_bus *>(event.data.ptr);

        int ret = sd_bus_process(bus, nullptr);
        if (ret < 0)
        {
            if (bus == _bus)
            {
                throw std::runtime_error(std::string("Failed to process bus: ") + strerror(-ret));
            }
        }

        if (ret > 0)
        {
            continue;
        }

        if (epoll_wait(_epoll_fd, &event, 1, -1) == -1)
        {
            throw std::runtime_error(std::string("Failed to wait on the epoll fd: ") + strerror(errno));
        }
    }
}

void service::register_bus(sd_bus * bus)
{
    auto bus_fd = sd_bus_get_fd(bus);
    if (bus_fd < 0)
    {
        throw std::runtime_error(std::string("Failed to obtain bus fd: ") + strerror(-bus_fd));
    }

    auto event = epoll_event{ .events = EPOLLIN | EPOLLOUT, .data = epoll_data{ .ptr = bus } };

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, bus_fd, &event) == -1)
    {
        throw std::runtime_error(std::string("Failed to add the bus fd to epoll: ") + strerror(errno));
    }
}

void service::unregister_bus(sd_bus * bus)
{
    auto bus_fd = sd_bus_get_fd(bus);
    if (bus_fd < 0)
    {
        throw std::runtime_error(std::string("Failed to obtain bus fd: ") + strerror(-bus_fd));
    }

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, bus_fd, nullptr) == -1)
    {
        throw std::runtime_error(std::string("Failed to remove the bus fd from epoll: ") + strerror(errno));
    }
}
}
