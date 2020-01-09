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
    HANDLE_DBUS_ERROR("Failed to issue method call", result, error);

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
    bool valid = false;
    nonsensed::kind_and_name target;
    std::string property;

    if (arguments.size() == 2)
    {
        auto attempt = nonsensed::try_parse_prefixed_name(arguments[0]);
        if (!attempt)
        {
            // TODO: further validation of name validity
            attempt = { nonsensed::entity_kind::namespace_, arguments[0] };
        }

        target = std::move(attempt.value());
        property = arguments[1];
        valid = true;
    }

    else if (arguments.size() == 3)
    {
        if (auto maybe_kind = nonsensed::try_singular_to_kind(arguments[0]))
        {
            target.kind = maybe_kind.value();
            target.name = arguments[1];
            property = arguments[2];
            valid = true;
        }
    }

    if (!valid)
    {
        std::cerr << "Error: unrecognized arguments to get:";
        for (auto && arg : arguments)
        {
            std::cerr << ' ' << arg;
        }
        std::cerr << '\n';
        std::exit(1);
    }

    using nonsensed::entity_kind;

    std::unordered_map<std::string_view, std::unordered_set<entity_kind>> valid_properties = {
        { "downlink-address", { entity_kind::namespace_, entity_kind::network } },
        { "downlink-address-masked", { entity_kind::namespace_, entity_kind::network } },
        { "uplink-address", { entity_kind::namespace_, entity_kind::network } },
        { "uplink-address-masked", { entity_kind::namespace_, entity_kind::network } },
        { "uplink-bridge", { entity_kind::namespace_ } },
        { "uplink-device", { entity_kind::namespace_, entity_kind::network } },
        { "uplink-entity", { entity_kind::namespace_, entity_kind::network } },
    };

    auto property_it = valid_properties.find(property);
    if (property_it == valid_properties.end())
    {
        std::cerr << "Error: unknown property: " << property << '\n';
        std::exit(1);
    }

    if (!property_it->second.count(target.kind))
    {
        std::cerr << "Error: property " << property << " is not valid for the entity kind "
                  << nonsensed::kind_to_singular(target.kind) << '\n';
        std::exit(1);
    }

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
        "yss",
        target.kind,
        target.name.c_str(),
        property.c_str());
    HANDLE_DBUS_ERROR("Failed to issue method call", status, error);

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

std::unordered_map<std::string_view, entity_kind_information> known_entity_kinds = {
    { "namespace",
      { 1, { { "type", { .validator = set_validator({ "netns", "netns-external", "netns-default" }) } } } } },
    { "network", { 2, { { "address", {} }, { "uplink", {} } } } },
    { "interface", {} }
};

void add_handler(const cxxopts::ParseResult & result)
{
    auto arguments = result["command-arguments"].as<std::vector<std::string>>();
    if (arguments.size() < 2)
    {
        std::cerr << "Error: Not enough arguments for command add\n";
        std::exit(1);
    }

    if (arguments.size() % 2 != 0)
    {
        std::cerr << "Error: Invalid (odd) number of arguments for command add\n";
        std::exit(1);
    }

    auto kind_it = known_entity_kinds.find(arguments[0]);
    if (kind_it == known_entity_kinds.end())
    {
        std::cerr << "Error: Invalid entity kind: " << arguments[0] << '\n';
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

    status = sd_bus_message_append(message, "ys", kind_it->second.numeric_id, arguments[1].c_str());
    HANDLE_DBUS_RESULT("Failed to append to a message", status);

    status = sd_bus_message_open_container(message, SD_BUS_TYPE_ARRAY, "(ss)");
    HANDLE_DBUS_RESULT("Failed to open a container", status);

    auto & known_params = kind_it->second.known_parameters;
    for (std::size_t i = 2; i < arguments.size(); i += 2)
    {
        std::string_view parameter = arguments[i];
        std::string_view value = arguments[i + 1];

        auto param_info_it = known_params.find(parameter);
        if (param_info_it == known_params.end())
        {
            std::cerr << "Error: Parameter '" << parameter << "' is not a valid parameter for entity kind '"
                      << arguments[0] << "'\n";
            std::exit(1);
        }

        auto validity = param_info_it->second.validator(value);
        if (!validity.is_valid)
        {
            std::cerr << "Error: The value '" << value << "' is not a valid value for parameter '"
                      << parameter << "' of entity kind '" << arguments[0] << "'; expected "
                      << validity.expected_message << '\n';
            std::exit(1);
        }

        status = sd_bus_message_append(message, "(ss)", parameter.data(), value.data());
        HANDLE_DBUS_RESULT("Failed to append to a container", status);
    }

    status = sd_bus_message_close_container(message);
    HANDLE_DBUS_RESULT("Failed to close a container", status);

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message * reply = nullptr;
    status = sd_bus_call(dbus, message, 0, &error, &reply);
    HANDLE_DBUS_ERROR("Failed to issue method call", status, error);

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
    HANDLE_DBUS_ERROR("Failed to issue method call", status, error);

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
    nonsensed::kind_and_name target = {};
    bool valid = false;
    const char * name = Mode == locking::lock ? "lock" : "unlock";

    auto arguments = result["command-arguments"].as<std::vector<std::string>>();

    if (arguments.size() == 1)
    {
        target = nonsensed::parse_prefixed_name(arguments[0]);
        valid = true;
    }

    else if (arguments.size() == 2)
    {
        if (auto maybe_kind = nonsensed::try_singular_to_kind(arguments[0]))
        {
            target.kind = maybe_kind.value();
            target.name = arguments[1];
            valid = true;
        }
    }

    if (!valid)
    {
        std::cerr << "Error: unrecognized arguments to " << name << ":";
        for (auto && arg : arguments)
        {
            std::cerr << ' ' << arg;
        }
        std::cerr << '\n';
        std::exit(1);
    }

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
        "ys",
        target.kind,
        target.name.c_str());
    HANDLE_DBUS_ERROR("Failed to issue method call", status, error);

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
    { "unlock", { locking_handler<locking::unlock> } }
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
        help_handler(result);
        return 1;
    }

    handler_it->second.handler(result);
}
catch (std::exception & ex)
{
    std::cerr << "Fatal error: " << ex.what() << '\n';
    return 1;
}
