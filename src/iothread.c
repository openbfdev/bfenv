/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#define MODULE_NAME "bfenv_iothread"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/iothread.h>
#include <bfdev/log.h>
#include <bfdev/bug.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <export.h>

static int
iothread_signal(bfenv_iothread_t *iothread, bfenv_iothread_signal_t signal)
{
    int retval;

    iothread->signal = signal;
    retval = eventfd_write(iothread->event_fd, signal);
    if (bfdev_unlikely(retval))
        return retval;

    return -BFDEV_ENOERR;
}

static void *
iothread_worker(void *pdata)
{
    bfenv_iothread_request_t request;
    bfenv_iothread_t *iothread;
    unsigned long length;
    int retval;

    iothread = pdata;
    retval = 0;

    for (;;) {
        sem_wait(&iothread->work_pending);
        if (bfdev_fifo_check_empty(&iothread->works))
            continue;

        length = bfdev_fifo_get(&iothread->works, &request);
        BFDEV_BUG_ON(length != 1);

        switch (request.work) {
            case bfenv_iothread_WORK_READ:
                retval = read(request.fd, request.buffer, request.size);
                if (bfdev_unlikely(retval < 0))
                    goto failed;

                if (bfenv_iothread_sigread_test(iothread)) {
                    retval = iothread_signal(iothread, BFENV_IOTHREAD_SIGNAL_READ);
                    if (bfdev_unlikely(retval))
                        goto failed;
                }
                break;

            case bfenv_iothread_WORK_WRITE:
                retval = write(request.fd, request.buffer, request.size);
                if (bfdev_unlikely(retval < 0))
                    goto failed;

                if (bfenv_iothread_sigwrite_test(iothread)) {
                    retval = iothread_signal(iothread, BFENV_IOTHREAD_SIGNAL_WRITE);
                    if (bfdev_unlikely(retval))
                        goto failed;
                }
                break;
        }

        continue;

failed:
        pthread_mutex_lock(&iothread->mutex);
        if (bfdev_unlikely(iothread->errno)) {
            retval = -BFDEV_ECANCELED;
            break;
        }

        iothread->errno = retval;
        if (bfenv_iothread_sigerr_test(iothread))
            iothread_signal(iothread, BFENV_IOTHREAD_SIGNAL_ERR);

        pthread_mutex_unlock(&iothread->mutex);
        break;
    }

    return (void *)(intptr_t)retval;
}

static int
iothread_append(bfenv_iothread_t *iothread, bfenv_iothread_request_t request)
{
    unsigned long length;

    length = bfdev_fifo_put(&iothread->works, request);
    if (bfdev_unlikely(length != 1))
        return -BFDEV_ENOMEM;

    return sem_post(&iothread->work_pending);
}

export int
bfenv_iothread_read(bfenv_iothread_t *iothread, int fd, void *buffer, size_t nbytes)
{
    bfenv_iothread_request_t request = {
        .work = bfenv_iothread_WORK_READ,
        .fd = fd,
        .buffer = (void *)buffer,
        .size = nbytes,
    };

    return iothread_append(iothread, request);
}

export int
bfenv_iothread_write(bfenv_iothread_t *iothread, int fd, const void *buffer, size_t nbytes)
{
    bfenv_iothread_request_t request = {
        .work = bfenv_iothread_WORK_WRITE,
        .fd = fd,
        .buffer = (void *)buffer,
        .size = nbytes,
    };

    return iothread_append(iothread, request);
}

export bfenv_iothread_t *
bfenv_iothread_create(const bfdev_alloc_t *alloc, unsigned int depth, unsigned long flags)
{
    bfenv_iothread_t *iothread;
    int retval;

    iothread = bfdev_zalloc(alloc, sizeof(*iothread));
    if (bfdev_unlikely(!iothread)) {
        bfdev_log_err("failed to alloc iothread\n");
        return NULL;
    }

    retval = bfdev_fifo_alloc(&iothread->works, alloc, depth);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to alloc write fifo\n");
        goto failed_free_alloc;
    }

    iothread->event_fd = eventfd(0, 0);
    if (bfdev_unlikely(iothread->event_fd < 0)) {
        bfdev_log_err("failed to create eventfd\n");
        goto failed_free_fifo;
    }

    retval = sem_init(&iothread->work_pending, 0, 0);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to init read sem\n");
        goto failed_free_eventfd;
    }

    retval = pthread_mutex_init(&iothread->mutex, NULL);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to init mutex\n");
        goto failed_free_sem;
    }

    iothread->alloc = alloc;
    iothread->flags = flags;

    retval = pthread_create(&iothread->worker_thread, NULL, iothread_worker, iothread);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to create read worker\n");
        goto failed_mutex;
    }

    return iothread;

failed_mutex:
    pthread_mutex_destroy(&iothread->mutex);
failed_free_sem:
    sem_destroy(&iothread->work_pending);
failed_free_eventfd:
    close(iothread->event_fd);
failed_free_fifo:
    bfdev_fifo_free(&iothread->works);
failed_free_alloc:
    bfdev_free(alloc, iothread);
    return NULL;
}

extern void
bfenv_iothread_destory(bfenv_iothread_t *iothread)
{
    const bfdev_alloc_t *alloc;

    alloc = iothread->alloc;
    pthread_cancel(iothread->worker_thread);
    pthread_mutex_destroy(&iothread->mutex);
    sem_destroy(&iothread->work_pending);
    close(iothread->event_fd);

    bfdev_fifo_free(&iothread->works);
    bfdev_free(alloc, iothread);
}
