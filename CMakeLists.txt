cmake_minimum_required(VERSION 3.20)
project(ApexHFT LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Chuẩn ngôn ngữ
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Mặc định build Release. Benchmark ở -O0 là vô nghĩa.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

# Không để file .exe rơi vào thư mục source
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(APEX_BUILD_BENCHMARKS "Build benchmark binaries" ON)
# Bật -march=native chỉ khi chạy trên máy của chính mình (không mang binary sang máy khác)
option(APEX_NATIVE_ARCH "Compile with -march=native" OFF)

# ---------------------------------------------------------------------------
# Thư viện header-only: toàn bộ engine nằm trong include/
# ---------------------------------------------------------------------------
add_library(apex_core INTERFACE)
target_include_directories(apex_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/engine
)

if(MSVC)
    target_compile_options(apex_core INTERFACE
        /W4
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Debug>:/Od /RTC1>
    )
else()
    target_compile_options(apex_core INTERFACE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-O0 -g3>
    )
    if(APEX_NATIVE_ARCH)
        target_compile_options(apex_core INTERFACE -march=native)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()

function(apex_add_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE apex_core)

    # Release build của CMake tự thêm -DNDEBUG => assert() trở thành no-op.
    # Test mà assert bị tắt thì "pass" cả khi code sai, nên phải bật lại.
    if(MSVC)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/UNDEBUG>)
    else()
        target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:-UNDEBUG>)
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

apex_add_test(test_orderbook tests/test_orderbook.cpp)

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
if(APEX_BUILD_BENCHMARKS)
    function(apex_add_bench target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE apex_core)
    endfunction()

    apex_add_bench(benchmark_ringbuffer tests/benchmark_ringbuffer.cpp)
    apex_add_bench(benchmark_orderbook  tests/benchmark_orderbook.cpp)
endif()
