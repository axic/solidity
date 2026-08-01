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
 * EVM execution host, i.e. component that implements a simulated Ethereum blockchain
 * for testing purposes.
 */

// Weird issue when compiling with O3 on gcc 12 and later due to usage of vector<uint8_t> (aka bytes) as std::map key
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=98465
// also clang doesn't know stringop-overread
#if defined(__GNUC__) && !defined(__clang__) // GCC-specific pragma
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif

#include <test/EVMHost.h>
#include <test/libsolidity/util/SoltestErrors.h>

#if defined(__GNUC__) && !defined(__clang__) // GCC-specific pragma
#pragma GCC diagnostic pop
#endif

#include <test/evmc/loader.h>

#include <test/EVMPrecompiles.h>

#include <libevmasm/GasMeter.h>

#include <libsolutil/Exceptions.h>
#include <libsolutil/Assertions.h>
#include <libsolutil/Keccak256.h>

using namespace solidity;
using namespace solidity::util;
using namespace solidity::test;
using namespace evmc::literals;

evmc::VM& EVMHost::getVM(std::string const& _path)
{
	static evmc::VM NullVM{nullptr};
	static std::map<std::string, std::unique_ptr<evmc::VM>> vms;
	if (vms.count(_path) == 0)
	{
		evmc_loader_error_code errorCode = {};
		auto vm = evmc::VM{evmc_load_and_configure(_path.c_str(), &errorCode)};
		if (vm && errorCode == EVMC_LOADER_SUCCESS)
		{
			if (vm.get_capabilities() & (EVMC_CAPABILITY_EVM1))
				vms[_path] = std::make_unique<evmc::VM>(evmc::VM(std::move(vm)));
			else
				std::cerr << "VM loaded does not support EVM1" << std::endl;
		}
		else
		{
			std::cerr << "Error loading VM from " << _path;
			if (char const* errorMsg = evmc_last_error_msg())
				std::cerr << ":" << std::endl << errorMsg;
			std::cerr << std::endl;
		}
	}

	if (vms.count(_path) > 0)
		return *vms[_path];

	return NullVM;
}

bool EVMHost::checkVmPaths(std::vector<boost::filesystem::path> const& _vmPaths)
{
	bool evmVmFound = false;
	for (auto const& path: _vmPaths)
	{
		evmc::VM& vm = EVMHost::getVM(path.string());
		if (!vm)
			continue;

		if (vm.has_capability(EVMC_CAPABILITY_EVM1))
		{
			if (evmVmFound)
				BOOST_THROW_EXCEPTION(std::runtime_error("Multiple evm1 evmc vms defined. Please only define one evm1 evmc vm."));
			evmVmFound = true;
		}
	}
	return evmVmFound;
}

EVMHost::EVMHost(langutil::EVMVersion _evmVersion, evmc::VM& _vm):
	m_vm(_vm),
	m_evmVersion(_evmVersion)
{
	if (!m_vm)
	{
		std::cerr << "Unable to find evmone library" << std::endl;
		solRequire(false, Exception, "");
	}

	if (_evmVersion == langutil::EVMVersion::homestead())
		m_evmRevision = EVMC_HOMESTEAD;
	else if (_evmVersion == langutil::EVMVersion::tangerineWhistle())
		m_evmRevision = EVMC_TANGERINE_WHISTLE;
	else if (_evmVersion == langutil::EVMVersion::spuriousDragon())
		m_evmRevision = EVMC_SPURIOUS_DRAGON;
	else if (_evmVersion == langutil::EVMVersion::byzantium())
		m_evmRevision = EVMC_BYZANTIUM;
	else if (_evmVersion == langutil::EVMVersion::constantinople())
		m_evmRevision = EVMC_CONSTANTINOPLE;
	else if (_evmVersion == langutil::EVMVersion::petersburg())
		m_evmRevision = EVMC_PETERSBURG;
	else if (_evmVersion == langutil::EVMVersion::istanbul())
		m_evmRevision = EVMC_ISTANBUL;
	else if (_evmVersion == langutil::EVMVersion::berlin())
		m_evmRevision = EVMC_BERLIN;
	else if (_evmVersion == langutil::EVMVersion::london())
		m_evmRevision = EVMC_LONDON;
	else if (_evmVersion == langutil::EVMVersion::paris())
		m_evmRevision = EVMC_PARIS;
	else if (_evmVersion == langutil::EVMVersion::shanghai())
		m_evmRevision = EVMC_SHANGHAI;
	else if (_evmVersion == langutil::EVMVersion::cancun())
		m_evmRevision = EVMC_CANCUN;
	else if (_evmVersion == langutil::EVMVersion::prague())
		m_evmRevision = EVMC_PRAGUE;
	else if (_evmVersion == langutil::EVMVersion::osaka())
		m_evmRevision = EVMC_OSAKA;
	else if (_evmVersion == langutil::EVMVersion::amsterdam())
		m_evmRevision = EVMC_AMSTERDAM;
	else if (_evmVersion == langutil::EVMVersion::future())
		m_evmRevision = EVMC_MAX_REVISION;
	else
		solRequire(false, Exception, "Unsupported EVM version");

	if (m_evmRevision >= EVMC_PARIS)
		// This is the value from the merge block.
		tx_context.block_prev_randao = 0xa86c2e601b6c44eb4848f7d23d9df3113fbcac42041c49cbed5000cb4f118777_bytes32;
	else
		tx_context.block_prev_randao = evmc::uint256be{200000000};
	tx_context.block_gas_limit = 20000000;
	tx_context.block_coinbase = 0x7878787878787878787878787878787878787878_address;
	tx_context.tx_gas_price = evmc::uint256be{3000000000};
	tx_context.tx_origin = 0x9292929292929292929292929292929292929292_address;
	// Mainnet according to EIP-155
	tx_context.chain_id = evmc::uint256be{1};
	// The minimum value of basefee
	tx_context.block_base_fee = evmc::bytes32{7};
	// The minimum value of blobbasefee
	tx_context.blob_base_fee = evmc::bytes32{1};

	static evmc_bytes32 const blob_hashes_array[] = {
		0x0100000000000000000000000000000000000000000000000000000000000001_bytes32,
		0x0100000000000000000000000000000000000000000000000000000000000002_bytes32
	};
	tx_context.blob_hashes = blob_hashes_array;
	tx_context.blob_hashes_count = sizeof(blob_hashes_array) / sizeof(blob_hashes_array[0]);

	// Reserve space for recording calls.
	if (!recorded_calls.capacity())
		recorded_calls.reserve(max_recorded_calls);

	reset();
}

