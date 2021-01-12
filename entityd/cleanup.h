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

#pragma once

#include "../daemon/function.h"

#include <vector>

namespace entityd
{
class cleanup
{
public:
    cleanup() = default;

    cleanup(const cleanup &) = delete;
    cleanup(cleanup &&) = default;
    cleanup & operator=(const cleanup &) = delete;
    cleanup & operator=(cleanup &&) = default;

    ~cleanup()
    {
    }

    void run()
    {
        for (auto it = _cleanups.rbegin(); it != _cleanups.rend(); ++it)
        {
            (*it)();
        }

        _cleanups.clear();
    }

    void add(nonsensed::function<void()> fn)
    {
        _cleanups.push_back(std::move(fn));
    }

    void add(cleanup && other)
    {
        std::move(other._cleanups.begin(), other._cleanups.end(), std::back_inserter(_cleanups));
        other._cleanups.clear();
    }

private:
    std::vector<nonsensed::function<void()>> _cleanups;
};
}
