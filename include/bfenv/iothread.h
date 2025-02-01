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
#include <bfdev/bitflags.h>

BFDEV_BEGIN_DECLS

typedef struct bfenv_iothread bfenv_iothread_t;
typedef struct bfenv_iothread_request bfenv_iothread_request_t;
typedef enum bfenv_iothread_signal bfenv_iothread_signal_t;
typedef enum bfenv_iothread_flags bfenv_iothread_flags_t;
typedef enum bfenv_iothread_event bfenv_iothread_event_t;

enum bfenv_iothread_flags {
    __BFENV_IOTHREAD_FLAGS_SIGREAD = 0,
    __BFENV_IOTHREAD_FLAGS_SIGWRITE,

    BFENV_IOTHREAD_FLAGS_SIGREAD = BFDEV_BIT(__BFENV_IOTHREAD_FLAGS_SIGREAD),
    BFENV_IOTHREAD_FLAGS_SIGWRITE = BFDEV_BIT(__BFENV_IOTHREAD_FLAGS_SIGWRITE),
};

enum bfenv_iothread_event {
    BFENV_IOTHREAD_EVENT_READ,
    BFENV_IOTHREAD_EVENT_WRITE,
};

struct bfenv_iothread_request {
    bfenv_iothread_event_t event;
    int error;
    int fd;

    void *buffer;
    size_t size;
};

struct bfenv_iothread {
    const bfdev_alloc_t *alloc;
    unsigned long flags;
    int event_fd;

    pthread_t worker_thread;
    pthread_mutex_t mutex;
    sem_t work_pending;

    BFDEV_DECLARE_FIFO_DYNAMIC(pending_works, bfenv_iothread_request_t);
    BFDEV_DECLARE_FIFO_DYNAMIC(done_works, bfenv_iothread_request_t);
};

BFDEV_BITFLAGS_STRUCT(
    bfenv_iothread_sigread,
    bfenv_iothread_t, flags,
    __BFENV_IOTHREAD_FLAGS_SIGREAD
);

BFDEV_BITFLAGS_STRUCT(
    bfenv_iothread_sigwrite,
    bfenv_iothread_t, flags,
    __BFENV_IOTHREAD_FLAGS_SIGWRITE
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
    bfenv_iothread_request_t request = {
        .event = BFENV_IOTHREAD_EVENT_READ,
        .fd = fd,
        .buffer = (void *)buffer,
        .size = nbytes,
    };

    return bfenv_iothread_append(iothread, request);
}

static inline int
bfenv_iothread_write(bfenv_iothread_t *iothread, int fd, const void *buffer, size_t nbytes)
{
    bfenv_iothread_request_t request = {
        .event = BFENV_IOTHREAD_EVENT_WRITE,
        .fd = fd,
        .buffer = (void *)buffer,
        .size = nbytes,
    };

    return bfenv_iothread_append(iothread, request);
}
BFDEV_END_DECLS

#endif /* _BFENV_IOTHREAD_H_ */
