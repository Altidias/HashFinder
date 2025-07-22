cmake_minimum_required(VERSION 3.20)
project(HashFinderAVX512 VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Create the static library
add_library(HashFinderAVX512 STATIC
    src/hashfinder.cpp
)

# Set target properties
set_target_properties(HashFinderAVX512 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Include directories
target_include_directories(HashFinderAVX512
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler-specific optimizations and AVX-512 flags
if(MSVC)
    target_compile_options(HashFinderAVX512 PRIVATE
        /arch:AVX512      # Enable AVX-512
        /O2               # Maximum optimization
        /GL               # Whole program optimization
        /Gy               # Function-level linking
        /GS-              # Disable security checks for performance
        /fp:fast          # Fast floating point
    )
    target_link_options(HashFinderAVX512 PRIVATE
        /LTCG             # Link-time code generation
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(HashFinderAVX512 PRIVATE
        -mavx512f         # Enable AVX-512F
        -mavx512cd        # Enable AVX-512 conflict detection
        -mavx512vl        # Enable AVX-512 vector length extensions
        -mavx512bw        # Enable AVX-512 byte and word
        -mavx512dq        # Enable AVX-512 doubleword and quadword
        -O3               # Maximum optimization
        -march=native     # Optimize for current CPU
        -mtune=native     # Tune for current CPU
        -flto             # Link-time optimization
        -ffast-math       # Fast math optimizations
    )
    target_link_options(HashFinderAVX512 PRIVATE
        -flto             # Link-time optimization
    )
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(HashFinderAVX512 PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Thread support (required for std::execution::par_unseq)
find_package(Threads REQUIRED)
target_link_libraries(HashFinderAVX512 PUBLIC Threads::Threads)

# TBB support for better parallel execution (optional but recommended)
find_package(TBB QUIET)
if(TBB_FOUND)
    target_link_libraries(HashFinderAVX512 PUBLIC TBB::tbb)
    target_compile_definitions(HashFinderAVX512 PUBLIC HASHFINDER_USE_TBB)
    message(STATUS "Intel TBB found - enhanced parallel execution enabled")
else()
    message(STATUS "Intel TBB not found - using standard library parallelism")
endif()

# Create an alias for easier consumption
add_library(HashFinder::AVX512 ALIAS HashFinderAVX512)

# Export targets for use by other projects
include(GNUInstallDirs)

# Installation rules
install(TARGETS HashFinderAVX512
    EXPORT HashFinderAVX512Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES include/hashfinder.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export configuration
install(EXPORT HashFinderAVX512Targets
    FILE HashFinderAVX512Targets.cmake
    NAMESPACE HashFinder::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HashFinderAVX512
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HashFinderAVX512ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HashFinderAVX512Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HashFinderAVX512Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HashFinderAVX512
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HashFinderAVX512Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HashFinderAVX512ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HashFinderAVX512
)

# Optional: Build examples/tests
option(HASHFINDER_BUILD_EXAMPLES "Build example programs" OFF)
if(HASHFINDER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(HASHFINDER_BUILD_TESTS "Build test programs" OFF)
if(HASHFINDER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "HashFinder AVX-512 Library Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
if(TBB_FOUND)
    message(STATUS "  Intel TBB: Enabled")
else()
    message(STATUS "  Intel TBB: Disabled")
endif()
message(STATUS "")

# CPU feature detection at configure time
include(CheckCXXSourceRuns)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} /arch:AVX512")
else()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -mavx512f")
endif()

check_cxx_source_runs("
#include <immintrin.h>
int main() {
    __m512i test = _mm512_set1_epi32(42);
    return 0;
}
" AVX512_COMPILE_TEST)

if(AVX512_COMPILE_TEST)
    message(STATUS "AVX-512 compilation test: PASSED")
else()
    message(WARNING "AVX-512 compilation test: FAILED - Library may not work correctly")
endif()
