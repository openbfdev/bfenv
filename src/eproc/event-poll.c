/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#define MODULE_NAME "eproc-poll"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/eproc.h>
#include <bfdev/log.h>
#include <bfdev/radix.h>
#include <sys/poll.h>

struct poll_eproc {
    bfenv_eproc_t eproc;
    BFDEV_DECLARE_RADIX(fdmap, bfenv_eproc_event_t *);
    bfdev_array_t pbuff;
};

#define eproc_to_poll(ptr) \
    bfdev_container_of(ptr, struct poll_eproc, eproc);

static int
poll_fetch_events(bfenv_eproc_t *eproc, bfenv_msec_t timeout)
{
    struct poll_eproc *sproc;
    int count, index, ready, nready;
    struct pollfd *pfds;
    bool pending;

    sproc = eproc_to_poll(eproc);
    count = bfdev_array_index(&sproc->pbuff);
    pfds = bfdev_array_data(&sproc->pbuff, 0);

    ready = poll(pfds, count, timeout);
    if (bfdev_unlikely(ready < 0)) {
        bfdev_log_alert("poll wait failed: %d\n", ready);
        return -BFDEV_EIO;
    }

    if (!ready) {
        if (timeout == BFENV_TIMEOUT_MAX) {
            bfdev_log_alert("poll returned no events\n");
            return -BFDEV_EINVAL;
        }

        return -BFDEV_ENOERR;
    }

    nready = 0;
    for (index = 0; index < count; ++index) {
        bfenv_eproc_event_t **evslot, *event;
        unsigned long flags;

        if (ready == nready)
            break;

        flags = 0;
        pending = false;

        if (pfds[index].revents & POLLIN) {
            pfds[index].revents &= ~POLLIN;
            bfenv_eproc_read_set(&flags);
            pending = true;
        }

        if (pfds[index].revents & POLLOUT) {
            pfds[index].revents &= ~POLLOUT;
            bfenv_eproc_write_set(&flags);
            pending = true;
        }

        if (pfds[index].revents) {
            bfdev_log_err("poll unknow revents: %#x\n", pfds[index].revents);
            return -BFDEV_EIO;
        }

        if (pending) {
            evslot = bfdev_radix_find(&sproc->fdmap, pfds[index].fd);
            BFDEV_BUG_ON(!evslot);

            event = *evslot;
            event->events = flags;
            bfdev_list_add(&eproc->pending, &event->node);
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
poll_event_register(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    struct poll_eproc *sproc;
    bfenv_eproc_event_t **evslot;
    struct pollfd *pfd;
    int index;

    sproc = eproc_to_poll(eproc);
    index = event->fd;

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

    pfd = bfdev_array_push(&sproc->pbuff, 1);
    if (!pfd) {
        bfdev_log_err("array alloc failed\n");
        return -BFDEV_ENOMEM;
    }

    *evslot = event;
    pfd->fd = index;

    if (bfenv_eproc_read_test(&event->flags))
        pfd->events |= POLLIN;

    if (bfenv_eproc_write_test(&event->flags))
        pfd->events |= POLLOUT;

    return -BFDEV_ENOERR;
}

static void
poll_event_unregister(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    struct poll_eproc *sproc;
    bfenv_eproc_event_t **evslot;
    struct pollfd *pfd;
    int index;

    sproc = eproc_to_poll(eproc);
    index = event->fd;

    bfdev_radix_free(&sproc->fdmap, index);
    bfdev_array_reset(&sproc->pbuff);

    bfdev_radix_for_each(evslot, &sproc->fdmap, &index) {
        pfd = bfdev_array_push(&sproc->pbuff, 1);
        BFDEV_BUG_ON(!pfd);

        *evslot = event;
        pfd->fd = index;
    }
}

static bfenv_eproc_t *
poll_create_eproc(const bfdev_alloc_t *alloc)
{
    struct poll_eproc *sproc;

    sproc = bfdev_malloc(alloc, sizeof(*sproc));
    if (!sproc)
        return NULL;

    sproc->fdmap = BFDEV_RADIX_INIT(&sproc->fdmap, alloc);
    bfdev_array_init(&sproc->pbuff, alloc, sizeof(struct pollfd));

    return &sproc->eproc;
}

static void
poll_destory_eproc(bfenv_eproc_t *eproc)
{
    struct poll_eproc *sproc;

    sproc = eproc_to_poll(eproc);
    bfdev_radix_release(&sproc->fdmap);
    bfdev_free(eproc->alloc, sproc);
}

static bfenv_eproc_func_t
eproc_poll = {
    .name = "poll",
    .fetch_events = poll_fetch_events,
    .event_register = poll_event_register,
    .event_unregister = poll_event_unregister,
    .create = poll_create_eproc,
    .destory = poll_destory_eproc,
};

static __bfdev_ctor int
eproc_poll_init(void)
{
    return bfenv_eproc_func_register(&eproc_poll);
}

static __bfdev_dtor void
eproc_poll_exit(void)
{
    bfenv_eproc_func_unregister(&eproc_poll);
}