void EVMHost::reset()
{
	accounts.clear();
	// Clear self destruct records
	recorded_selfdestructs.clear();
	// Clear call records
	recorded_calls.clear();
	// Clear EIP-2929 account access indicator
	recorded_account_accesses.clear();
	m_newlyCreatedAccounts.clear();
	m_totalCodeDepositGas = 0;

	// Mark all precompiled contracts as existing. Existing here means to have a balance (as per EIP-161).
	// NOTE: keep this in sync with `EVMHost::call` below.
	//
	// A lot of precompile addresses had a balance before they became valid addresses for precompiles.
	// For example all the precompile addresses allocated in Byzantium had a 1 wei balance sent to them
	// roughly 22 days before the update went live.
	for (unsigned precompiledAddress = 1; precompiledAddress <= 8; precompiledAddress++)
	{
		evmc::address address{precompiledAddress};
		// 1wei
		accounts[address].balance = evmc::uint256be{1};
		// Set according to EIP-1052.
		if (precompiledAddress < 5 || m_evmVersion >= langutil::EVMVersion::byzantium())
			accounts[address].codehash = 0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470_bytes32;
	}
}

void EVMHost::newTransactionFrame()
{
	// Clear EIP-2929 account access indicator
	recorded_account_accesses.clear();

	for (auto& [address, account]: accounts)
	{
		for (auto& [slot, value]: account.storage)
		{
			value.access_status = EVMC_ACCESS_COLD; // Clear EIP-2929 storage access indicator
			value.original = value.current;         // Clear EIP-2200 dirty slot
		}

		// Clear transient storage according to EIP 1153
		account.transient_storage.clear();
	}
	// Process selfdestruct list
	for (auto& [address, _]: recorded_selfdestructs)
		if (m_evmVersion < langutil::EVMVersion::cancun() || m_newlyCreatedAccounts.count(address))
			// EIP-6780: If SELFDESTRUCT is executed in a transaction different from the one
			// in which it was created, we do NOT record it or clear any data.
			// Otherwise, the previous behavior (pre-Cancun) is maintained.
			accounts.erase(address);
	m_newlyCreatedAccounts.clear();
	m_totalCodeDepositGas = 0;
	recorded_selfdestructs.clear();
}

void EVMHost::transfer(evmc::MockedAccount& _sender, evmc::MockedAccount& _recipient, u256 const& _value) noexcept
{
	solRequire(u256(convertFromEVMC(_sender.balance)) >= _value, Exception, "Insufficient balance for transfer");
	_sender.balance = convertToEVMC(u256(convertFromEVMC(_sender.balance)) - _value);
	_recipient.balance = convertToEVMC(u256(convertFromEVMC(_recipient.balance)) + _value);
}

bool EVMHost::selfdestruct(const evmc::address& _addr, const evmc::address& _beneficiary) noexcept
{
	// TODO actual selfdestruct is even more complicated.

	// NOTE: EIP-6780: The transfer of the entire account balance to the beneficiary should still happen
	// after cancun.
	transfer(accounts[_addr], accounts[_beneficiary], convertFromEVMC(accounts[_addr].balance));

	// Record self destructs. Clearing will be done in newTransactionFrame().
	return MockedHost::selfdestruct(_addr, _beneficiary);
}

