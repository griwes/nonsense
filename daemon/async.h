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

#include "bus_slot.h"
#include "log_helpers.h"
#include "overloads.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <tuple>
#include <variant>
#include <vector>

// TODO: GCC appears to have a bug and not define the coroutine feature test macros correctly. If it did, the
// __has_includes would turn into `defined(__cpp_impl_coroutine)` for the IS and `defined(__cpp_coroutines)`
// for the TS.
#if __has_include(<coroutine>)
// C++20 coroutines
#include <coroutine>

namespace nonsensed
{
namespace coro = std;
}
#elif __has_include(<experimental/coroutine>)
// Coroutines TS
#include <experimental/coroutine>

namespace nonsensed
{
namespace coro = std::experimental;
}
#else
#error no coroutines support detected!
#endif

#define RETURN_TASK                                                                                          \
    return [=]([[maybe_unused]] coro::coroutine_handle<promise> nonsense_promise_arg) -> future

// This macro should not need to exist.
//
// It does need to exist, however, because the heuristics for language detection in clangd's clang-format are
// DUUUUUUUUMB and decide that if you `#define foo [=, this]`, that SURELY means that you're writing code in
// ObjC, right? RIGHT?!
//
// AAAAAAAAAAAAAAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.
#define NONSENSE_SQUARE [

#define RETURN_MEMBER_TASK                                                                                   \
    return NONSENSE_SQUARE =, this]([[maybe_unused]] coro::coroutine_handle<promise> nonsense_promise_arg)   \
    -> future

namespace nonsensed
{
struct reply_error_t
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
};

inline reply_error_t reply_error_const(const char * name, const char * description)
{
    reply_error_t ret;
    sd_bus_error_set_const(&ret.error, name, description);
    return ret;
}

template<typename... Ts>
reply_error_t reply_error_format(const char * name, const char * format, Ts... ts)
{
    reply_error_t ret;
    sd_bus_error_setf(&ret.error, name, format, ts...);
    return ret;
}

struct reply_status_t
{
    int code;
    sd_bus_error error = SD_BUS_ERROR_NULL;
};

inline reply_status_t reply_status(int code)
{
    return { code };
}

inline reply_status_t reply_status(int code, const sd_bus_error * error)
{
    reply_status_t ret{ code };
    sd_bus_error_copy(&ret.error, error);
    return ret;
}

inline reply_status_t reply_status_const(int code, const char * name, const char * description)
{
    reply_status_t ret{ code };
    sd_bus_error_set_const(&ret.error, name, description);
    return ret;
}

template<typename... Ts>
reply_status_t reply_status_format(int code, const char * name, const char * format, Ts... ts)
{
    reply_status_t ret{ code };
    sd_bus_error_setf(&ret.error, name, format, ts...);
    return ret;
}

struct maybe_reply_status_t
{
    int code;
};

inline maybe_reply_status_t log_and_reply_on_error(int code, const char * message)
{
    if (code < 0)
    {
        std::ostringstream os;
        os << error_prefix() << "Error: " << message << ": " << strerror(-code) << '\n';
        std::cerr << os.str();
    }
    return { code };
}

class promise;

class future
{
public:
    using promise_type = promise;
};

inline constexpr struct unit_t
{
} unit;

template<typename Signature>
struct function;

using subtask = function<future(coro::coroutine_handle<promise>)>;

class promise
{
public:
    template<typename Class>
    promise(const Class &, sd_bus_message * message, sd_bus_error * error)
        : _payload(sd_bus_message_ref(message))
    {
    }

    promise(coro::coroutine_handle<promise> handle) : _payload(std::move(handle))
    {
    }

    template<typename Class>
    promise(const Class &, coro::coroutine_handle<promise> handle) : _payload(std::move(handle))
    {
    }

    ~promise()
    {
        std::visit(
            overload{ [](sd_bus_message * message) { sd_bus_message_unref(message); },
                      [](coro::coroutine_handle<promise> handle) {} },
            _payload);
    }

    coro::suspend_never initial_suspend() noexcept
    {
        return {};
    }

