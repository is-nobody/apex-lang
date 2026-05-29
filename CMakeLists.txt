cmake_minimum_required(VERSION 3.15)
project(Apex VERSION 26.06 LANGUAGES C)

# Use GNU extensions for POSIX function support (readlink, setenv, etc.)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# List of all source files, including new directories
set(SOURCES
    main.c 
    source/utils/execute.c 
    source/utils/add_to_path.c 
    source/core/tokenizer.c 
    source/core/parser.c 
    source/core/ast.c 
    source/core/sema.c 
    source/core/bytecode.c 
    source/core/codegen.c 
    source/core/vm.c
    source/libraries/os_module.c 
    source/libraries/math_module.c 
    source/libraries/string_module.c 
    source/libraries/table_module.c
)

add_executable(apex ${SOURCES})

# Header search paths (#include "vm.h" will work from any directory)
target_include_directories(apex PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}                # Root (for main.h, etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/source/core    # For vm.h, parser.h, etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/source/utils   # For execute.h, add_to_path.h
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries # For os_module.h, etc.
)

# Compiler flags
target_compile_options(apex PRIVATE
    -Wall -Wextra -O2 -fno-strict-aliasing -D_DEFAULT_SOURCE
)

# Generate compile_commands.json for IDE support (VSCode, CLion)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# OS-specific library linking
if(WIN32)
    target_link_libraries(apex PRIVATE shlwapi)
elseif(APPLE)
    target_link_libraries(apex PRIVATE m)
else()
    # Linux requires libm for math functions
    target_link_libraries(apex PRIVATE m)
endif()

# Option: fully static build (zero system dependencies)
option(BUILD_STATIC "Build fully static binary" OFF)
if(BUILD_STATIC)
    set_target_properties(apex PROPERTIES LINK_FLAGS "-static")
endif()

# Strip debug symbols in Release mode (reduces binary size)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET apex POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:apex>
        COMMENT "Stripping debug symbols..."
    )
endif()

# ==========================================================
# PACKAGING
# ==========================================================

# 1. Install rules
install(TARGETS apex RUNTIME DESTINATION bin)

# 2. Package metadata
set(CPACK_PACKAGE_NAME "apex")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "dev@apex-lang.org")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apex Language Interpreter")
set(CPACK_PACKAGE_DESCRIPTION "High-performance interpreted language with REPL and stdlib.")
set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}")

# 3. Generator selection
set(CPACK_GENERATOR "DEB;RPM;TGZ")

# 4. Dependencies
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Development/Languages")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc")

# If static build -> no dependencies required
if(BUILD_STATIC)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
    set(CPACK_RPM_PACKAGE_REQUIRES "")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-static")
endif()

# Include the packaging module
include(CPack)