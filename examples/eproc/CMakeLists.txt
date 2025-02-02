# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
#

add_executable(eproc-echo echo.c)
target_link_libraries(eproc-echo bfenv bfdev)

add_executable(eproc-prio prio.c)
target_link_libraries(eproc-prio bfenv bfdev)
add_test(eproc-prio eproc-prio)

add_executable(eproc-timer timer.c)
target_link_libraries(eproc-timer bfenv bfdev)
add_test(eproc-timer eproc-timer)

if(${CMAKE_PROJECT_NAME} STREQUAL "bfenv")
    install(FILES
        echo.c
        prio.c
        timer.c
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/examples/eproc
    )

    install(TARGETS
        eproc-echo
        eproc-prio
        eproc-timer
        DESTINATION
        ${CMAKE_INSTALL_DOCDIR}/bin
    )
endif()
