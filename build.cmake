# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2023 John Sanpe <sanpeqf@gmail.com>
#

add_compile_options(
    -std=gnu11
    -Wall
    -Wextra
    -Wno-override-init
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-pointer-sign
    -Wno-null-pointer-arithmetic
    -Wmissing-prototypes
    -Wmissing-declarations
    -fvisibility=hidden
)

if(BFENV_STRICT)
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} \
         -Werror"
    )
endif()

include(scripts/sanitize.cmake)

configure_file(
    ${BFENV_MODULE_PATH}/config.h.in
    ${BFENV_GENERATED_PATH}/bfenv/config.h
)

configure_file(
    ${BFENV_MODULE_PATH}/bfenv-config.cmake.in
    ${BFENV_CONFIGURE}
)

file(GLOB_RECURSE BFENV_HEADER
    ${BFENV_HEADER_PATH}/*.h
)

file(GLOB_RECURSE BFENV_GENERATED_HEADER
    ${BFENV_GENERATED_PATH}/*.h
)

set(BFENV_INCLUDE_DIRS
    ${BFENV_HEADER_PATH}
    ${BFENV_GENERATED_PATH}
)

set_property(
    GLOBAL PROPERTY
    "BFENV_INCLUDE_DIRS"
    ${BFENV_INCLUDE_DIRS}
)

include_directories(${BFENV_INCLUDE_DIRS})
include(${BFENV_SOURCE_PATH}/build.cmake)

set(BFENV_LIBRARY_HEADER
    ${BFENV_HEADER}
    ${BFENV_GENERATED_HEADER}
)

set(BFENV_LIBRARY_SOURCE
    ${BFENV_SOURCE}
)

set(BFENV_LIBRARY
    ${BFENV_LIBRARY_HEADER}
    ${BFENV_LIBRARY_SOURCE}
)
