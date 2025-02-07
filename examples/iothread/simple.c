/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#include <bfenv/iothread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/eventfd.h>

#define TEST_DEPTH 1
#define TEST_BUFFER 256

int
main(int argc, const char *argv[])
{
    bfenv_iothread_t *iothread;
    struct pollfd pfd;
    eventfd_t signal;
    char buffer[TEST_BUFFER];
    int retval;

    iothread = bfenv_iothread_create(NULL, 8, BFENV_IOTHREAD_SIGREAD | BFENV_IOTHREAD_SIGWRITE);
    pfd.fd = iothread->eventfd;
    pfd.events = POLLIN;

    for (;;) {
        bfenv_iothread_read(iothread, STDIN_FILENO, buffer, TEST_BUFFER);
        retval = poll(&pfd, 1, -1);
        if (retval < 0) {
            perror("poll");
            return retval;
        }
        eventfd_read(iothread->eventfd, &signal);
        (void)signal;

        bfenv_iothread_write(iothread, STDOUT_FILENO, buffer, TEST_BUFFER);
        retval = poll(&pfd, 1, -1);
        if (retval < 0) {
            perror("poll");
            return retval;
        }
        eventfd_read(iothread->eventfd, &signal);
        (void)signal;
    }

}
