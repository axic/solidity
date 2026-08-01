/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * Thin wrapper around evmone's reference precompile implementations
 * (evmone::state::call_precompile / is_precompile), used by EVMHost.
 *
 * evmone ships its own copy of the evmc C++ headers, whose `namespace evmc`
 * definitions would clash (both use `#pragma once`, so the two physical copies
 * are not deduplicated) with Solidity's vendored `test/evmc` if included in the
 * same translation unit. To avoid that, the evmone headers are confined to
 * EVMPrecompiles.cpp and this interface is expressed purely in terms of the
 * evmc *C* API (evmc.h), which is ABI-stable and guarded by a shared EVMC_H
 * macro, so the two copies coexist without redefinition.
 */

#pragma once

#include <test/evmc/evmc.h>

namespace solidity::test::evmprecompiles
{

/// @returns true if @a _address is a precompiled contract at EVM revision @a _rev.
bool isPrecompile(evmc_revision _rev, evmc_address const& _address) noexcept;

/// Executes the precompiled contract addressed by @a _message.code_address using
/// evmone's reference implementations.
/// @pre isPrecompile(_rev, _message.code_address) must be true.
/// @returns the raw result; ownership of any output buffer is transferred to the
///     caller via evmc_result::release (wrap it in an evmc::Result to manage it).
evmc_result callPrecompile(evmc_revision _rev, evmc_message const& _message) noexcept;

}
