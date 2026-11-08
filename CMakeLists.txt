# CMakeLists.txt
# Implementation of CMakeLists for Apex language
# https://github.com/is-nobody/apex-lang
# MIT license

cmake_minimum_required(VERSION 3.13)
project(Apex VERSION 26.11 LANGUAGES C)

# enables gnu extensions for posix functions (readlink, setenv, etc.)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# enables the jit
option(APEX_JIT_ENABLED "Enable the JIT compiler" ON)

# enables debug tracing of the jit
option(APEX_JIT_TRACE "Enable JIT debug tracing" OFF)

# path to a local checkout of the Apex UI project; leave empty to fetch it
set(APEX_UI_DIR "" CACHE PATH "Path to a local Apex UI checkout")

# apex ui dependency
if(APEX_UI_DIR)
    add_subdirectory(${APEX_UI_DIR} ${CMAKE_BINARY_DIR}/apex-ui EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(apexui
        GIT_REPOSITORY https://github.com/is-nobody/apex-ui.git
        GIT_TAG        main)
    set(APEXUI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(apexui)
    set_property(DIRECTORY ${apexui_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL TRUE)
endif()

# normalizes the target cpu name into a short arch tag
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" APEX_PROC_LOWER)
if(APEX_PROC_LOWER MATCHES "^(x86_64|amd64)")
    set(APEX_HOST_ARCH "x86-64")
elseif(APEX_PROC_LOWER MATCHES "^(aarch64|arm64)")
    set(APEX_HOST_ARCH "aarch64")
else()
    set(APEX_HOST_ARCH "${APEX_PROC_LOWER}")
endif()

# checks if the jit backend is supported on this target
if(APEX_JIT_ENABLED AND APEX_HOST_ARCH STREQUAL "x86-64" AND
   (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
    CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
    WIN32))
    set(APEX_JIT_AVAILABLE TRUE)
else()
    set(APEX_JIT_AVAILABLE FALSE)
endif()

# sources
set(SOURCES
    main.c
    source/core/tokenizer.c
    source/core/parser.c
    source/core/ast.c
    source/core/bytecode.c
    source/core/vm.c

    source/compiler/codegen.c
    source/compiler/codegen_expr.c
    source/compiler/codegen_stmt.c
    source/compiler/codegen_if.c
    source/compiler/codegen_for.c
    source/compiler/codegen_match.c
    source/compiler/codegen_fn.c
    source/compiler/codegen_scope.c
    source/compiler/codegen_modules.c
    source/compiler/opt_const_fold.c
    source/compiler/opt_const_cache.c
    source/compiler/opt_lvn.c
    source/compiler/opt_peephole.c
    source/compiler/opt_inline.c
    source/compiler/opt_licm.c
    source/compiler/opt_dce.c
    source/compiler/opt_branch_merge.c
    source/compiler/opt_jump_targets.c
    source/compiler/opt_linear_scan.c
    source/compiler/opt_bc_dce.c
    source/compiler/opt_for_fold.c
    source/compiler/opt_recursion.c

    source/libraries/os/os_module.c
    source/libraries/sys/sys_module.c
    source/libraries/math/math_module.c
    source/libraries/string/string_module.c
    source/libraries/table/table_module.c
    source/libraries/random/random_module.c
    source/libraries/json/json_module.c
    source/libraries/xml/xml_module.c
    source/libraries/csv/csv_module.c
    source/libraries/base/base_module.c
    source/libraries/regex/regex_module.c
    source/libraries/crypto/crypto_module.c
    source/libraries/zip/zip_module.c
    source/libraries/datetime/datetime_module.c
    source/libraries/ui/ui_module.c

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

# the jit sources stay on the language side
if(APEX_JIT_AVAILABLE)
    list(APPEND SOURCES
        source/jit/jit.c
        source/jit/jit_analysis.c
        source/jit/x86-64/x86_64_common.c
        source/jit/x86-64/x86_64_emit_common.c
        source/jit/x86-64/x86_64_emit_function.c
        source/jit/x86-64/x86_64_emit_move.c
        source/jit/x86-64/x86_64_emit_arith.c
        source/jit/x86-64/x86_64_emit_cmp.c
        source/jit/x86-64/x86_64_emit_jump.c
        source/jit/x86-64/x86_64_emit_match.c
        source/jit/x86-64/x86_64_emit_call.c
        source/jit/x86-64/x86_64_emit_table.c
        source/jit/x86-64/x86_64_emit_loop.c
        source/jit/x86-64/x86_64.c
    )

    if(APEX_JIT_TRACE)
        list(APPEND SOURCES source/jit/jit_disasm.c)
    endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/source/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/os
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/sys
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/math
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/string
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/table
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/random
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/json
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/xml
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/csv
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/base
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/regex
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/zip
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/datetime
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/source/jit
    ${CMAKE_CURRENT_SOURCE_DIR}/source/jit/x86-64
)

# propagates the jit toggle into c preprocessor
if(APEX_JIT_AVAILABLE)
    target_compile_definitions(apex PRIVATE APEX_JIT_ENABLED=1)
    if(APEX_JIT_TRACE)
        target_compile_definitions(apex PRIVATE APEX_JIT_TRACE_BUILD=1)
    endif()
endif()

# baseline warnings and posix compatibility
set(BASE_FLAGS -Wall -Wextra -D_DEFAULT_SOURCE)

# applies gcc/clang-specific performance and base flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apex PRIVATE
        ${BASE_FLAGS}
        $<$<CONFIG:Release>:-O3>
    )

    include(CheckIPOSupported)
    check_ipo_supported(RESULT APEX_IPO_OK OUTPUT APEX_IPO_ERR)
    if(APEX_IPO_OK)
        set_property(TARGET apex PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(WARNING "IPO/LTO not supported: ${APEX_IPO_ERR}")
    endif()
endif()

# windows-specific defines to suppress deprecation warnings
if(WIN32)
    target_compile_definitions(apex PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
endif()

# threading support for background i/o workers
find_package(Threads REQUIRED)
target_link_libraries(apex PRIVATE Threads::Threads)

# the ui backend
target_link_libraries(apex PRIVATE apexui)

# strips debug symbols in release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET apex POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:apex>
    )
endif()

#[[
# standard build command
cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --parallel
]]