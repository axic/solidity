# Fetches evmone from source and exposes its reference precompile
# implementations (the `evmone::state` target, which provides
# `evmone::state::call_precompile` / `evmone::state::is_precompile`) so that they
# can be linked into the test executable and used by `test/EVMHost.cpp`.
#
# The EVM interpreter itself is still loaded dynamically at runtime as a shared
# library through the evmc loader (see `EVMHost::getVM`); this module only
# provides the precompiles.
#
# NOTE: the evmone release artifacts (the `libevmone-*.tar.gz` archives the CI
# installs) ship *only* `libevmone.so` plus a couple of headers - they contain
# neither `evmone::state` nor the lower-level `evmone_precompiles` library (which
# has no install rules). The precompiles therefore have to be built from source
# here. Keep EVMONE_VERSION in sync with the libevmone used at runtime (see
# `scripts/docker/*` and `.circleci/*_install_dependencies.sh`).

include(FetchContent)

set(EVMONE_VERSION v0.22.0)

FetchContent_Declare(
	evmone
	GIT_REPOSITORY https://github.com/ipsilon/evmone.git
	GIT_TAG ${EVMONE_VERSION}
	GIT_SHALLOW ON
	GIT_SUBMODULES_RECURSE ON
)

# `evmone::state` lives behind EVMONE_TOOLS (it is added by
# `add_subdirectory(test/state)`, which is guarded by that option). We do not
# need evmone's own unit-test suite (EVMONE_TESTING) or its fuzzers.
set(EVMONE_TESTING OFF CACHE BOOL "" FORCE)
set(EVMONE_TOOLS ON CACHE BOOL "" FORCE)
set(EVMONE_FUZZING OFF CACHE BOOL "" FORCE)

# Build evmone statically so it links directly into soltest instead of producing
# a second shared object next to the one we load at runtime. Set as a normal
# variable (honoured by option() via CMP0077) so it only affects the evmone
# sub-build configured below, not Solidity's own targets.
set(BUILD_SHARED_LIBS OFF)

# evmone is a separate project with its own warning configuration. Solidity
# enables `-Werror -Wall -Wextra -pedantic ...` globally via add_compile_options
# in EthCompilerSettings when PEDANTIC is on (the default). Those directory-scoped
# flags would otherwise be inherited by the evmone sources and break the build,
# so temporarily clear them for the duration of the evmone sub-build and restore
# them afterwards.
get_directory_property(_solidity_compile_options COMPILE_OPTIONS)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

FetchContent_MakeAvailable(evmone)

set_directory_properties(PROPERTIES COMPILE_OPTIONS "${_solidity_compile_options}")