    coro::suspend_never final_suspend() noexcept
    {
        return {};
    }

    // Error replies.
    void return_value(const reply_error_t & error)
    {
        std::visit(
            overload{ [&](sd_bus_message * message) {
                         int result = sd_bus_reply_method_error(message, &error.error);
                         if (result < 0)
                         {
                             std::cerr << "Error while handling an error in " << __PRETTY_FUNCTION__ << ": "
                                       << strerror(-result) << '\n';
                             std::abort();
                         }
                     },
                      [&](coro::coroutine_handle<promise> handle) {
                          handle.promise().return_value(error);
                          handle.destroy();
                      } },
            _payload);
    }

    void return_value(const reply_status_t & status)
    {
        std::visit(
            overload{ [&](sd_bus_message * message) {
                         if (status.code < 0)
                         {
                             int result = sd_bus_reply_method_errno(message, status.code, &status.error);
                             if (result < 0)
                             {
                                 std::cerr << "Error while handling an error in " << __PRETTY_FUNCTION__
                                           << ": " << strerror(-result) << '\n';
                                 std::abort();
                             }
                         }
                     },
                      [&](coro::coroutine_handle<promise> handle) {
                          handle.promise().return_value(status);
                          handle.destroy();
                      } },
            _payload);
    }

    // "void"-returning sub-promises.
    void return_value(unit_t)
    {
        std::visit(
            overload{ [](sd_bus_message * message) {
                         std::cerr << "Fatal error: attempted to return void from a top-level coroutine.\n";
                         std::abort();
                     },
                      [](coro::coroutine_handle<promise> handle) { handle.resume(); } },
            _payload);
    }

    // Maybe-error replies.
    auto yield_value(const maybe_reply_status_t & status)
    {
        struct
        {
            const maybe_reply_status_t & status;
            promise & promise_;

            bool await_ready()
            {
                return false;
            }

            bool await_suspend(coro::coroutine_handle<> handle)
            {
                if (status.code < 0)
                {
                    promise_.return_value(reply_status_t{ status.code });
                    handle.destroy();
                    return true;
                }

                return false;
            }

            void await_resume()
            {
            }
        } ret{ status, *this };

        return ret;
    }

    // Sub-coroutines.
    template<
        typename U,
        typename std::enable_if<std::is_invocable_v<U, coro::coroutine_handle<promise>>>::type...>
    auto await_transform(U u)
    {
        struct
        {
            U u;

            bool await_ready()
            {
                return false;
            }

            void await_suspend(coro::coroutine_handle<promise> handle)
            {
                u(std::move(handle));
            }

            void await_resume()
            {
            }
        } awaitable{ std::move(u) };

        return awaitable;
    }

    // All else.
    template<
        typename U,
        typename std::enable_if<!std::is_invocable_v<U, coro::coroutine_handle<promise>>>::type...>
    auto await_transform(U u)
    {
        return u;
    }

    future get_return_object()
    {
        return future();
    }

    void unhandled_exception()
    {
        try
        {
            throw;
        }
        catch (...)
        {
            std::terminate();
        }
    }

private:
    std::variant<sd_bus_message *, coro::coroutine_handle<promise>> _payload;
};

struct service_description
{
    const char * service;
    const char * dbus_path;
    const char * interface;
};

namespace services
{
    namespace dbus
    {
        inline service_description local = { .service = "org.freedesktop.DBus.Local",
                                             .dbus_path = "/org/freedesktop/DBus/Local",
                                             .interface = "org.freedesktop.DBus.Local" };
    }

    namespace systemd
    {
        inline service_description manager = { .service = "org.freedesktop.systemd1",
                                               .dbus_path = "/org/freedesktop/systemd1",
                                               .interface = "org.freedesktop.systemd1.Manager" };
    }

    inline service_description entityd = { .service = "info.griwes.nonsense",
                                           .dbus_path = "/",
                                           .interface = "info.griwes.nonsense.Entityd" };
}

