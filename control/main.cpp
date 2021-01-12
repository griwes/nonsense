/*
 * Copyright © 2019-2020 Michał 'Griwes' Dominiak
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

#include <cxxopts.hpp>
#include <systemd/sd-bus.h>

#include <cassert>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "../daemon/common_definitions.h"

#define HANDLE_DBUS_ERROR(msg, result, error)                                                                \
    if (result < 0)                                                                                          \
    {                                                                                                        \
        std::cerr << "Error: " msg ": " << error.name << ": " << error.message << '\n';                      \
        std::exit(1);                                                                                        \
    }

#define HANDLE_DBUS_RESULT(msg, result)                                                                      \
    if (result < 0)                                                                                          \
    {                                                                                                        \
        std::cerr << "Error: " msg ": " << strerror(-result) << '\n';                                        \
        std::exit(1);                                                                                        \
    }

sd_bus * dbus = nullptr;
const char * dbus_service = "info.griwes.nonsense";
std::string dbus_path_prefix = "/info/griwes/nonsense";

void dbus_connect()
{
    int result = sd_bus_open_system(&dbus);
    HANDLE_DBUS_RESULT("Failed to connect to system bus", result);
}

cxxopts::Options opts{ "nonsensectl", "Controller binary for nonsense, the namespace engine." };

using verb_handler = void (*)(const cxxopts::ParseResult &);
struct verb_information
{
    verb_handler handler;
    std::string_view help_string = "";
};
extern std::unordered_map<std::string_view, verb_information> recognized_verbs;

void help_handler(const cxxopts::ParseResult & result)
{
    auto arguments = result["command-arguments"].as<std::vector<std::string>>();
    if (arguments.size() == 0)
    {
        std::cout << opts.help() << "\n";
        std::cout << "TODO: supported command verbs\n";
    }
    else if (arguments.size() == 1)
    {
        if (recognized_verbs.count(arguments.front()))
        {
            std::cout << "TODO: help for command verb " << arguments.front() << '\n';
        }
        else
        {
            std::cerr << "Error: Unknown command verb " << arguments.front() << '\n';
            std::exit(1);
        }
    }
    else
    {
        std::cerr << "Error: Invalid arguments to the command verb 'help':";
        for (auto && arg : arguments)
        {
            std::cerr << ' ' << arg;
        }
        std::cerr << '\n';
        std::exit(1);
    }
}

void version_handler(const cxxopts::ParseResult &)
{
    assert(0);
}

void get_transaction_token(const std::vector<std::string> & arguments)
{
    if (arguments.size() != 1)
    {
        std::cerr << "Error: unrecognized arguments to get-transaction-token:";
        for (auto it = arguments.begin() + 1; it != arguments.end(); ++it)
        {
            std::cerr << ' ' << *it;
        }
        std::cerr << '\n';
        std::exit(1);
    }

    dbus_connect();

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * message = nullptr;

    int result = sd_bus_call_method(
        dbus,
        dbus_service,
        (dbus_path_prefix + "/configuration/transactions").c_str(),
        "info.griwes.nonsense.TransactionManager",
        "New",
        &error,
        &message,
        "");
    HANDLE_DBUS_ERROR("Method call failed", result, error);

    std::uint64_t token;
    const char * transaction_path;

    result = sd_bus_message_read(message, "to", &token, &transaction_path);
    HANDLE_DBUS_RESULT("Failed to parse response message", result);

    std::ostringstream os;
    os.fill('0');
    os << std::setw(16) << std::hex << token;
    std::cout << os.str() << '\n';
}

void get_property(const std::vector<std::string> & arguments)
{
    auto & name = arguments.front();
    std::string property = arguments[arguments.size() - 1];

    dbus_connect();

    int status;
    sd_bus_message * message = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    status = sd_bus_call_method(
        dbus,
        dbus_service,
        (dbus_path_prefix + "/configuration/running").c_str(),
        "info.griwes.nonsense.MutableConfig",
        "Get",
        &error,
        &message,
        "ss",
        name.c_str(),
        property.c_str());
    HANDLE_DBUS_ERROR("Method call failed", status, error);

    const char * response;
    status = sd_bus_message_read(message, "s", &response);
    HANDLE_DBUS_RESULT("Failed to parse response message", status);

    std::cout << response << '\n';
}

void get_handler(const cxxopts::ParseResult & result)
{
    auto arguments = result["command-arguments"].as<std::vector<std::string>>();
    if (arguments.size() == 0)
    {
        std::cerr << "Error: Insufficient number of arguments for command get\n";
        std::exit(1);
    }

    if (arguments.front() == "new-transaction-token")
    {
        get_transaction_token(arguments);
    }

    else
    {
        get_property(arguments);
    }
}

struct parameter_validity
{
    bool is_valid = true;
    std::string_view expected_message;
};

using parameter_validator = std::function<parameter_validity(std::string_view)>;

struct parameter_information
{
    parameter_validator validator = +[](std::string_view) { return parameter_validity{ true, "" }; };
};

struct entity_kind_information
{
    std::uint8_t numeric_id;
    std::unordered_map<std::string_view, parameter_information> known_parameters;
};

parameter_validator set_validator(std::vector<std::string_view> allowed_values)
{
    std::unordered_set<std::string_view> set{ allowed_values.begin(), allowed_values.end() };
    std::string expected_message = "'" + std::string(allowed_values.front()) + "'";
    if (allowed_values.size() > 1)
    {
        for (auto it = allowed_values.begin() + 1; it != allowed_values.end() - 1; ++it)
        {
            expected_message += ", '" + std::string(*it) + "'";
        }
        expected_message += ", or '" + std::string(allowed_values.back()) + "'";
    }

    return [set = std::move(set),
            message = std::move(expected_message)](std::string_view value) -> parameter_validity {
        if (set.count(value))
        {
            return { true, "" };
        }
        return { false, message };
    };
}

void add_handler(const cxxopts::ParseResult & result)
{
    auto arguments = result["command-arguments"].as<std::vector<std::string>>();
    if (arguments.size() < 2)
    {
        std::cerr << "Error: Not enough arguments for command add\n";
        std::exit(1);
    }

    auto dbus_path = dbus_path_prefix + "/configuration/running";
    const char * dbus_interface = "info.griwes.nonsense.Config";

    if (result.count("token"))
    {
        dbus_path = dbus_path_prefix + "/configuration/transactions/" + result["token"].as<std::string>();
        dbus_interface = "info.griwes.nonsense.Transaction";
    }

    int status;

    dbus_connect();

    sd_bus_message * message = nullptr;
    status = sd_bus_message_new_method_call(
        dbus, &message, dbus_service, dbus_path.c_str(), dbus_interface, "Add");
    HANDLE_DBUS_RESULT("Failed to create a dbus method call message", status);

    status = sd_bus_message_append(message, "s", arguments[0].c_str());
    HANDLE_DBUS_RESULT("Failed to append to a message", status);

    status = sd_bus_message_open_container(message, SD_BUS_TYPE_ARRAY, "(ss)");
    HANDLE_DBUS_RESULT("Failed to open a container", status);

    for (std::size_t i = 1; i < arguments.size(); ++i)
    {
        auto & argument = arguments[i];
        auto eq_pos = argument.find('=');
        if (eq_pos == std::string::npos)
        {
            std::cerr << "Invalid argument '" << argument << "', must be in form 'parameter-name=value'.\n";
            std::exit(1);
        }

        auto parameter = argument.substr(0, eq_pos);
        auto value = argument.substr(eq_pos + 1);

        status = sd_bus_message_append(message, "(ss)", parameter.data(), value.data());
        HANDLE_DBUS_RESULT("Failed to append to a container", status);
    }

    status = sd_bus_message_close_container(message);
    HANDLE_DBUS_RESULT("Failed to close a container", status);

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * reply = nullptr;
    status = sd_bus_call(dbus, message, 0, &error, &reply);
    HANDLE_DBUS_ERROR("Method call failed", status, error);

    status = sd_bus_message_read(message, "");
    HANDLE_DBUS_RESULT("Failed to parse response message", status);
}

void commit_handler(const cxxopts::ParseResult &)
{
    assert(0);
}

enum class finalize
{
    commit,
    discard
};

template<finalize Mode>
void finalize_handler(const cxxopts::ParseResult & result)
{
    const char * name = Mode == finalize::commit ? "commit" : "discard";

    auto arguments = result["command-arguments"].as<std::vector<std::string>>();
    if (arguments.size() != 0)
    {
        std::cerr << "Error: unrecognized arguments to " << name << ":";
        for (auto && arg : arguments)
        {
            std::cerr << ' ' << arg;
        }
        std::cerr << '\n';
        std::exit(1);
    }

    if (!result.count("token"))
    {
        std::cerr << "Error: a transaction token must be provided for " << name << ".\n";
        std::exit(1);
    }

    std::istringstream is(result["token"].as<std::string>());
    std::uint64_t token;
    is >> std::hex >> token;

    if (!is)
    {
        std::cerr << "Error: Invalid token format: " << result["token"].as<std::string>() << '\n';
        std::exit(1);
    }

    dbus_connect();

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * message = nullptr;

    int status = sd_bus_call_method(
        dbus,
        dbus_service,
        (dbus_path_prefix + "/configuration/transactions").c_str(),
        "info.griwes.nonsense.TransactionManager",
        Mode == finalize::commit ? "Commit" : "Discard",
        &error,
        &message,
        "t",
        token);
    HANDLE_DBUS_ERROR("Method call failed", status, error);

    status = sd_bus_message_read(message, "");
    HANDLE_DBUS_RESULT("Failed to parse response message", status);
}

enum class locking
{
    lock,
    unlock
};

template<locking Mode>
void locking_handler(const cxxopts::ParseResult & result)
{
    auto arguments = result["command-arguments"].as<std::vector<std::string>>();

    auto & name = arguments.front();

    dbus_connect();

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * message = nullptr;

    int status = sd_bus_call_method(
        dbus,
        dbus_service,
        (dbus_path_prefix + "/configuration/running").c_str(),
        "info.griwes.nonsense.MutableConfig",
        Mode == locking::lock ? "Lock" : "Unlock",
        &error,
        &message,
        "s",
        name.c_str());
    HANDLE_DBUS_ERROR("Method call failed", status, error);

    status = sd_bus_message_read(message, "");
    HANDLE_DBUS_RESULT("Failed to parse response message", status);
}

enum class action
{
    start,
    stop,
    restart,
    status
};

template<action Mode>
void action_handler(const cxxopts::ParseResult & result)
{
    const char * dbus_method;
    switch (Mode)
    {
        case action::start:
            dbus_method = "Start";
            break;
        case action::stop:
            dbus_method = "Stop";
            break;
        case action::restart:
            dbus_method = "Restart";
            break;
        case action::status:
            dbus_method = "Status";
            break;
        default:
            __builtin_unreachable();
    }

    auto arguments = result["command-arguments"].as<std::vector<std::string>>();

    auto & name = arguments.front();

    dbus_connect();

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * message = nullptr;

    int status = sd_bus_call_method(
        dbus,
        dbus_service,
        (dbus_path_prefix + "").c_str(),
        "info.griwes.nonsense.Controller",
        dbus_method,
        &error,
        &message,
        "s",
        name.c_str());
    HANDLE_DBUS_ERROR("Method call failed", status, error);

    status = sd_bus_message_read(message, "");
    HANDLE_DBUS_RESULT("Failed to parse response message", status);
}

std::unordered_map<std::string_view, verb_information> recognized_verbs = {
    { "help", { help_handler } },
    { "version", { version_handler } },

    { "get", { get_handler } },
    { "add", { add_handler } },

    { "commit", { finalize_handler<finalize::commit> } },
    { "discard", { finalize_handler<finalize::discard> } },

    { "lock", { locking_handler<locking::lock> } },
    { "unlock", { locking_handler<locking::unlock> } },

    { "start", { action_handler<action::start> } },
    { "stop", { action_handler<action::stop> } }
};

int main(int argc, char ** argv)
try
{
    // clang-format off
    opts.add_options()
        ("t,token", "Set the transaction token for this operation. If not present, the operation is applied "
            "immediately. Only relevant for the add, set, and delete verbs.", cxxopts::value<std::string>(),
            "options");

    opts.add_options()
        ("verb", "The command to execute.", cxxopts::value<std::string>(), "verbs")
        ("command-arguments", "The arguments to the requested command.",
            cxxopts::value<std::vector<std::string>>()->default_value({}), "verbs");
    // clang-format on

    opts.positional_help("<command verb> [<command parameters>...]");
    opts.parse_positional({ "verb", "command-arguments" });

    auto result = opts.parse(argc, argv);

    if (argc != 1)
    {
        std::cerr << "Error: Unrecognized options:\n";

        for (int i = 1; i < argc; ++i)
        {
            std::cerr << "    " << argv[i] << '\n';
        }

        return 1;
    }

    if (result.count("verb") != 1)
    {
        std::cerr << "Error: No command verb specified.\n";
        help_handler(result);
        return 1;
    }

    auto verb = result["verb"].as<std::string>();
    auto handler_it = recognized_verbs.find(verb);
    if (handler_it == recognized_verbs.end())
    {
        std::cerr << "Error: Unknown command verb specified: " << verb << ".\n";
        return 1;
    }

    handler_it->second.handler(result);
}
catch (std::exception & ex)
{
    std::cerr << "Fatal error: " << ex.what() << '\n';
    return 1;
}
