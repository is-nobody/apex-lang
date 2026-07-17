cmake_minimum_required(VERSION 3.15)
project(Apex VERSION 26.07 LANGUAGES C)

# enables gnu extensions for posix functions (readlink, setenv, etc.)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# all source files grouped by module
set(SOURCES
    main.c
    source/utils/execute.c
    source/utils/error.c
    source/utils/platform.c
    source/utils/repl.c
    source/utils/commands.c
    source/utils/build.c
    source/utils/apex_api.c
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
    source/libraries/codecs_module.c
)

add_executable(apex ${SOURCES})

# header search paths for #include "vm.h" from any directory
target_include_directories(apex PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source/core
    ${CMAKE_CURRENT_SOURCE_DIR}/source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/source/libraries
)

# baseline warnings and posix compatibility
set(BASE_FLAGS 
    -Wall -Wextra
    -D_DEFAULT_SOURCE
)

# performance flags for gcc/clang (linux/macos/mingw)
set(HIGH_PERF_FLAGS
    -O3
    -flto
    -march=native
    -mtune=native
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apex PRIVATE ${BASE_FLAGS} ${HIGH_PERF_FLAGS})
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(apex PRIVATE -flto)
    endif()
elseif(MSVC)
    target_compile_options(apex PRIVATE /O2 /Ot /GL /fp:fast /MD)
    target_link_options(apex PRIVATE /LTCG)
else()
    target_compile_options(apex PRIVATE ${BASE_FLAGS} -O2)
endif()

# windows-specific defines to suppress deprecation warnings
if(MINGW OR WIN32)
    target_compile_definitions(apex PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# generates compile_commands.json for ide support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# os-specific library linking
if(MINGW)
    target_link_libraries(apex PRIVATE shlwapi ws2_32)
    set_target_properties(apex PROPERTIES
        LINK_FLAGS "-Wl,--subsystem,console"
    )
elseif(MSVC)
    target_link_libraries(apex PRIVATE shlwapi)
elseif(APPLE)
    target_link_libraries(apex PRIVATE m z dl)
elseif(WIN32)
    target_link_libraries(apex PRIVATE shlwapi)
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

# sets .exe extension explicitly for windows cross-compilation
if(MINGW)
    set_target_properties(apex PROPERTIES
        SUFFIX ".exe"
        PREFIX ""
    )
endif()

# installation rules
install(TARGETS apex RUNTIME DESTINATION bin)

# package metadata
set(CPACK_PACKAGE_NAME "apex")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "dev@apex-lang.org")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apex Language Interpreter")
set(CPACK_PACKAGE_DESCRIPTION "High-performance interpreted language with REPL and stdlib.")

# platform-specific package settings
if(MINGW)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-win64")
    set(CPACK_NSIS_DISPLAY_NAME "Apex Language")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Apex.lnk' '$INSTDIR\\\\bin\\\\apex.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Apex.lnk'"
    )
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-macos")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_PACKAGE_FILE_NAME "apex-${PROJECT_VERSION}-linux")
    
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Languages")
endif()

include(CPack)

#[[
# standard build command for linux
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel

# cross-compilation for windows from linux via mingw
cmake -S . -B build/windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc

cmake --build build/windows --parallel

# build debug version
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
]]