template<typename... Arguments>
struct signal_description
{
    const service_description & service;
    const char * name;
    const char * argument_string;
};

namespace signals
{
    namespace dbus
    {
        inline signal_description<> connected = { .service = services::dbus::local,
                                                  .name = "Connected",
                                                  .argument_string = "" };

        inline signal_description<> disconnected = { .service = services::dbus::local,
                                                     .name = "Disconnected",
                                                     .argument_string = "" };
    }

    namespace systemd
    {
        inline signal_description<std::uint32_t, const char *, const char *, const char *> job_removed = {
            .service = services::systemd::manager,
            .name = "JobRemoved",
            .argument_string = "uoss"
        };
    }
}

namespace async
{
    template<typename... Ts>
    auto sd_bus_call_method(
        sd_bus * bus,
        const service_description & service,
        const char * method,
        const char * argument_string,
        Ts... ts)
    {
        struct awaitable_t
        {
            sd_bus * bus;
            const service_description & service;
            const char * method;
            const char * argument_string;
            std::tuple<Ts...> arguments;

            sd_bus_message * message = nullptr;
            coro::coroutine_handle<promise> handle;

            bool await_ready()
            {
                return false;
            }

            void await_suspend(coro::coroutine_handle<promise> handle)
            {
                this->handle = std::move(handle);

                std::apply(
                    [&](Ts... ts) {
                        int r = sd_bus_call_method_async(
                            bus,
                            nullptr,
                            service.service,
                            service.dbus_path,
                            service.interface,
                            method,
                            +[](sd_bus_message * message, void * userdata, sd_bus_error * ret_error) {
                                auto & self = *static_cast<awaitable_t *>(userdata);

                                if (sd_bus_message_is_method_error(message, nullptr))
                                {
                                    self.handle.promise().return_value(reply_status(
                                        -sd_bus_message_get_errno(message),
                                        sd_bus_message_get_error(message)));
                                    self.handle.destroy();

                                    return 1;
                                }

                                sd_bus_message_ref(message);
                                self.message = message;
                                self.handle();

                                return 1;
                            },
                            this,
                            argument_string,
                            ts...);

                        if (r < 0)
                        {
                            std::cerr << "Failed to issue a method call: " << strerror(-r) << '\n';
                            std::abort();
                        }
                    },
                    arguments);
            }

            auto await_resume()
            {
                return std::unique_ptr<
                    sd_bus_message,
                    std::integral_constant<decltype(&sd_bus_message_unref), &sd_bus_message_unref>>{
                    message
                };
            }
        } awaitable{ bus, service, method, argument_string, { ts... } };

        return awaitable;
    }

