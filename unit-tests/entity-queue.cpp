/*
 * Copyright © 2021 Michał 'Griwes' Dominiak
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

#include "../daemon/cli.h"
#include "../daemon/config.h"
#include "../daemon/entity.h"

int main(int argc, char ** argv)
{
    auto opts = nonsensed::options(argc, argv);
    auto imm_config = nonsensed::config(opts);
    auto config = imm_config;

    assert(config.add("ent1", {}).error_code == 0);
    assert(config.add("ent2", {}).error_code == 0);

    struct task
    {
        nonsensed::entity entity;
        bool & flag;

        nonsensed::future doit(sd_bus_message *, sd_bus_error *)
        {
            {
                auto token = co_await entity.enqueue();
                flag = true;
            }

            co_await nonsensed::coro::suspend_always();

            co_return nonsensed::unit;
        }
    };

    auto spawn = [](auto && task) { return task.doit(nullptr, nullptr); };

    bool flag1 = false;
    bool flag2 = false;
    bool flag2_2 = false;

    auto task1 = task{ *config.try_get("ent1"), flag1 };
    auto task2 = task{ *config.try_get("ent2"), flag2 };
    auto task2_2 = task{ *config.try_get("ent2"), flag2_2 };

    {
        spawn(task2);
        assert(flag2);

        auto token = task1.entity.enqueue().await_resume();

        {
            auto token = task2.entity.enqueue().await_resume();

            spawn(task1);
            assert(!flag1);

            spawn(task2_2);
            assert(!flag2_2);
        }

        assert(flag2_2);
    }

    assert(flag1);
}
