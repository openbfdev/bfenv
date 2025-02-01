/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#define MODULE_NAME "eproc-epoll"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/eproc.h>
#include <bfdev/log.h>
#include <bfdev/radix.h>
#include <unistd.h>
#include <sys/epoll.h>

struct epoll_eproc {
    bfenv_eproc_t eproc;
    bfdev_array_t pbuff;
    int epfd, count;
};

#define eproc_to_epoll(ptr) \
    bfdev_container_of(ptr, struct epoll_eproc, eproc);

static int
epoll_fetch_events(bfenv_eproc_t *eproc, bfenv_msec_t timeout)
{
    struct epoll_eproc *sproc;
    int count, index, ready, nready;
    struct epoll_event *pfds;
    bool pending;

    sproc = eproc_to_epoll(eproc);
    count = bfdev_array_index(&sproc->pbuff);
    pfds = bfdev_array_data(&sproc->pbuff, 0);

    ready = epoll_wait(sproc->epfd, pfds, count, timeout);
    if (bfdev_unlikely(ready < 0)) {
        bfdev_log_alert("epoll wait failed: %d\n", ready);
        return -BFDEV_EIO;
    }

    if (!ready) {
        if (timeout == BFENV_TIMEOUT_MAX) {
            bfdev_log_alert("epoll returned no events\n");
            return -BFDEV_EINVAL;
        }

        return -BFDEV_ENOERR;
    }

    nready = 0;
    for (index = 0; index < ready; ++index) {
        bfenv_eproc_event_t *event;
        unsigned long flags;

        flags = 0;
        pending = false;

        if (pfds[index].events & EPOLLIN) {
            pfds[index].events &= ~EPOLLIN;
            bfenv_eproc_read_set(&flags);
            pending = true;
        }

        if (pfds[index].events & EPOLLOUT) {
            pfds[index].events &= ~EPOLLOUT;
            bfenv_eproc_write_set(&flags);
            pending = true;
        }

        if (pfds[index].events & EPOLLRDHUP) {
            pfds[index].events &= ~EPOLLRDHUP;
            bfenv_eproc_eof_set(&flags);
            pending = true;
        }

        if (pfds[index].events & (EPOLLERR | EPOLLHUP)) {
            pfds[index].events &= ~(EPOLLERR | EPOLLHUP);
            bfenv_eproc_error_set(&flags);
            pending = true;
        }

        if (pfds[index].events)
            bfdev_log_warn("epoll unknow revents: %#x\n", pfds[index].events);

        if (pending) {
            event = pfds[index].data.ptr;
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
epoll_event_register(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    struct epoll_eproc *sproc;
    struct epoll_event pfd;
    int retval;

    sproc = eproc_to_epoll(eproc);
    retval = bfdev_array_resize(&sproc->pbuff, ++sproc->count);
    if (bfdev_unlikely(retval)) {
        bfdev_log_alert("epoll buffer resize failed\n");
        return retval;
    }

    memset(&pfd, 0, sizeof(pfd));
    pfd.data.ptr = event;

    if (bfenv_eproc_read_test(&event->flags))
        pfd.events |= EPOLLIN;

    if (bfenv_eproc_write_test(&event->flags))
        pfd.events |= EPOLLOUT;

    if (bfenv_eproc_edge_test(&event->flags))
        pfd.events |= EPOLLET;

    retval = epoll_ctl(sproc->epfd, EPOLL_CTL_ADD, event->fd, &pfd);
    if (bfdev_unlikely(retval)) {
        sproc->count--;
        return retval;
    }

    return -BFDEV_ENOERR;
}

static void
epoll_event_unregister(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    struct epoll_eproc *sproc;
    int retval;

    sproc = eproc_to_epoll(eproc);
    sproc->count--;

    retval = epoll_ctl(sproc->epfd, EPOLL_CTL_DEL, event->fd, NULL);
    BFDEV_BUG_ON(retval);
}

static bfenv_eproc_t *
epoll_create_eproc(const bfdev_alloc_t *alloc)
{
    struct epoll_eproc *sproc;
    void *block;

    sproc = bfdev_zalloc(alloc, sizeof(*sproc));
    if (!sproc)
        return NULL;

    bfdev_array_init(&sproc->pbuff, alloc, sizeof(struct epoll_event));
    block = bfdev_array_push(&sproc->pbuff, 1);
    if (bfdev_unlikely(!block)) {
        bfdev_free(alloc, sproc);
        return NULL;
    }

    sproc->epfd = epoll_create(1);
    if (sproc->epfd < 0) {
        bfdev_array_release(&sproc->pbuff);
        bfdev_free(alloc, sproc);
        return NULL;
    }

    return &sproc->eproc;
}

static void
epoll_destory_eproc(bfenv_eproc_t *eproc)
{
    struct epoll_eproc *sproc;

    sproc = eproc_to_epoll(eproc);
    close(sproc->epfd);
    bfdev_array_release(&sproc->pbuff);
    bfdev_free(eproc->alloc, sproc);
}

static bfenv_eproc_func_t
eproc_epoll = {
    .name = "epoll",
    .fetch_events = epoll_fetch_events,
    .event_register = epoll_event_register,
    .event_unregister = epoll_event_unregister,
    .create = epoll_create_eproc,
    .destory = epoll_destory_eproc,
};

static __bfdev_ctor int
eproc_epoll_init(void)
{
    return bfenv_eproc_func_register(&eproc_epoll);
}

static __bfdev_dtor void
eproc_epoll_exit(void)
{
    bfenv_eproc_func_unregister(&eproc_epoll);
}