    template<typename... Arguments>
    class signal_subscription
    {
    public:
        signal_subscription(sd_bus * bus, const signal_description<Arguments...> & signal)
            : _bus(bus), _signal(signal)
        {
            assert(
                sd_bus_match_signal(
                    _bus,
                    &_slot,
                    signal.service.service,
                    signal.service.dbus_path,
                    signal.service.interface,
                    signal.name,
                    +[](sd_bus_message * message, void * userdata, sd_bus_error * ret_error) {
                        auto & self = *static_cast<signal_subscription *>(userdata);

                        // The loop below is, addmittedly, written in a very weird way. Let me explain.
                        //
                        // First of all, we're trying to roughly follow the sd_bus handler mechanics,
                        // where returning a positive result means that the message has been handled. In
                        // that case, since the callbacks are one-shot, we want to clear the element out
                        // of the set of callbacks we want to handle, so that we don't try to invoke it
                        // the next time - probably because it was a coroutine continuation and those are
                        // one shot.
                        //
                        // *However*, if this was done naively (like I did initially), i.e. just by
                        // calling erase inside the early return branch, there's... a problem. Since, in
                        // the intended use case, a matched signal would cause *the coroutine that owns
                        // the subscription* to resume, it may also... cause it to end, destroying the
                        // subscription. That's bad, because it'd cause the call to erase to reach into an
                        // already destroyed vector. Oops.
                        //
                        // So, what do we do to achieve the same effect? Well, we first check if the entry
                        // has already been cleared, if so then there's nothing for us to do. Then, we
                        // replace the entry with an empty one, speculatively. It gets restored if the
                        // callback didn't return a positive value, in which case we want to continue
                        // looping (and *hopefully* there isn't a bug where the coroutine is destroyed yet
                        // a non-positive value is provided). If the handler did match, however, we take
                        // advantage of the speculative clear - to effectively clear an entry in a
                        // potentially-already-gone vector, without touching the memory of said vector at
                        // all.
                        //
                        // There's two conditions under which this breaks, one already mentioned:
                        //   1. The callback ends up destroying the current coroutine and therefore this
                        //   subscription, but returns 0 or less. In this case, the swap after the branch
                        //   will reach into deallocated memory. Should be checkable with ASAN.
                        //   2. The callback ends up *inserting* a new callback into the set, but - again
                        //   - returns 0 or less. In this case the effect is the same if the vector
                        //   reallocates its buffer; again, checkable with ASAN.
                        //
                        // Both of those result from an error where someone inserted a callback that does
                        // not behave correctly into the callback queue; if that's you, you should feel
                        // bad. This is not something we should need to check for too hard, because the
                        // callback queue is internal and only populated by `match`.

                        for (auto it = self._callbacks.begin(), end = self._callbacks.end(); it != end; ++it)
                        {
                            if (it->function == nullptr)
                            {
                                continue;
                            }

                            callback_type elem{};
                            std::swap(elem, *it);

                            auto result = elem.function(message, elem.userdata, ret_error);
                            if (result > 0)
                            {
                                return result;
                            }

                            std::swap(elem, *it);
                        }

                        return 0;
                    },
                    this)
                >= 0);
        }

        struct callback_type
        {
            sd_bus_message_handler_t function;
            void * userdata;
        };

        template<std::size_t KeyIndex = std::size_t(-1), typename Key = int>
        auto match(Key key = 0)
        {
            struct awaitable_t
            {
                Key key;
                signal_subscription & subscription;

                sd_bus_message * message = nullptr;
                coro::coroutine_handle<promise> handle;

                bool await_ready()
                {
                    return false;
                }

                void await_suspend(coro::coroutine_handle<promise> handle)
                {
                    this->handle = std::move(handle);

                    subscription._callbacks.push_back(
                        { +[](sd_bus_message * message, void * userdata, sd_bus_error * ret_error) {
                             auto & self = *static_cast<awaitable_t *>(userdata);

                             std::tuple<Arguments...> arguments;
                             std::apply(
                                 [&](Arguments &... args) {
                                     assert(
                                         sd_bus_message_read(
                                             message, self.subscription._signal.argument_string, &args...)
                                         >= 0);
                                 },
                                 arguments);
                             sd_bus_message_rewind(message, true);

                             auto matches = [&] {
                                 if constexpr (KeyIndex == -1)
                                 {
                                     return true;
                                 }
                                 else
                                 {
                                     if (self.key == std::get<KeyIndex>(arguments))
                                     {
                                         return true;
                                     }
                                     else
                                     {
                                         return false;
                                     }
                                 }
                             }();

                             if (matches)
                             {
                                 sd_bus_message_ref(message);
                                 self.message = message;

                                 self.handle();
                                 return 1;
                             }

                             return 0;
                         },
                          this });
                }

                auto await_resume()
                {
                    return std::unique_ptr<
                        sd_bus_message,
                        std::integral_constant<decltype(&sd_bus_message_unref), &sd_bus_message_unref>>{
                        message
                    };
                }
            } awaitable{ key, *this };

            return awaitable;
        }

    private:
        sd_bus * _bus;
        dbus_slot _slot;
        const signal_description<Arguments...> _signal;

        std::vector<callback_type> _callbacks;
    };

    template<typename... Arguments>
    auto sd_bus_subscribe_signal(sd_bus * bus, const signal_description<Arguments...> & signal)
    {
        return signal_subscription<Arguments...>(bus, signal);
    }
}
}
