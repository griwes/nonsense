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

// TODO: move the docs below into a proper documentation system.

/**
 * Nonsense configuration d-bus interfaces
 * =======================================
 *
 * The configuration part of Nonsense exposes objects of the following d-bus interfaces in its d-bus tree:
 *
 * info.griwes.nonsense.ConfigManager
 * ==================================
 * Methods:
 *  - Diff
 *  - Commit
 *  - Save
 *
 * info.griwes.nonsense.Config
 * ===========================
 * Methods:
 *  - Serialize
 *  - List
 *  - Lock
 *  - Unlock
 *
 * info.griwes.nonsense.TransactionManager
 * =======================================
 * Methods:
 *  - List :: "" -> "a(to)"
 *    No parameters.
 *    Return values:
 *      * an array of pairs of transaction token and the full object path to a transaction
 *    Semantics: return a list of all currently open transactions that the invoking user can manipulate.
 *  - New :: "" -> "to"
 *    No parameters.
 *    Return values:
 *      * the transaction token as an integer
 *      * the full object path to the newly created transaction
 *    Semantics: creates a new transaction associated with the invoking user.
 *  * Commit :: "t" -> ""
 *    Parameters:
 *      * the transaction token as an integer
 *    No return value.
 *    Semantics: commits the transaction indicated by the transaction token to the running configuration.
 *  - Discard :: "t" -> ""
 *    Parameters:
 *      * the transaction token as an integer
 *    No return values.
 *    Semantics: discards the transaction indicated by the transaction token.
 *  - DiscardAll :: "" -> ""
 *    No parameters.
 *    No return values.
 *    Semantics: discards all of the currently opened transactions that the invoking user can manipulate.
 *
 * info.griwes.nonsense.Transaction
 * ================================
 * Methods:
 *  - Serialize :: "" -> "s"
 *    No parameters.
 *    Return values:
 *      * the string representation of the transaction operations
 *    Semantics: returns a string containing the sequence of nonsensectl commands that can be issued to
 * replicate the state of the transaction it is invoked on.
 *  - Add :: "ysa(ss) -> ""
 *    Parameters:
 *      * the kind of a new entity to add:
 *        1 - namespace
 *        2 - network
 *        3 - interface
 *      * the name of the new entity to add
 *      * an array of parameters of the new entity, in the same format as the 2nd and 3rd parameters of Set.
 *    No return values.
 *    Semantics: adds an operation of adding a new entity to the configuration to the transaction. The new
 * entity's kind is identified by the first argument; the name by the second; and a set of initial parameters
 * by the third.
 *  - Set :: "ys(ss)" -> ""
 *    Parameters:
 *      * the kind of the entity to modify (see docs of Add)
 *      * the name of the entity to modify
 *      * a struct containing:
 *        * the name of the configuration key to change
 *        * the value of the configuration key to change
 *  - Delete :: "ys" -> ""
 *    Parameters:
 *      * the kind of the entity to delete (see docs of Add)
 *      * the name of the entity to delete
 *     No return values.
 *     Semantics: adds an operation of removing an entity from the configuration to the transaction. The
 * entity to delete's kind is identified by the first argument; the name by the second.
 */

/**
 * Nonsense configuration d-bus tree objects
 * =========================================
 *
 * The /info/griwes/nonsense/configuration object is the main interface for controlling changes to the
 * configuration.
 *
 * Under /info/griwes/nonsense/configuration, three sub-objects are exposed:
 *  - /info/griwes/nonsense/configuration/running, the currently applied configuration used by the nonsense
 * controller subsystem.
 *  - /info/griwes/nonsense/configuration/saved, which is the equivalent of the saved file in the location
 * configured by the -c parameter to the daemon's command line (/etc/nonsense/nonsensed.json by default).
 *  - /info/griwes/nonsense/configuration/transactions, which is the collection of currently opened
 * transactions.
 *
 * /info/griwes/nonsense/configuration
 * ===================================
 * Interfaces:
 *  - info.griwes.nonsense.ConfigManager
 *
 * /info/griwes/nonsense/configuration/running
 * ===========================================
 * Interfaces:
 *  - info.griwes.nonsense.Config
 *
 * /info/griwes/nonsense/configuration/saved
 * =========================================
 * Interfaces:
 *  - info.griwes.nonsense.Config
 *
 * /info/griwes/nonsense/configuration/transactions
 * ================================================
 * Interfaces:
 *  - info.griwes.nonsense.TransactionManager
 *
 * /info/griwes/nonsense/configuration/transactions/...
 * ====================================================
 * Interfaces:
 *  - info.griwes.nonsense.Transaction
 */

#include "configuration.h"

#include "log_helpers.h"
#include "service.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace nonsensed
{
static const sd_bus_vtable config_mgr_vtable[] = { SD_BUS_VTABLE_START(0), SD_BUS_VTABLE_END };

configuration::configuration(const options & opts)
    : _saved_config{ opts },
      _running_config{ _saved_config },
      _transaction_manager{ _saved_config, _running_config }
{
}

void configuration::install(service & srv)
{
    const char * dbus_path = "/info/griwes/nonsense/configuration";
    int ret = sd_bus_add_object_vtable(
        srv.bus(), &_bus_slot, dbus_path, "info.griwes.nonsense.ConfigManager", config_mgr_vtable, this);
    if (ret < 0)
    {
        throw std::runtime_error(
            std::string("Failed to install the ConfigManager interface at ") + dbus_path + ": "
            + strerror(-ret));
    }

    _saved_config.install(srv, "/info/griwes/nonsense/configuration/saved");
    _running_config.install(srv, "/info/griwes/nonsense/configuration/running");
    _transaction_manager.install(srv, "/info/griwes/nonsense/configuration/transactions");
}
}
