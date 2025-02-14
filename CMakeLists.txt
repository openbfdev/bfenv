# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2023 ffashion <helloworldffashion@gmail.com>
# Copyright(c) 2023 John Sanpe <sanpeqf@gmail.com>
#

cmake_minimum_required(VERSION 3.12)
project(bfenv VERSION 0.0.1 LANGUAGES C)

include(GNUInstallDirs)
include(CheckIncludeFiles)

set(BFENV_NAME sagittarius)
set(BFENV_VERSION ${PROJECT_VERSION})
set(BFENV_EXTREVERSION -devel)

if(BFENV_EXTREVERSION)
    string(APPEND BFENV_VERSION ${BFENV_EXTREVERSION})
endif()

set(BFENV_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(BFENV_HEADER_PATH ${PROJECT_SOURCE_DIR}/include)
set(BFENV_SOURCE_PATH ${PROJECT_SOURCE_DIR}/src)
set(BFENV_GENERATED_PATH ${PROJECT_BINARY_DIR}/generated)
set(BFENV_CONFIGURE ${BFENV_GENERATED_PATH}/bfenv-config.cmake)

include(scripts/packed-header.cmake)
include(scripts/commit.cmake)

commit_hash(BFENV_COMMITID)
commit_branch(BFENV_BRANCH)
string(TIMESTAMP BFENV_BUILD_TIME "%A %B %d %H:%M:%S UTC %Y" UTC)

string(APPEND BFENV_RELEASE
    "${PROJECT_NAME}-${BFENV_NAME} "
    "${BFENV_VERSION} "
    "${BFENV_COMMITID}-${BFENV_BRANCH} "
    "(${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} ${CMAKE_HOST_SYSTEM_NAME}) "
    "${BFENV_BUILD_TIME}"
)

option(BFENV_DEVEL "Enable development mode" OFF)
option(BFENV_STRICT "Enable strict compilation" OFF)
option(BFENV_EXAMPLES "Build examples" OFF)
option(BFENV_DOCS "Install Documentation" OFF)

option(BFENV_ASAN "Enable Address Sanitizer" OFF)
option(BFENV_UBSAN "Enable Undefined Behaviour Sanitizer" OFF)
option(BFENV_GCOV "Enable Code Coverage Test" OFF)

if(BFENV_DEVEL)
    set(BFENV_STRICT ON)
    set(BFENV_EXAMPLES ON)
    set(BFENV_DOCS ON)
    set(BFENV_ASAN ON)
    set(BFENV_UBSAN ON)
    set(BFENV_GCOV ON)
endif()

include(${PROJECT_SOURCE_DIR}/build.cmake)

packed_header(
    bfenv/
    _BFENV_H_
    ${BFENV_GENERATED_PATH}/bfenv.h
    ${BFENV_HEADER_PATH}/bfenv
)

add_library(bfenv_object OBJECT ${BFENV_LIBRARY})
target_link_libraries(bfenv_object bfdev pthread)
add_library(bfenv ALIAS bfenv_object)

if(BFENV_EXAMPLES)
    enable_testing()
    add_subdirectory(examples)
endif()

if(${CMAKE_PROJECT_NAME} STREQUAL "bfenv")
    add_library(bfenv_static STATIC ${BFENV_LIBRARY})
    add_library(bfenv_shared SHARED ${BFENV_LIBRARY})

    target_link_libraries(bfenv_static bfdev pthread)
    target_link_libraries(bfenv_shared bfdev pthread)

    set_target_properties(bfenv_static
        PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        LINKER_LANGUAGE C
    )

    set_target_properties(bfenv_shared
        PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        LINKER_LANGUAGE C
        MACOSX_RPATH ON
    )

    install(FILES
        ${BFENV_GENERATED_PATH}/bfenv.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY
        ${BFENV_HEADER_PATH}/bfenv
        ${BFENV_GENERATED_PATH}/bfenv
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(FILES
        ${BFENV_CONFIGURE}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bfenv
    )

    install(TARGETS
        bfenv_static bfenv_shared
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    if(BFENV_DOCS)
        install(FILES
            ${PROJECT_SOURCE_DIR}/COPYING
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
        )
    endif()
endif()