void EVMHost::recordCalls(evmc_message const& _message) noexcept
{
	if (recorded_calls.size() < max_recorded_calls)
		recorded_calls.emplace_back(_message);
}

// NOTE: this is used for both internal and external calls.
// External calls are triggered from ExecutionFramework and contain only EVMC_CREATE or EVMC_CALL.
evmc::Result EVMHost::call(evmc_message const& _message) noexcept
{
	recordCalls(_message);

	// Precompiled contracts are executed through evmone's reference implementations
	// (see test/EVMPrecompiles.{h,cpp}). Unlike the previous hard-coded input/output
	// tables, this handles gas accounting and the full input domain for every precompile
	// that is available at the current EVM revision (including MODEXP and BLAKE2F, which
	// used to be unimplemented here).
	if (evmprecompiles::isPrecompile(m_evmRevision, _message.code_address))
		return evmc::Result{evmprecompiles::callPrecompile(m_evmRevision, _message)};

	auto const stateBackup = accounts;

	u256 value{convertFromEVMC(_message.value)};
	auto& sender = accounts[_message.sender];

	evmc::bytes code;

	evmc_message message = _message;
	if (message.depth == 0)
	{
		message.gas -= message.kind == EVMC_CREATE ? evmasm::GasCosts::txCreateGas : evmasm::GasCosts::txGas;
		for (size_t i = 0; i < message.input_size; ++i)
			message.gas -= message.input_data[i] == 0 ? evmasm::GasCosts::txDataZeroGas : evmasm::GasCosts::txDataNonZeroGas(m_evmVersion);
		if (message.gas < 0)
		{
			evmc::Result result;
			result.status_code = EVMC_OUT_OF_GAS;
			accounts = stateBackup;
			return result;
		}
	}

	if (message.kind == EVMC_CREATE)
	{
		// TODO is the nonce incremented on failure, too?
		// NOTE: nonce for creation from contracts starts at 1
		// TODO: check if sender is an EOA and do not pre-increment
		sender.nonce++;

		auto encodeRlpInteger = [](int value) -> bytes {
			if (value == 0) {
				return bytes{128};
			} else if (value <= 127) {
				return bytes{static_cast<uint8_t>(value)};
			} else if (value <= 0xff) {
				return bytes{128 + 1, static_cast<uint8_t>(value)};
			} else if (value <= 0xffff) {
				return bytes{128 + 55 + 2, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
			} else {
				solUnimplemented("Can only encode RLP numbers <= 0xffff");
			}
		};

		bytes encodedNonce = encodeRlpInteger(sender.nonce);

		h160 createAddress(keccak256(
			bytes{static_cast<uint8_t>(0xc0 + 21 + encodedNonce.size())} +
			bytes{0x94} +
			bytes(std::begin(message.sender.bytes), std::end(message.sender.bytes)) +
			encodedNonce
		), h160::AlignRight);

		message.recipient = convertToEVMC(createAddress);
		soltestAssert(accounts.count(message.recipient) == 0, "Account cannot exist");

		code = evmc::bytes(message.input_data, message.input_data + message.input_size);
	}
	else if (message.kind == EVMC_CREATE2)
	{
		h160 createAddress(keccak256(
			bytes{0xff} +
			bytes(std::begin(message.sender.bytes), std::end(message.sender.bytes)) +
			bytes(std::begin(message.create2_salt.bytes), std::end(message.create2_salt.bytes)) +
			keccak256(bytes(message.input_data, message.input_data + message.input_size)).asBytes()
		), h160::AlignRight);

		message.recipient = convertToEVMC(createAddress);
		if (accounts.count(message.recipient) && (
			accounts[message.recipient].nonce > 0 ||
			!accounts[message.recipient].code.empty()
		))
		{
			evmc::Result result;
			result.status_code = EVMC_OUT_OF_GAS;
			accounts = stateBackup;
			return result;
		}


		code = evmc::bytes(message.input_data, message.input_data + message.input_size);
	}
	else
		code = accounts[message.code_address].code;

	auto& destination = accounts[message.recipient];
	if (message.kind == EVMC_CREATE || message.kind == EVMC_CREATE2)
		// Mark account as created if it is a CREATE or CREATE2 call
		// TODO: Should we roll changes back on failure like we do for `accounts`?
		m_newlyCreatedAccounts.emplace(message.recipient);

	if (value != 0 && message.kind != EVMC_DELEGATECALL && message.kind != EVMC_CALLCODE)
	{
		if (value > convertFromEVMC(sender.balance))
		{
			evmc::Result result;
			result.status_code = EVMC_INSUFFICIENT_BALANCE;
			accounts = stateBackup;
			return result;
		}
		transfer(sender, destination, value);
	}

	// Populate the access list (enabled since Berlin).
	// Note, this will also properly touch the created address.
	// TODO: support a user supplied access list too
	if (m_evmRevision >= EVMC_BERLIN)
	{
		access_account(message.sender);
		access_account(message.recipient);

		// EIP-3651 rule
		if (m_evmRevision >= EVMC_SHANGHAI)
			access_account(tx_context.block_coinbase);
	}

	if (message.kind == EVMC_CREATE || message.kind == EVMC_CREATE2)
	{
		message.input_data = nullptr;
		message.input_size = 0;
	}
	evmc::Result result = m_vm.execute(*this, m_evmRevision, message, code.data(), code.size());

	if (message.kind == EVMC_CREATE || message.kind == EVMC_CREATE2)
	{
		int64_t codeDepositGas = static_cast<int64_t>(evmasm::GasCosts::createDataGas * result.output_size);
		result.gas_left -= codeDepositGas;
		if (result.gas_left < 0)
		{
			m_totalCodeDepositGas += -result.gas_left;
			result.gas_left = 0;
			result.status_code = EVMC_OUT_OF_GAS;
			// TODO clear some fields?
		}
		else
		{
			m_totalCodeDepositGas += codeDepositGas;
			result.create_address = message.recipient;
			destination.code = evmc::bytes(result.output_data, result.output_data + result.output_size);
			destination.codehash = convertToEVMC(keccak256({result.output_data, result.output_size}));
		}
	}

	if (result.status_code != EVMC_SUCCESS)
		accounts = stateBackup;

	return result;
}

evmc::bytes32 EVMHost::get_block_hash(int64_t _number) const noexcept
{
	return convertToEVMC(u256("0x3737373737373737373737373737373737373737373737373737373737373737") + _number);
}

h160 EVMHost::convertFromEVMC(evmc::address const& _addr)
{
	return h160(bytes(std::begin(_addr.bytes), std::end(_addr.bytes)));
}

evmc::address EVMHost::convertToEVMC(h160 const& _addr)
{
	evmc::address a;
	for (unsigned i = 0; i < 20; ++i)
		a.bytes[i] = _addr[i];
	return a;
}

h256 EVMHost::convertFromEVMC(evmc::bytes32 const& _data)
{
	return h256(bytes(std::begin(_data.bytes), std::end(_data.bytes)));
}

evmc::bytes32 EVMHost::convertToEVMC(h256 const& _data)
{
	evmc::bytes32 d;
	for (unsigned i = 0; i < 32; ++i)
		d.bytes[i] = _data[i];
	return d;
}

StorageMap const& EVMHost::get_address_storage(evmc::address const& _addr)
{
	soltestAssert(account_exists(_addr), "Account does not exist.");
	return accounts[_addr].storage;
}

std::string EVMHostPrinter::state()
{
	// Print state and execution trace.
	if (m_host.account_exists(m_account))
	{
		storage();
		balance();
	}
	else
		selfdestructRecords();

	callRecords();
	return m_stateStream.str();
}

void EVMHostPrinter::storage()
{
	for (auto const& [slot, value]: m_host.get_address_storage(m_account))
		if (m_host.get_storage(m_account, slot))
			m_stateStream << "  "
				<< m_host.convertFromEVMC(slot)
				<< ": "
				<< m_host.convertFromEVMC(value.current)
				<< std::endl;
}

void EVMHostPrinter::balance()
{
	m_stateStream << "BALANCE "
		<< m_host.convertFromEVMC(m_host.get_balance(m_account))
		<< std::endl;
}

void EVMHostPrinter::selfdestructRecords()
{
	for (auto const& record: m_host.recorded_selfdestructs)
		for (auto const& beneficiary: record.second)
			m_stateStream << "SELFDESTRUCT"
				<< " BENEFICIARY "
				<< m_host.convertFromEVMC(beneficiary)
				<< std::endl;
}

void EVMHostPrinter::callRecords()
{
	static auto constexpr callKind = [](evmc_call_kind _kind) -> std::string
	{
		switch (_kind)
		{
			case evmc_call_kind::EVMC_CALL:
				return "CALL";
			case evmc_call_kind::EVMC_DELEGATECALL:
				return "DELEGATECALL";
			case evmc_call_kind::EVMC_CALLCODE:
				return "CALLCODE";
			case evmc_call_kind::EVMC_CREATE:
				return "CREATE";
			case evmc_call_kind::EVMC_CREATE2:
				return "CREATE2";
		}
		unreachable();
	};

	for (auto const& record: m_host.recorded_calls)
		m_stateStream << callKind(record.kind)
			<< " VALUE "
			<< m_host.convertFromEVMC(record.value)
			<< std::endl;
}
