# CMakeLists.txt
# Implementation of CMakeLists for Apex language
# https://github.com/is-nobody/apex-lang
# MIT license

cmake_minimum_required(VERSION 3.13)
project(Apex VERSION 26.09 LANGUAGES C)

# enables gnu extensions for posix functions (readlink, setenv, etc.)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# enables the numeric-pure jit
option(APEX_JIT_ENABLED "Enable the JIT compiler" ON)

# normalizes the target cpu name into a short arch tag
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" APEX_PROC_LOWER)
if(APEX_PROC_LOWER MATCHES "^(x86_64|amd64)")
    set(APEX_HOST_ARCH "x86_64")
elseif(APEX_PROC_LOWER MATCHES "^(aarch64|arm64)")
    set(APEX_HOST_ARCH "aarch64")
elseif(APEX_PROC_LOWER MATCHES "^(i[3-6]86|x86)")
    set(APEX_HOST_ARCH "x86")
else()
    set(APEX_HOST_ARCH "${APEX_PROC_LOWER}")
endif()

# checks if the jit backend is supported on this target
if(APEX_JIT_ENABLED AND APEX_HOST_ARCH STREQUAL "x86_64" AND
   (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
    CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
    WIN32))
    set(APEX_JIT_AVAILABLE TRUE)
else()
    set(APEX_JIT_AVAILABLE FALSE)
endif()

set(SOURCES
    main.c
    source/core/tokenizer.c
    source/core/parser.c
    source/core/ast.c
    source/core/bytecode.c
    source/core/codegen.c
    source/core/vm.c

    source/libraries/os_module.c
    source/libraries/sys_module.c
    source/libraries/math_module.c
    source/libraries/string_module.c
    source/libraries/table_module.c
    source/libraries/ffi_module.c
    source/libraries/random_module.c
    source/libraries/json_module.c
    source/libraries/xml_module.c
    source/libraries/csv_module.c
    source/libraries/base_module.c
    source/libraries/regex_module.c
    source/libraries/crypto_module.c
    source/libraries/zip_module.c
    source/libraries/network_module.c

    source/utils/execute.c
    source/utils/error.c
    source/utils/emit.c
    source/utils/platform.c
    source/utils/repl.c
    source/utils/commands.c
    source/utils/build.c
    source/utils/compile.c
    source/utils/loader.c
    source/utils/apex_api.c
)

if(APEX_JIT_AVAILABLE)
    list(APPEND SOURCES
        source/jit/jit.c
        source/jit/jit_analysis.c
        source/jit/x86-64/x86_64_common.c
        source/jit/x86-64/x86_64_emit.c
        source/jit/x86-64/x86_64.c
    )

    if(WIN32)
        list(APPEND SOURCES source/jit/x86-64/x86_64_windows.c)
    elseif(APPLE)
        list(APPEND SOURCES source/jit/x86-64/x86_64_macos.c)
    else()
        list(APPEND SOURCES source/jit/x86-64/x86_64_linux.c)
    endif()
endif()

add_executable(apex ${SOURCES})

# header search paths for include from any directory
target_include_directories(apex PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source/core
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/source/jit
    ${CMAKE_CURRENT_SOURCE_DIR}/source/jit/x86-64
)

# propagates the JIT toggle into C preprocessor
if(APEX_JIT_AVAILABLE)
    target_compile_definitions(apex PRIVATE APEX_JIT_ENABLED=1)
endif()

# baseline warnings and posix compatibility
set(BASE_FLAGS 
    -Wall -Wextra
    -D_DEFAULT_SOURCE
)

# performance flags for gcc/clang (linux/macos/mingw)
set(HIGH_PERF_FLAGS
    -O3
    -flto
)

# native cpu flags only when NOT cross-compiling
if(NOT CMAKE_CROSSCOMPILING)
    list(APPEND HIGH_PERF_FLAGS -march=native -mtune=native)
endif()

# applies gcc/clang-specific performance and base flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apex PRIVATE ${BASE_FLAGS} ${HIGH_PERF_FLAGS})
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(apex PRIVATE -flto)
    endif()
else()
    target_compile_options(apex PRIVATE ${BASE_FLAGS} -O2)
endif()

# windows-specific defines to suppress deprecation warnings
if(WIN32)
    target_compile_definitions(apex PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
endif()

# generates compile_commands.json for ide support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# os-specific library linking
if(WIN32)
    target_link_libraries(apex PRIVATE shlwapi ws2_32)
elseif(APPLE)
    target_link_libraries(apex PRIVATE m z dl)
else()
    target_link_libraries(apex PRIVATE m z dl)
endif()

# strips debug symbols in release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET apex POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:apex>
        COMMENT "Stripping debug symbols..."
    )
endif()

#[[
# standard build command
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --parallel
]]