cmake_minimum_required(VERSION 3.24)

# Some pinned dependencies (doctest 2.4.11) still declare a pre-3.5
# cmake_minimum_required, which CMake 4.x rejects outright. Opt back in.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(minerby VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MINERBY_BUILD_TESTS "Build the test suite" ON)
option(MINERBY_WITH_RANDOMX "Build the RandomX (Monero) engine" ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.14.1
  GIT_SHALLOW    TRUE)
FetchContent_Declare(cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.2
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nlohmann_json spdlog cli11)

if(MINERBY_BUILD_TESTS)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(doctest)
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/minerby/version.hpp
  @ONLY)

# RandomX (Monero PoW) - vendored as a submodule, pinned to a v1.x tag for
# Monero mainnet (rx/0) compatibility.
if(MINERBY_WITH_RANDOMX)
  if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/RandomX/CMakeLists.txt)
    message(FATAL_ERROR
      "third_party/RandomX is missing. Run: git submodule update --init --recursive")
  endif()
  add_subdirectory(third_party/RandomX EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(minerby_core STATIC
  src/pow/sha256.cpp
  src/pow/sha256d_hasher.cpp
  src/pow/target.cpp
  src/net/tcp_client.cpp
  src/pool/stratum_v1_client.cpp
  src/pool/monero_client.cpp
  src/stratum/parse.cpp
  src/stratum/work.cpp
  src/miner/worker.cpp
  src/telemetry/metrics.cpp
  src/telemetry/http_server.cpp
  src/telemetry/stats_store.cpp
  src/config/config.cpp
  src/util/platform.cpp
)

if(MINERBY_WITH_RANDOMX)
  target_sources(minerby_core PRIVATE
    src/pow/randomx_context.cpp
    src/pow/randomx_hasher.cpp)
  target_link_libraries(minerby_core PUBLIC randomx)
  target_include_directories(minerby_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/RandomX/src)
  target_compile_definitions(minerby_core PUBLIC MINERBY_WITH_RANDOMX=1)
endif()

target_include_directories(minerby_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(minerby_core PUBLIC
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)

if(WIN32)
  target_link_libraries(minerby_core PUBLIC ws2_32)
  target_compile_definitions(minerby_core PUBLIC
    WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
endif()

if(MSVC)
  target_compile_options(minerby_core PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(minerby_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(minerby src/app/main.cpp)
target_link_libraries(minerby PRIVATE minerby_core CLI11::CLI11)
if(MSVC)
  target_compile_options(minerby PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(minerby PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(MINERBY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
