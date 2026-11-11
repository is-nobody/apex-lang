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

# auto-discovery
file(GLOB_RECURSE APEX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
list(APPEND APEX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.c")

# jit sources are only compiled when the backend is available
set(APEX_JIT_SOURCES "")
foreach(src IN LISTS APEX_SOURCES)
    if(src MATCHES "/source/jit/")
        list(APPEND APEX_JIT_SOURCES "${src}")
    endif()
endforeach()
if(APEX_JIT_SOURCES)
    list(REMOVE_ITEM APEX_SOURCES ${APEX_JIT_SOURCES})
endif()

if(APEX_JIT_AVAILABLE)
    # only the backend for the current os
    list(FILTER APEX_JIT_SOURCES EXCLUDE
        REGEX "/x86-64/x86_64_(linux|macos|windows)\\.c$")

    if(WIN32)
        list(APPEND APEX_JIT_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/source/jit/x86-64/x86_64_windows.c")
    elseif(APPLE)
        list(APPEND APEX_JIT_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/source/jit/x86-64/x86_64_macos.c")
    else()
        list(APPEND APEX_JIT_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/source/jit/x86-64/x86_64_linux.c")
    endif()

    if(NOT APEX_JIT_TRACE)
        list(FILTER APEX_JIT_SOURCES EXCLUDE REGEX "/jit_disasm\\.c$")
    endif()

    list(APPEND APEX_SOURCES ${APEX_JIT_SOURCES})
endif()

add_executable(apex ${APEX_SOURCES})

# auto-discovery
file(GLOB_RECURSE APEX_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(APEX_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(header IN LISTS APEX_HEADERS)
    get_filename_component(dir "${header}" DIRECTORY)
    list(APPEND APEX_INCLUDE_DIRS "${dir}")
endforeach()
list(REMOVE_DUPLICATES APEX_INCLUDE_DIRS)

target_include_directories(apex PRIVATE ${APEX_INCLUDE_DIRS})

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
target_link_libraries(apex PRIVATE apex-ui)

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