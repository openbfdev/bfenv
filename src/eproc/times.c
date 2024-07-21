/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

static inline bfenv_msec_t
monotonic_time(void)
{
    struct timespec ts;
    time_t sec;
    long msec;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    sec = ts.tv_sec;
    msec = ts.tv_nsec / 1000000;

    return (bfenv_msec_t)sec * 1000 + msec;
}

static void
eproc_times_update(bfenv_eproc_t *eproc)
{
    eproc->current_msec = monotonic_time();
}
