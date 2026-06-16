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
    source/utils/error.c
    source/utils/platform.c
    source/utils/repl.c
    source/utils/commands.c
    source/utils/build.c
    source/utils/bridge.c
    source/utils/apex_api.c
    source/core/tokenizer.c 
    source/core/parser.c 
    source/core/ast.c 
    source/core/bytecode.c 
    source/core/codegen.c 
    source/core/vm.c
    source/package_manager/install.c
    source/package_manager/publish.c
    source/libraries/os_module.c 
    source/libraries/files_module.c
    source/libraries/sys_module.c
    source/libraries/math_module.c 
    source/libraries/string_module.c 
    source/libraries/table_module.c
    source/libraries/ffi_module.c
    source/libraries/random_module.c
    source/libraries/regex_module.c
    source/libraries/json_module.c
)

add_executable(apex ${SOURCES})

# Compile the VFS packer utility
# If cross-compiling to MinGW from a non-Windows host, build the packer 
# with the HOST compiler so the host OS can execute it.
if(MINGW AND NOT CMAKE_HOST_WIN32)
    find_program(HOST_C_COMPILER NAMES gcc clang cc REQUIRED)
    set(HOST_PACKER "${CMAKE_CURRENT_BINARY_DIR}/vfs_packer_host")
    
    add_custom_command(
        OUTPUT ${HOST_PACKER}
        COMMAND ${HOST_C_COMPILER} -O2 -o ${HOST_PACKER} "${CMAKE_CURRENT_SOURCE_DIR}/source/utils/vfs_packer.c"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/utils/vfs_packer.c"
        COMMENT "Compiling host VFS packer..."
    )
    add_custom_target(vfs_packer_host ALL DEPENDS ${HOST_PACKER})
    add_dependencies(apex vfs_packer_host)
    
    set(VFS_PACKER_CMD ${HOST_PACKER})
else()
    add_executable(vfs_packer source/utils/vfs_packer.c)
    add_dependencies(apex vfs_packer)
    set(VFS_PACKER_CMD $<TARGET_FILE:vfs_packer>)
endif()

# Header search paths (#include "vm.h" will work from any directory)
target_include_directories(apex PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source/core
    ${CMAKE_CURRENT_SOURCE_DIR}/source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/source/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/source/package_manager
)

# ==========================================================
# COMPILER FLAGS FOR MAXIMUM SPEED
# ==========================================================

# Base flags for all builds
set(BASE_FLAGS 
    -Wall -Wextra 
    -fno-strict-aliasing 
    -D_DEFAULT_SOURCE
)

# High-performance flags for GCC/Clang (Linux/macOS/MinGW)
set(HIGH_PERF_FLAGS
    -O3                 # Maximum optimization level
    -march=native       # Optimize for current CPU (AVX/SSE/etc.)
    -funroll-loops      # Unroll loops (critical for VM dispatch loop)
    -fomit-frame-pointer # Free up a register
    -flto               # Link-Time Optimization (inter-procedural optimization)
)

# Apply flags based on compiler
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apex PRIVATE ${BASE_FLAGS} ${HIGH_PERF_FLAGS})
    
    # Enable LTO support in linker
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(apex PRIVATE -flto)
    endif()
elseif(MSVC)
    # MSVC equivalents
    target_compile_options(apex PRIVATE 
        /O2 /Ot /GL /arch:AVX2 /fp:fast
    )
    target_link_options(apex PRIVATE /LTCG)
else()
    # Fallback for other compilers
    target_compile_options(apex PRIVATE ${BASE_FLAGS} -O2)
endif()

# Windows-specific: suppress MinGW POSIX deprecation warnings
if(MINGW OR WIN32)
    target_compile_definitions(apex PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
    )
endif()

# Generate compile_commands.json for IDE support (VSCode, CLion)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# OS-specific library linking
if(MINGW)
    # MinGW cross-compilation
    target_link_libraries(apex PRIVATE
        shlwapi
        ws2_32      # Winsock
        wsock32     # Winsock legacy
        -static-libgcc
        -static
    )
    set_target_properties(apex PROPERTIES
        LINK_FLAGS "-Wl,--subsystem,console"
    )
elseif(MSVC)
    # Native MSVC build
    target_link_libraries(apex PRIVATE shlwapi zlibstatic)
elseif(WIN32)
    # Generic Windows (non-MinGW)
    target_link_libraries(apex PRIVATE shlwapi zlib)
elseif(APPLE)
    # macOS: link math and zlib
    target_link_libraries(apex PRIVATE m z dl)
else()
    # Linux: link math and zlib
    target_link_libraries(apex PRIVATE m z dl)
endif()

# Option: fully static build (zero system dependencies)
option(BUILD_STATIC "Build fully static binary" OFF)
if(BUILD_STATIC AND NOT MINGW)
    # Static linking for Linux/macOS
    set_target_properties(apex PROPERTIES LINK_FLAGS "-static")
endif()

# ==========================================================
# EMBEDDED PAYLOAD PACKER
# ==========================================================

file(GLOB_RECURSE EMBEDDED_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/source/embedded/*.apex")

list(REMOVE_DUPLICATES EMBEDDED_SCRIPTS)

if(EMBEDDED_SCRIPTS)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(NOT MINGW)
            add_custom_command(TARGET apex POST_BUILD
                COMMAND ${CMAKE_STRIP} $<TARGET_FILE:apex>
                COMMAND ${VFS_PACKER_CMD} $<TARGET_FILE:apex> "${CMAKE_CURRENT_SOURCE_DIR}" ${EMBEDDED_SCRIPTS}
                COMMENT "Stripping and embedding built-in payload..."
            )
        else()
            add_custom_command(TARGET apex POST_BUILD
                COMMAND x86_64-w64-mingw32-strip $<TARGET_FILE:apex>
                COMMAND ${VFS_PACKER_CMD} $<TARGET_FILE:apex> "${CMAKE_CURRENT_SOURCE_DIR}" ${EMBEDDED_SCRIPTS}
                COMMENT "Stripping and embedding built-in payload (MinGW)..."
            )
        endif()
    else()
        add_custom_command(TARGET apex POST_BUILD
            COMMAND ${VFS_PACKER_CMD} $<TARGET_FILE:apex> "${CMAKE_CURRENT_SOURCE_DIR}" ${EMBEDDED_SCRIPTS}
            COMMENT "Embedding built-in payload..."
        )
    endif()
endif()

# Windows cross-compilation: set .exe extension explicitly
if(MINGW)
    set_target_properties(apex PROPERTIES
        SUFFIX ".exe"
        PREFIX ""
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

# Platform-specific package settings
if(MINGW)
    # Windows packages
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-win64")
    set(CPACK_NSIS_DISPLAY_NAME "Apex Language")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Apex.lnk' '$INSTDIR\\\\bin\\\\apex.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU_FOLDER\\\\Apex.lnk'"
    )
elseif(APPLE)
    # macOS packages
    set(CPACK_GENERATOR "TGZ;DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-macos")
else()
    # Linux packages
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-linux")
    
    # Debian-specific metadata
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    
    # RPM-specific metadata
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Languages")
    set(CPACK_RPM_PACKAGE_REQUIRES "glibc")
endif()

# Static build: no dependencies required, adjust package name
if(BUILD_STATIC)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
    set(CPACK_RPM_PACKAGE_REQUIRES "")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-static")
endif()

# Include the packaging module
include(CPack)