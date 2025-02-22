/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#define MODULE_NAME "bfenv-iothread"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/iothread.h>
#include <bfdev/log.h>
#include <bfdev/bug.h>
#include <bfdev/log2.h>
#include <bfdev/minmax.h>
#include <unistd.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <export.h>

static int
iothread_signal_write(bfenv_iothread_t *iothread,
                      bfenv_iothread_request_t request)
{
    unsigned long count;
    int retval;

    count = bfdev_fifo_put(&iothread->done_works, request);
    BFDEV_BUG_ON(count != 1);

    retval = eventfd_write(iothread->eventfd, 1);
    if (bfdev_unlikely(retval < 0))
        return retval;

    return -BFDEV_ENOERR;
}

static int
iothread_signal_flush(bfenv_iothread_t *iothread)
{
    bfenv_iothread_request_t *peek;
    int retval;

    for (;;) {
        if (bfdev_fifo_check_full(&iothread->done_works))
            break;

        peek = bfdev_list_last_entry_or_null(
            &iothread->pending_dones,
            bfenv_iothread_request_t, pending
        );
        if (bfdev_unlikely(!peek))
            break;

        retval = iothread_signal_write(iothread, *peek);
        if (bfdev_unlikely(retval))
            return retval;

        bfdev_list_del(&peek->pending);
        bfdev_free(iothread->alloc, peek);
    }

    return -BFDEV_ENOERR;
}

static int
iothread_signal(bfenv_iothread_t *iothread, bfenv_iothread_request_t request)
{
    bfenv_iothread_request_t *pending;
    int retval;

    pending = bfdev_malloc(iothread->alloc, sizeof(*pending));
    if (bfdev_unlikely(!pending))
        return -BFDEV_ENOMEM;

    *pending = request;
    bfdev_list_add(&iothread->pending_dones, &pending->pending);

    retval = iothread_signal_flush(iothread);
    if (bfdev_unlikely(retval))
        return retval;

    return -BFDEV_ENOERR;
}

static int
iothread_wait(bfenv_iothread_t *iothread, struct timespec *time)
{
    int retval;

    if (bfdev_list_check_empty(&iothread->pending_dones))
        retval = sem_wait(&iothread->pending);
    else
        retval = sem_timedwait(&iothread->pending, time);

    if (bfdev_unlikely(retval < 0)) {
        if (errno != ETIMEDOUT)
            return errno;
    }

    retval = iothread_signal_flush(iothread);
    if (bfdev_unlikely(retval))
        return retval;

    return -BFDEV_ENOERR;
}

