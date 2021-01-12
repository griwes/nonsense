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

#include "entity.h"

#include "config.h"
#include "service.h"

#include <nonsense-paths.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <systemd/sd-id128.h>
#include <unistd.h>

#include <list>
#include <thread>

namespace nonsensed
{
std::unordered_map<std::string, entity::_entity_state> entity::_live_entities;

entity::entity(config & config_object, nlohmann::json & self, std::string_view name)
    : _config(config_object), _self(self), _name(name)
{
}

auto & _get_queue_by_name(const std::string & name)
{
    struct queue_state
    {
        bool held = false;
        std::list<coro::coroutine_handle<promise>> queue;
    };
    static std::unordered_map<std::string, queue_state> queues;
    return queues[name];
}

entity::queue_awaitable::queue_token::queue_token(std::string name) : _name(std::move(name))
{
}

entity::queue_awaitable::queue_token::~queue_token()
{
    auto & state = _get_queue_by_name(_name);

    assert(state.held);
    state.held = false;

    if (!state.queue.empty())
    {
        auto handle = std::move(state.queue.front());
        state.queue.pop_front();
        handle();
    }
}

entity::queue_awaitable::queue_awaitable(std::string name) : _name(std::move(name))
{
}

bool entity::queue_awaitable::await_ready()
{
    return !_get_queue_by_name(_name).held;
}

void entity::queue_awaitable::await_suspend(coro::coroutine_handle<promise> handle)
{
    auto & state = _get_queue_by_name(_name);

    assert(state.held);
    state.queue.push_back(std::move(handle));
}

entity::queue_awaitable::queue_token entity::queue_awaitable::await_resume()
{
    auto & state = _get_queue_by_name(_name);

    assert(!state.held);
    state.held = true;

    return { _name };
}

entity::queue_awaitable entity::enqueue()
{
    return { _name };
}

subtask entity::start()
{
    RETURN_MEMBER_TASK
    {
        auto token = co_await enqueue();

        if (_live_entities.count(_name))
        {
            co_return unit;
        }

        auto it = _self.find("network");
        if (it != _self.end())
        {
            auto uplink_it = it->find("uplink");
            if (uplink_it != it->end())
            {
                auto uplink = _config.try_get(uplink_it->get<std::string>());
                assert(uplink);
                co_await uplink->start();
            }
        }

        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1)
        {
            co_return reply_status(errno);
        }

        auto pid = fork();
        if (pid == -1)
        {
            co_return reply_status(errno);
        }

        if (pid == 0)
        {
            if (dup2(sv[1], STDIN_FILENO) == -1)
            {
                perror("Call to dup2 failed");
                std::abort();
            }
            close(sv[0]);
            close(sv[1]);

            auto filename = (install_prefix / "bin" / "nonsense-entityd").string();

            char * arguments[] = { filename.data(), _name.data(), NULL };

            if (execv(filename.c_str(), arguments) == -1)
            {
                perror("Failed to exec into nonsense-entityd");
                std::abort();
            }

            assert(!"exec went very wrong, you are not supposed to reach this point");
        }

        close(sv[1]);

        sd_bus * raw_bus;
        co_yield log_and_reply_on_error(sd_bus_new(&raw_bus), "Failed to allocate an sd_bus");

        _entity_state state = { .pid = pid, .bus = _entity_state::bus_ptr(sd_bus_ref(raw_bus)) };
        _live_entities.emplace(_name, std::move(state));

        co_yield log_and_reply_on_error(sd_bus_set_fd(raw_bus, sv[0], sv[0]), "Failed to set bus fd");
        co_yield log_and_reply_on_error(
            sd_bus_set_bus_client(raw_bus, false), "Failed to disable client bus mode");

        _config.get_service().register_bus(raw_bus);

        co_yield log_and_reply_on_error(sd_bus_start(raw_bus), "Failed to start bus");

        while (sd_bus_is_open(raw_bus) && !sd_bus_is_ready(raw_bus))
        {
            co_yield log_and_reply_on_error(sd_bus_process(raw_bus, nullptr), "Failed to process client bus");
        }

        if (sd_bus_is_ready(raw_bus) <= 0)
        {
            assert(!"failed to connect to entity dbus server, TODO: handle this more gracefully");
        }

        auto dashed_name = _name;
        for (auto && c : dashed_name)
        {
            if (c == '.')
            {
                c = '-';
            }
        }

        auto slice_name = "nonsense-" + dashed_name + ".slice";

        auto subscription =
            async::sd_bus_subscribe_signal(_config._srv->bus(), signals::systemd::job_removed);

        auto reply = co_await async::sd_bus_call_method(
            _config.get_service().bus(),
            services::systemd::manager,
            "StartTransientUnit",
            "ssa(sv)a(sa(sv))",
            slice_name.c_str(),
            "fail",
            1,
            "Description",
            "s",
            ("Slice for nonsense namespace engine entity " + _name).c_str(),
            0);

        const char * job;
        co_yield log_and_reply_on_error(
            sd_bus_message_read(reply.get(), "o", &job), "Failed to parse systemd response");

        auto result = co_await subscription.match<1>(std::string_view(job));

        std::uint32_t id;
        const char * job_;
        const char * unit_name;
        const char * result_string;
        co_yield log_and_reply_on_error(
            sd_bus_message_read(result.get(), "uoss", &id, &job_, &unit_name, &result_string),
            "Failed to parse systemd signal");

        if (std::string_view(result_string) != "done")
        {
            co_return reply_error_format(
                "info.griwes.nonsense.FailedToStart",
                "Failed to start unit %s: job returned result '%s'.",
                slice_name.c_str(),
                result_string);
        }

        auto scope_name = "nonsense-" + _name + "-entityd.scope";

        reply = co_await async::sd_bus_call_method(
            _config.get_service().bus(),
            services::systemd::manager,
            "StartTransientUnit",
            "ssa(sv)a(sa(sv))",
            scope_name.c_str(),
            "fail",
            3,
            "Description",
            "s",
            ("Scope for nonsense namespace engine entity daemon for " + _name).c_str(),
            "Slice",
            "s",
            slice_name.c_str(),
            "PIDs",
            "au",
            1,
            pid,
            0);

        co_yield log_and_reply_on_error(
            sd_bus_message_read(reply.get(), "o", &job), "Failed to parse systemd response");

        result = co_await subscription.match<1>(std::string_view(job));

        co_yield log_and_reply_on_error(
            sd_bus_message_read(result.get(), "uoss", &id, &job_, &unit_name, &result_string),
            "Failed to parse systemd signal");

        if (std::string_view(result_string) != "done")
        {
            co_return reply_error_format(
                "info.griwes.nonsense.FailedToStart",
                "Failed to start unit %s: job returned result '%s'.",
                scope_name.c_str(),
                result_string);
        }

        for (auto elements : _self.items())
        {
            auto type = elements.key();
            auto component = elements.value();

            if (type == "network")
            {
                auto deep_uplink_info = [&](auto && component, auto && self) {
                    auto it = component.find("uplink");
                    if (it == component.end())
                    {
                        return;
                    }

                    auto uplink_ent = _config.try_get(it->template get_ref<std::string &>());
                    assert(uplink_ent);
                    auto uplink = uplink_ent->_self["network"];
                    assert(!uplink.is_null());

                    self(uplink, self);

                    component[":uplink-name"] = std::move(component["uplink"]);
                    component["uplink"] = std::move(uplink);
                };

                deep_uplink_info(component, deep_uplink_info);
            }

            auto reply = co_await async::sd_bus_call_method(
                raw_bus, services::entityd, "AddComponent", "ss", type.c_str(), component.dump().c_str());

            bool result;
            co_yield log_and_reply_on_error(
                sd_bus_message_read(reply.get(), "b", &result),
                "Failed to parse entityd response to AddComponent");

            assert(result); // FIXME better handling
        }

        co_return unit;
    };
}

