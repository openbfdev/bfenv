/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#define MODULE_NAME "bfenv-iothread"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/iothread.h>
#include <bfdev/log.h>
#include <bfdev/bug.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <export.h>

static int
iothread_signal(bfenv_iothread_t *iothread, bfenv_iothread_request_t request)
{
    int retval;

    for (;;) {
        if (bfdev_fifo_put(&iothread->done_works, request) == 1)
            break;

        /* TODO: sleep thread */
        sched_yield();
    }

    retval = eventfd_write(iothread->event_fd, 1);
    if (bfdev_unlikely(retval < 0))
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
        if (bfdev_fifo_check_empty(&iothread->pending_works))
            continue;

        length = bfdev_fifo_get(&iothread->pending_works, &request);
        BFDEV_BUG_ON(length != 1);

        switch (request.event) {
            case BFENV_IOTHREAD_EVENT_READ:
                retval = read(request.fd, request.buffer, request.size);
                if (bfdev_unlikely(retval < 0)) {
                    bfdev_log_err("worker read failed: %d\n", retval);
                    goto failed;
                }

                if (bfenv_iothread_sigread_test(iothread)) {
                    retval = iothread_signal(iothread, request);
                    if (bfdev_unlikely(retval)) {
                        bfdev_log_err("worker send done signal failed: %d\n", retval);
                        goto failed;
                    }
                }
                break;

            case BFENV_IOTHREAD_EVENT_WRITE:
                retval = write(request.fd, request.buffer, request.size);
                if (bfdev_unlikely(retval < 0)) {
                    bfdev_log_err("worker write failed: %d\n", retval);
                    goto failed;
                }

                if (bfenv_iothread_sigwrite_test(iothread)) {
                    retval = iothread_signal(iothread, request);
                    if (bfdev_unlikely(retval)) {
                        bfdev_log_err("worker send write done signal failed: %d\n", retval);
                        goto failed;
                    }
                }
                break;

            default:
                bfdev_log_err("worker unknown event: %d\n", request.event);
                retval = -BFDEV_EINVAL;
                goto failed;
        }

        continue;

failed:
        pthread_mutex_lock(&iothread->mutex);
        request.error = retval;
        retval = iothread_signal(iothread, request);
        BFDEV_BUG_ON(retval);
        pthread_mutex_unlock(&iothread->mutex);
    }

    return 0;
}

export int
bfenv_iothread_append(bfenv_iothread_t *iothread, bfenv_iothread_request_t request)
{
    unsigned long length;

    length = bfdev_fifo_put(&iothread->pending_works, request);
    if (bfdev_unlikely(length != 1))
        return -BFDEV_ENOMEM;

    return sem_post(&iothread->work_pending);
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

    retval = bfdev_fifo_alloc(&iothread->pending_works, alloc, depth);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to alloc pending fifo\n");
        goto failed_free_alloc;
    }

    retval = bfdev_fifo_alloc(&iothread->done_works, alloc, depth);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to alloc done fifo\n");
        goto failed_free_pending_fifo;
    }

    iothread->event_fd = eventfd(0, 0);
    if (bfdev_unlikely(iothread->event_fd < 0)) {
        bfdev_log_err("failed to create eventfd\n");
        goto failed_free_done_fifo;
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
failed_free_done_fifo:
    bfdev_fifo_free(&iothread->done_works);
failed_free_pending_fifo:
    bfdev_fifo_free(&iothread->pending_works);
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

    bfdev_fifo_free(&iothread->done_works);
    bfdev_fifo_free(&iothread->pending_works);
    bfdev_free(alloc, iothread);
}
