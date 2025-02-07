/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#ifndef _BFENV_IOTHREAD_H_
#define _BFENV_IOTHREAD_H_

#include <pthread.h>
#include <semaphore.h>
#include <bfdev/allocator.h>
#include <bfdev/fifo.h>
#include <bfdev/list.h>
#include <bfdev/bitflags.h>

BFDEV_BEGIN_DECLS

typedef struct bfenv_iothread bfenv_iothread_t;
typedef struct bfenv_iothread_request bfenv_iothread_request_t;
typedef enum bfenv_iothread_signal bfenv_iothread_signal_t;
typedef enum bfenv_iothread_flags bfenv_iothread_flags_t;
typedef enum bfenv_iothread_event bfenv_iothread_event_t;
typedef enum bfenv_iothread_state bfenv_iothread_state_t;

enum bfenv_iothread_flags {
    __BFENV_IOTHREAD_SIGREAD = 0,
    __BFENV_IOTHREAD_SIGWRITE,
    __BFENV_IOTHREAD_SIGSYNC,

    BFENV_IOTHREAD_SIGREAD = BFDEV_BIT(__BFENV_IOTHREAD_SIGREAD),
    BFENV_IOTHREAD_SIGWRITE = BFDEV_BIT(__BFENV_IOTHREAD_SIGWRITE),
    BFENV_IOTHREAD_SIGSYNC = BFDEV_BIT(__BFENV_IOTHREAD_SIGSYNC),
};

enum bfenv_iothread_event {
    BFENV_IOTHREAD_EVENT_READ = 0,
    BFENV_IOTHREAD_EVENT_WRITE,
    BFENV_IOTHREAD_EVENT_SYNC,
};

struct bfenv_iothread_request {
    bfenv_iothread_event_t event;
    bfdev_list_head_t pending;
    int error;
    int fd;

    void *buffer;
    size_t size;
};

struct bfenv_iothread {
    const bfdev_alloc_t *alloc;
    unsigned long flags;
    int eventfd;

    pthread_t worker_thread;
    sem_t pending;
    int error;

    BFDEV_DECLARE_FIFO_DYNAMIC(pending_works, bfenv_iothread_request_t);
    BFDEV_DECLARE_FIFO_DYNAMIC(done_works, bfenv_iothread_request_t);
    bfdev_list_head_t pending_dones;
};

BFDEV_BITFLAGS_STRUCT(
    bfenv_iothread_sigread,
    bfenv_iothread_t, flags,
    __BFENV_IOTHREAD_SIGREAD
);

BFDEV_BITFLAGS_STRUCT(
    bfenv_iothread_sigwrite,
    bfenv_iothread_t, flags,
    __BFENV_IOTHREAD_SIGWRITE
);

BFDEV_BITFLAGS_STRUCT(
    bfenv_iothread_sigsync,
    bfenv_iothread_t, flags,
    __BFENV_IOTHREAD_SIGSYNC
);

extern int
bfenv_iothread_append(bfenv_iothread_t *iothread, bfenv_iothread_request_t request);

extern bfenv_iothread_t *
bfenv_iothread_create(const bfdev_alloc_t *alloc, unsigned int depth, unsigned long flags);

extern void
bfenv_iothread_destory(bfenv_iothread_t *iothread);

static inline int
bfenv_iothread_read(bfenv_iothread_t *iothread, int fd, void *buffer, size_t nbytes)
{
    bfenv_iothread_request_t request;

    request.event = BFENV_IOTHREAD_EVENT_READ;
    request.fd = fd;
    request.buffer = (void *)buffer;
    request.size = nbytes;
    request.error = 0;

    return bfenv_iothread_append(iothread, request);
}

static inline int
bfenv_iothread_write(bfenv_iothread_t *iothread, int fd, const void *buffer, size_t nbytes)
{
    bfenv_iothread_request_t request;

    request.event = BFENV_IOTHREAD_EVENT_WRITE;
    request.fd = fd;
    request.buffer = (void *)buffer;
    request.size = nbytes;
    request.error = 0;

    return bfenv_iothread_append(iothread, request);
}

static inline int
bfenv_iothread_sync(bfenv_iothread_t *iothread, int fd, void *buffer, size_t nbytes)
{
    bfenv_iothread_request_t request;

    request.event = BFENV_IOTHREAD_EVENT_SYNC;
    request.error = 0;

    return bfenv_iothread_append(iothread, request);
}

BFDEV_END_DECLS

#endif /* _BFENV_IOTHREAD_H_ */
