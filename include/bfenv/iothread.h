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
typedef enum bfenv_iothread_work bfenv_iothread_work_t;

enum bfenv_iothread_signal {
    BFENV_IOTHREAD_SIGNAL_READ = 1,
    BFENV_IOTHREAD_SIGNAL_WRITE,
    BFENV_IOTHREAD_SIGNAL_ERR,
};

enum bfenv_iothread_flags {
    __BFENV_IOTHREAD_FLAGS_SIGREAD = 0,
    __BFENV_IOTHREAD_FLAGS_SIGWRITE,
    __BFENV_IOTHREAD_FLAGS_SIGERR,

    BFENV_IOTHREAD_FLAGS_SIGREAD    = BFDEV_BIT(__BFENV_IOTHREAD_FLAGS_SIGREAD),
    BFENV_IOTHREAD_FLAGS_SIGWRITE   = BFDEV_BIT(__BFENV_IOTHREAD_FLAGS_SIGWRITE),
    BFENV_IOTHREAD_FLAGS_SIGERR     = BFDEV_BIT(__BFENV_IOTHREAD_FLAGS_SIGERR),
};

enum bfenv_iothread_work {
    bfenv_iothread_WORK_READ = 0,
    bfenv_iothread_WORK_WRITE,
};

struct bfenv_iothread_request {
    bfenv_iothread_work_t work;
    int fd;

    void *buffer;
    size_t size;
};

struct bfenv_iothread {
    const bfdev_alloc_t *alloc;
    int event_fd;

    pthread_t worker_thread;
    pthread_mutex_t mutex;

    BFDEV_DECLARE_FIFO_DYNAMIC(works, bfenv_iothread_request_t);
    sem_t work_pending;

    bfenv_iothread_signal_t signal;
    unsigned long flags;
    int errno;
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

BFDEV_BITFLAGS_STRUCT(
    bfenv_iothread_sigerr,
    bfenv_iothread_t, flags,
    __BFENV_IOTHREAD_FLAGS_SIGERR
);

extern int
bfenv_iothread_read(bfenv_iothread_t *iothread, int fd, void *buf, size_t nbytes);

extern int
bfenv_iothread_write(bfenv_iothread_t *iothread, int fd, const void *buf, size_t nbytes);

extern bfenv_iothread_t *
bfenv_iothread_create(const bfdev_alloc_t *alloc, unsigned int depth, unsigned long flags);

extern void
bfenv_iothread_destory(bfenv_iothread_t *iothread);

BFDEV_END_DECLS

#endif /* _BFENV_IOTHREAD_H_ */
