# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2023 John Sanpe <sanpeqf@gmail.com>
#

add_executable(bfenv-version version.c)
target_link_libraries(bfenv-version bfenv)
add_test(bfenv-version bfenv-version)

if(${CMAKE_PROJECT_NAME} STREQUAL "bfenv")
    install(FILES
        version.c
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/examples/bfenv
    )

    install(TARGETS
        bfenv-version
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/bin
    )
endif()