static void *
iothread_worker(void *pdata)
{
    bfenv_iothread_request_t request;
    bfenv_iothread_t *iothread;
    struct timespec polltime;
    unsigned long avail;
    ssize_t length;
    int retval;

    iothread = pdata;
    retval = 0;

    polltime.tv_sec = BFENV_IOTHREAD_POLLMS / 1000;
    polltime.tv_nsec = (BFENV_IOTHREAD_POLLMS % 1000) * 1000000;

    for (;;) {
        retval = iothread_wait(iothread, &polltime);
        if (bfdev_unlikely(retval)) {
            bfdev_log_notice("worker wait failed: %d\n", retval);
            goto eexit;
        }

        avail = bfdev_fifo_get(&iothread->pending_works, &request);
        if (!avail)
            continue;

        switch (request.event) {
            case BFENV_IOTHREAD_EVENT_READ: {
                for (;;) {
                    length = read(request.fd, request.buffer, request.size);
                    if (bfdev_unlikely(length < 0)) {
                        if (errno == EINTR)
                            continue;

                        bfdev_log_notice("worker read failed: %d\n", errno);
                        retval = errno;
                        goto failed;
                    }

                    break;
                }

                request.size = length;
                if (bfenv_iothread_sigread_test(iothread)) {
                    retval = iothread_signal(iothread, request);
                    if (bfdev_unlikely(retval)) {
                        bfdev_log_notice("worker send read signal failed: %d\n", errno);
                        goto eexit;
                    }
                }
                break;
            }

            case BFENV_IOTHREAD_EVENT_WRITE: {
                void *buffer;
                size_t count;

                count = 0;
                buffer = request.buffer;

                do {
                    length = write(request.fd, buffer, request.size - count);
                    if (bfdev_unlikely(length < 0 && errno != EINTR)) {
                        bfdev_log_notice("worker write failed: %d\n", errno);
                        retval = errno;
                        goto failed;
                    }

                    count += length;
                    buffer += length;
                } while (count < request.size);

                request.size = count;
                if (bfenv_iothread_sigwrite_test(iothread)) {
                    retval = iothread_signal(iothread, request);
                    if (bfdev_unlikely(retval)) {
                        bfdev_log_err("worker send write signal failed: %d\n", retval);
                        goto eexit;
                    }
                }
                break;
            }

            case BFENV_IOTHREAD_EVENT_SYNC:
                for (;;) {
                    retval = fsync(request.fd);
                    if (bfdev_unlikely(retval < 0)) {
                        if (errno == EINTR)
                            continue;

                        bfdev_log_notice("worker sync failed: %d\n", errno);
                        retval = errno;
                        goto failed;
                    }
                }

                if (bfenv_iothread_sigsync_test(iothread)) {
                    retval = iothread_signal(iothread, request);
                    if (bfdev_unlikely(retval)) {
                        bfdev_log_err("worker send sync signal failed: %d\n", retval);
                        goto eexit;
                    }
                }
                break;

            default:
                bfdev_log_notice("worker unknown event: %d\n", request.event);
                retval = -BFDEV_EINVAL;
                goto failed;
        }

        continue;

failed:
        request.error = retval;
        retval = iothread_signal(iothread, request);
        if (bfdev_unlikely(retval)) {
            bfdev_log_err("worker send error signal failed: %d\n", retval);
            goto eexit;
        }
    }

eexit:
    iothread->error = retval;
    return NULL;
}

export int
bfenv_iothread_append(bfenv_iothread_t *iothread, bfenv_iothread_request_t request)
{
    unsigned long length;

    length = bfdev_fifo_put(&iothread->pending_works, request);
    if (bfdev_unlikely(length != 1))
        return -BFDEV_EAGAIN;

    return sem_post(&iothread->pending);
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

    bfdev_max_adj(depth, 2);
    depth = bfdev_pow2_roundup(depth);

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

    iothread->eventfd = eventfd(0, 0);
    if (bfdev_unlikely(iothread->eventfd < 0)) {
        bfdev_log_err("failed to create eventfd\n");
        goto failed_free_done_fifo;
    }

    retval = sem_init(&iothread->pending, 0, 0);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to init read sem\n");
        goto failed_free_eventfd;
    }

    iothread->alloc = alloc;
    iothread->flags = flags;
    bfdev_list_head_init(&iothread->pending_dones);

    retval = pthread_create(&iothread->worker_thread, NULL, iothread_worker, iothread);
    if (bfdev_unlikely(retval)) {
        bfdev_log_err("failed to create read worker\n");
        goto failed_free_sem;
    }

    return iothread;

failed_free_sem:
    sem_destroy(&iothread->pending);
failed_free_eventfd:
    close(iothread->eventfd);
failed_free_done_fifo:
    bfdev_fifo_free(&iothread->done_works);
failed_free_pending_fifo:
    bfdev_fifo_free(&iothread->pending_works);
failed_free_alloc:
    bfdev_free(alloc, iothread);
    return NULL;
}

export void
bfenv_iothread_destory(bfenv_iothread_t *iothread,
                       bfenv_iothread_release_t release, void *pdata)
{
    bfenv_iothread_request_t request, *pending, *tmp;
    const bfdev_alloc_t *alloc;

    pthread_cancel(iothread->worker_thread);
    pthread_join(iothread->worker_thread, NULL);
    sem_destroy(&iothread->pending);
    close(iothread->eventfd);

    alloc = iothread->alloc;
    bfdev_list_for_each_entry_safe(pending, tmp,
        &iothread->pending_dones, pending) {
        if (release)
            release(pending, pdata);
        bfdev_free(alloc, pending);
    }

    while (bfdev_fifo_get(&iothread->pending_works, &request)) {
        if (release)
            release(pending, pdata);
    }

    bfdev_fifo_free(&iothread->done_works);
    bfdev_fifo_free(&iothread->pending_works);
    bfdev_free(alloc, iothread);
}
