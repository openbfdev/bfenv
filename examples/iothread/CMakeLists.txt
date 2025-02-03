# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2023 John Sanpe <sanpeqf@gmail.com>
#

add_executable(simple simple.c)
target_link_libraries(simple bfenv)

if(${CMAKE_PROJECT_NAME} STREQUAL "bfenv")
    install(FILES
        simple.c
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/examples/decoder
    )

    install(TARGETS
        simple
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/bin
    )
endif()
