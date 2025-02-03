/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#define MODULE_NAME "eproc-select"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/eproc.h>
#include <bfdev/log.h>
#include <bfdev/radix.h>
#include <sys/select.h>

struct select_eproc {
    bfenv_eproc_t eproc;
    BFDEV_DECLARE_RADIX(fdmap, bfenv_eproc_event_t *);
    fd_set read_fds;
    fd_set write_fds;
    fd_set error_fds;
};

#define eproc_to_select(ptr) \
    bfdev_container_of(ptr, struct select_eproc, eproc);

static int
select_fetch_events(bfenv_eproc_t *eproc, bfenv_msec_t timeout)
{
    struct select_eproc *sproc;
    bfenv_eproc_event_t **evslot;
    fd_set read_fds, write_fds, error_fds;
    struct timeval tv, *tvp;
    int index, ready, nready;

    sproc = eproc_to_select(eproc);
    if (timeout == BFENV_TIMEOUT_MAX)
        tvp = NULL;
    else {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }

    read_fds = sproc->read_fds;
    write_fds = sproc->write_fds;
    error_fds = sproc->error_fds;

    /* when return null, it means any fd is not set */
    if (bfdev_radix_last(&sproc->fdmap, &index))
        index++;

    ready = select(index, &read_fds, &write_fds, &error_fds, tvp);
    if (bfdev_unlikely(ready < 0)) {
        bfdev_log_alert("select wait failed: %d\n", ready);
        return -BFDEV_EIO;
    }

    if (!ready) {
        if (timeout == BFENV_TIMEOUT_MAX) {
            bfdev_log_alert("select returned no events\n");
            return -BFDEV_EINVAL;
        }

        return -BFDEV_ENOERR;
    }

    nready = 0;
    bfdev_radix_for_each(evslot, &sproc->fdmap, &index) {
        bfenv_eproc_event_t *event;
        bool pending;

        if (ready == nready)
            break;

        event = *evslot;
        pending = false;

        if (FD_ISSET(index, &read_fds)) {
            bfenv_eproc_read_set(&event->events);
            pending = true;
        }

        if (FD_ISSET(index, &write_fds)) {
            bfenv_eproc_write_set(&event->events);
            pending = true;
        }

        if (FD_ISSET(index, &write_fds)) {
            bfenv_eproc_write_set(&event->events);
            pending = true;
        }

        if (FD_ISSET(index, &error_fds)) {
            bfenv_eproc_error_set(&event->events);
            pending = true;
        }

        if (pending) {
            bfenv_eproc_event_pend(eproc, event);
            nready++;
        }
    }

    if (ready != nready) {
        bfdev_log_warn("ready != nready, ready %d, nready %d\n", ready, nready);
        return -BFDEV_ENODATA;
    }

    return -BFDEV_ENOERR;
}

static int
select_event_register(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    struct select_eproc *sproc;
    bfenv_eproc_event_t **evslot;
    int index;

    sproc = eproc_to_select(eproc);
    index = event->fd;

    if (bfdev_unlikely(index > FD_SETSIZE)) {
        bfdev_log_err("fdnum bigger than FD_SETSIZE\n");
        return -BFDEV_EMLINK;
    }

    if (bfdev_unlikely(bfenv_eproc_edge_test(&event->flags))) {
        bfdev_log_err("\n");
        return -BFDEV_EINVAL;
    }

    if (bfdev_radix_find(&sproc->fdmap, index)) {
        bfdev_log_err("fdset already set\n");
        return -BFDEV_EALREADY;
    }

    evslot = bfdev_radix_alloc(&sproc->fdmap, index);
    if (!evslot) {
        bfdev_log_err("radix alloc failed\n");
        return -BFDEV_ENOMEM;
    }

    *evslot = event;
    if (bfenv_eproc_read_test(&event->flags)) {
        FD_SET(index, &sproc->read_fds);
        FD_SET(index, &sproc->error_fds);
    }

    if (bfenv_eproc_write_test(&event->flags)) {
        FD_SET(index, &sproc->write_fds);
        FD_SET(index, &sproc->error_fds);
    }

    return -BFDEV_ENOERR;
}

static void
select_event_unregister(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    struct select_eproc *sproc;
    int index;

    sproc = eproc_to_select(eproc);
    index = event->fd;

    FD_CLR(index, &sproc->read_fds);
    FD_CLR(index, &sproc->write_fds);
    FD_CLR(index, &sproc->error_fds);
    bfdev_radix_free(&sproc->fdmap, index);
}

static bfenv_eproc_t *
select_create_eproc(const bfdev_alloc_t *alloc)
{
    struct select_eproc *sproc;

    sproc = bfdev_zalloc(alloc, sizeof(*sproc));
    if (!sproc)
        return NULL;

    sproc->fdmap = BFDEV_RADIX_INIT(&sproc->fdmap, alloc);
    FD_ZERO(&sproc->read_fds);
    FD_ZERO(&sproc->write_fds);

    return &sproc->eproc;
}

static void
select_destory_eproc(bfenv_eproc_t *eproc)
{
    struct select_eproc *sproc;

    sproc = eproc_to_select(eproc);
    bfdev_radix_release(&sproc->fdmap);
    bfdev_free(eproc->alloc, sproc);
}

static bfenv_eproc_func_t
eproc_select = {
    .name = "select",
    .fetch_events = select_fetch_events,
    .event_register = select_event_register,
    .event_unregister = select_event_unregister,
    .create = select_create_eproc,
    .destory = select_destory_eproc,
};

static __bfdev_ctor int
eproc_select_init(void)
{
    return bfenv_eproc_func_register(&eproc_select);
}

static __bfdev_dtor void
eproc_select_exit(void)
{
    bfenv_eproc_func_unregister(&eproc_select);
}