subtask entity::stop()
{
    RETURN_MEMBER_TASK
    {
        auto token = co_await enqueue();

        auto it = _live_entities.find(_name);
        if (it == _live_entities.end())
        {
            co_return reply_error_format(
                "info.griwes.nonsense.EntityNotStarted",
                "Failed to stop entity %s: entity is not running.",
                _name.c_str());
        }

        auto raw_bus = it->second.bus.get();

        auto reply = co_await async::sd_bus_call_method(raw_bus, services::entityd, "Shutdown", "");

        int status;
        waitpid(it->second.pid, &status, 0);

        _config.get_service().unregister_bus(raw_bus);

        _live_entities.erase(it);

        auto dashed_name = _name;
        for (auto && c : dashed_name)
        {
            if (c == '.')
            {
                c = '-';
            }
        }

        auto slice_name = "nonsense-" + dashed_name + ".slice";

        auto subscription =
            async::sd_bus_subscribe_signal(_config._srv->bus(), signals::systemd::job_removed);

        reply = co_await async::sd_bus_call_method(
            _config._srv->bus(), services::systemd::manager, "StopUnit", "ss", slice_name.c_str(), "replace");

        const char * job;
        co_yield log_and_reply_on_error(
            sd_bus_message_read(reply.get(), "o", &job), "Failed to parse systemd response");

        auto result = co_await subscription.match<2>(std::string_view(slice_name));

        std::uint32_t id;
        const char * job_;
        const char * unit_name;
        const char * result_string;
        co_yield log_and_reply_on_error(
            sd_bus_message_read(result.get(), "uoss", &id, &job_, &unit_name, &result_string),
            "Failed to parse systemd signal");

        if (std::string_view(result_string) != "done")
        {
            co_return reply_error_format(
                "info.griwes.nonsense.FailedToStop",
                "Failed to stop unit %s: job returned result '%s'.",
                slice_name.c_str(),
                result_string);
        }

        co_return unit;
    };
}
}
