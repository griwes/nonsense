/*
 * Copyright © 2020 Michał 'Griwes' Dominiak
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

#include <memory>

namespace nonsensed
{
template<typename Signature>
struct function;

template<typename Ret, typename... Args>
struct function<Ret(Args...)>
{
public:
    template<typename F>
    function(F f)
        : _data(
            new F(std::move(f)),
            +[](void * ptr) { delete reinterpret_cast<F *>(ptr); }),

          _call_operator(+[](void * ptr, Args... args) {
              return (*reinterpret_cast<F *>(ptr))(std::forward<Args>(args)...);
          })
    {
    }

    Ret operator()(Args... args)
    {
        return _call_operator(_data.get(), std::forward<Args>(args)...);
    }

private:
    using _call_operator_t = Ret (*)(void *, Args...);

    std::unique_ptr<void, void (*)(void *)> _data;
    _call_operator_t _call_operator = nullptr;
};
}
