/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#include <bfenv/eproc.h>
#include <bfdev/log.h>
#include <unistd.h>
#include <fcntl.h>

static int
event_func(bfenv_eproc_event_t *ev, void *pdata)
{
    char buff[256];

    if (read(ev->fd, buff, sizeof(buff)) < 0)
        return 1;
    bfdev_log_info("%s\n", buff);

    return 0;
}

static int
event_trigger(int pipe1, int pipe2)
{
    const char msg1[] = "prio1: helloworld";
    const char msg2[] = "prio2: helloworld";

    if ((write(pipe1, msg1, sizeof(msg1)) < 0) ||
        (write(pipe2, msg2, sizeof(msg2)) < 0))
        return 1;

    return 0;
}

int
main(void)
{
    bfenv_eproc_event_t event1, event2;
    int pipe1fd[2], pipe2fd[2];
    bfenv_eproc_t *eproc;
    int retval;

    eproc = bfenv_eproc_create(NULL, "select");
    if (!eproc)
        return 1;

    if (pipe(pipe1fd) || pipe(pipe2fd))
        return 1;

    event1.flags = BFENV_EPROC_READ;
    event1.fd = pipe1fd[0];
    event1.func = event_func;
    event1.priority = -100;

    event2.flags = BFENV_EPROC_READ;
    event2.fd = pipe2fd[0];
    event2.func = event_func;
    event2.priority = 100;

    retval = bfenv_eproc_event_add(eproc, &event1);
    if (retval)
        return retval;

    retval = bfenv_eproc_event_add(eproc, &event2);
    if (retval)
        return retval;

    retval = event_trigger(pipe1fd[1], pipe2fd[1]);
    if (retval)
        return retval;

    return bfenv_eproc_run(eproc, 200);
}
