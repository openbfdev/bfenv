# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
#

add_executable(eproc-echo echo.c)
target_link_libraries(eproc-echo bfenv bfdev)
add_test(eproc-echo eproc-echo)

add_executable(eproc-timer timer.c)
target_link_libraries(eproc-timer bfenv bfdev)
add_test(eproc-timer eproc-timer)

if(${CMAKE_PROJECT_NAME} STREQUAL "bfenv")
    install(FILES
        timer.c
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/examples/eproc
    )

    install(TARGETS
        eproc-timer
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/bin
    )
endif()
