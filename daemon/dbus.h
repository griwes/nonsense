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

#pragma once

#include "async.h"
#include "bus_slot.h"
#include "log_helpers.h"

// member declaration macros

#define DECLARE_METHOD(name)                                                                                 \
    static int method_##name(sd_bus_message *, void *, sd_bus_error *);                                      \
    nonsensed::future method_##name##_impl(sd_bus_message *, sd_bus_error *)

#define DECLARE_PROPERTY_GET(name)                                                                           \
    static int property_##name##_get(                                                                        \
        sd_bus *, const char *, const char *, const char *, sd_bus_message *, void *, sd_bus_error *);       \
    int property_##name##_get_impl(                                                                          \
        sd_bus *, const char *, const char *, const char *, sd_bus_message *, sd_bus_error *);

#define DECLARE_PROPERTY_SET(name)                                                                           \
    static int property_##name##_set(                                                                        \
        sd_bus *, const char *, const char *, const char *, sd_bus_message *, void *, sd_bus_error *);       \
    int property_##name##_set_impl(                                                                          \
        sd_bus *, const char *, const char *, const char *, sd_bus_message *, sd_bus_error *);

// member definition macros

#define DEFINE_METHOD(class, name)                                                                           \
    int class ::method_##name(sd_bus_message * message, void * userdata, sd_bus_error * error)               \
    {                                                                                                        \
        static_cast<class *>(userdata)->method_##name##_impl(message, error);                                \
        return 1;                                                                                            \
    }

#define DEFINE_PROPERTY_GET(class, name)                                                                     \
    int class ::property_##name##_get(                                                                       \
        sd_bus * bus,                                                                                        \
        const char * path,                                                                                   \
        const char * interface,                                                                              \
        const char * property,                                                                               \
        sd_bus_message * reply,                                                                              \
        void * userdata,                                                                                     \
        sd_bus_error * error)                                                                                \
    {                                                                                                        \
        return static_cast<class *>(userdata)->property_##name##_get_impl(                                   \
            bus, path, interface, property, reply, error);                                                   \
    }

#define DEFINE_PROPERTY_SET(class, name)                                                                     \
    int class ::property_##name##_set(                                                                       \
        sd_bus * bus,                                                                                        \
        const char * path,                                                                                   \
        const char * interface,                                                                              \
        const char * property,                                                                               \
        sd_bus_message * value,                                                                              \
        void * userdata,                                                                                     \
        sd_bus_error * error)                                                                                \
    {                                                                                                        \
        return static_cast<class *>(userdata)->property_##name##_set_impl(                                   \
            bus, path, interface, property, value, error);                                                   \
    }

// member signature macros

#define METHOD_SIGNATURE(class, name)                                                                        \
    future class ::method_##name##_impl(sd_bus_message * message, sd_bus_error * error)

#define PROPERTY_GET_SIGNATURE(class, name)                                                                  \
    int class ::property_##name##_get_impl(                                                                  \
        sd_bus * bus,                                                                                        \
        const char * path,                                                                                   \
        const char * interface,                                                                              \
        const char * property,                                                                               \
        sd_bus_message * reply,                                                                              \
        sd_bus_error * error)

#define PROPERTY_SET_SIGNATURE(class, name)                                                                  \
    int class ::property_##name##_set_impl(                                                                  \
        sd_bus * bus,                                                                                        \
        const char * path,                                                                                   \
        const char * interface,                                                                              \
        const char * property,                                                                               \
        sd_bus_message * value,                                                                              \
        sd_bus_error * error)
