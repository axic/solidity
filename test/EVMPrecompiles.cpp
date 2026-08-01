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
 * Bridge to evmone's reference precompile implementations. This is the only
 * translation unit that includes evmone's headers; see EVMPrecompiles.h for why
 * they are kept isolated from the rest of the test sources.
 */

// This pulls in evmone's own copy of <evmc/evmc.hpp>. It MUST come before the
// wrapper header (which includes the ABI-compatible evmc C API), and this file
// must never include Solidity's vendored <test/evmc/evmc.hpp>.
#include <test/state/precompiles.hpp>

#include <test/EVMPrecompiles.h>

namespace solidity::test::evmprecompiles
{

bool isPrecompile(evmc_revision _rev, evmc_address const& _address) noexcept
{
	// evmone::state::is_precompile takes the evmc C++ address type, which derives
	// from the C evmc_address; copy the C struct into it.
	evmc::address address;
	static_cast<evmc_address&>(address) = _address;
	return evmone::state::is_precompile(_rev, address);
}

evmc_result callPrecompile(evmc_revision _rev, evmc_message const& _message) noexcept
{
	// release_raw() hands the underlying evmc_result (including its output buffer
	// and release function) to the caller.
	return evmone::state::call_precompile(_rev, _message).release_raw();
}

}
