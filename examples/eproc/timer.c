/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#include <bfenv/eproc.h>
#include <bfdev/log.h>

static int
timer_func(bfenv_eproc_timer_t *timer, void *pdata)
{
    bfdev_log_info("%s\n", (const char *)pdata);
    return bfenv_eproc_timer_add(timer->eproc, timer, 200);
}

int
main(void)
{
    bfenv_eproc_timer_t timer;
    bfenv_eproc_t *eproc;
    int retval;

    eproc = bfenv_eproc_create(NULL, "select");
    if (!eproc)
        return 1;

    timer.func = timer_func;
    timer.pdata = "helloworld";

    retval = bfenv_eproc_timer_add(eproc, &timer, 200);
    if (retval)
        return retval;

    return bfenv_eproc_run(eproc, 1000);
}
