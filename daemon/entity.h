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

#pragma once

#include "async.h"
#include "function.h"

#include <json.hpp>

#include <string_view>

namespace nonsensed
{
class config;

class entity
{
public:
    subtask start();
    subtask stop();

    class queue_awaitable
    {
    public:
// chromatica.vim hack:
#define NODISCARD [[nodiscard]]

        class NODISCARD queue_token
        {
        public:
            ~queue_token();

            queue_token(const queue_token &) = delete;
            queue_token & operator=(const queue_token &) = delete;

        private:
            queue_token(std::string);
            friend class queue_awaitable;

            std::string _name;
        };

#undef NODISCARD

        bool await_ready();
        void await_suspend(coro::coroutine_handle<promise>);
        queue_token await_resume();

    private:
        queue_awaitable(std::string);
        friend class entity;

        std::string _name;
    };

    queue_awaitable enqueue();

private:
    friend class config;

    entity(config & config_object, nlohmann::json & self, std::string_view name);

    config & _config;
    nlohmann::json & _self;

    std::string _name;

    // Since entity objects are created ad-hoc from a json reference, we need a place to persist things that
    // can't be put into the json tree itself.
    struct _entity_state
    {
        int pid;

        using bus_ptr =
            std::unique_ptr<sd_bus, std::integral_constant<sd_bus * (*)(sd_bus *), &sd_bus_unref>>;
        bus_ptr bus;
    };

    static std::unordered_map<std::string, _entity_state> _live_entities;
};
}
