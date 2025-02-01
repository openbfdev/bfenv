# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2023 John Sanpe <sanpeqf@gmail.com>
#

set(BFENV_SOURCE
    ${BFENV_SOURCE}
    ${CMAKE_CURRENT_LIST_DIR}/iothread.c
)

include(${CMAKE_CURRENT_LIST_DIR}/eproc/build.cmake)
