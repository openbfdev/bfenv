/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#ifndef _BFENV_EPROC_H_
#define _BFENV_EPROC_H_

#include <bfenv/config.h>
#include <bfenv/types.h>
#include <time.h>

BFDEV_BEGIN_DECLS

typedef struct bfenv_eproc bfenv_eproc_t;
typedef struct bfenv_eproc_func bfenv_eproc_func_t;
typedef struct bfenv_eproc_event bfenv_eproc_event_t;
typedef struct bfenv_eproc_timer bfenv_eproc_timer_t;

typedef int
(*bfenv_eproc_event_cb_t)(bfenv_eproc_event_t *event, void *pdata);

typedef int
(*bfenv_eproc_timer_cb_t)(bfenv_eproc_timer_t *timer, void *pdata);

enum bfenv_eproc_type {
    __BFENV_EPROC_READ = 0,
    __BFENV_EPROC_WRITE,
    __BFENV_EPROC_EDGE,
    __BFENV_EPROC_BLOCKING,

    BFENV_EPROC_READ = BFDEV_BIT(__BFENV_EPROC_READ),
    BFENV_EPROC_WRITE = BFDEV_BIT(__BFENV_EPROC_WRITE),
    BFENV_EPROC_EDGE = BFDEV_BIT(__BFENV_EPROC_EDGE),
    BFENV_EPROC_BLOCKING = BFDEV_BIT(__BFENV_EPROC_BLOCKING),
};

struct bfenv_eproc {
    const bfdev_alloc_t *alloc;
    bfenv_eproc_func_t *func;
    bfenv_msec_t current_msec;

    bfdev_list_head_t pending;
    bfdev_heap_root_t timers;
    bfdev_rb_root_t processes;
    bfdev_rb_root_t signals;
};

struct bfenv_eproc_event {
    bfdev_list_head_t node;
	unsigned long flags;
	unsigned long events;
    int fd;

    struct bfenv_eproc *eproc;
    bfenv_eproc_event_cb_t func;
    void *pdata;
};

struct bfenv_eproc_timer {
    bfdev_heap_node_t node;
    time_t time;
    bool pending;

    struct bfenv_eproc *eproc;
    bfenv_eproc_timer_cb_t func;
    void *pdata;
};

struct bfenv_eproc_func {
    const char *name;
    bfdev_list_head_t list;

    int (*fetch_events)(bfenv_eproc_t *eproc, bfenv_msec_t timeout);
    int (*event_register)(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event);
    void (*event_unregister)(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event);

    bfenv_eproc_t *(*create)(const bfdev_alloc_t *alloc);
    void (*destory)(bfenv_eproc_t *eproc);
};

BFDEV_BITFLAGS(bfenv_eproc_read, __BFENV_EPROC_READ)
BFDEV_BITFLAGS(bfenv_eproc_write, __BFENV_EPROC_WRITE)
BFDEV_BITFLAGS(bfenv_eproc_edge, __BFENV_EPROC_EDGE)
BFDEV_BITFLAGS(bfenv_eproc_blocking, __BFENV_EPROC_BLOCKING)

static __bfdev_always_inline bool
bfenv_eproc_timer_pending(const bfenv_eproc_timer_t *timer)
{
    return timer->pending;
}

extern int
bfenv_eproc_event_add(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event);

extern void
bfenv_eproc_event_remove(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event);

extern int
bfenv_eproc_timer_add(bfenv_eproc_t *eproc, bfenv_eproc_timer_t *timer, bfenv_msec_t timeout);

extern void
bfenv_eproc_timer_remove(bfenv_eproc_t *eproc, bfenv_eproc_timer_t *timer);

extern int
bfenv_eproc_run(bfenv_eproc_t *eproc, bfenv_msec_t timeout);

extern bfenv_eproc_t *
bfenv_eproc_create(const bfdev_alloc_t *alloc, const char *name);

extern void
bfenv_eproc_destory(bfenv_eproc_t *eproc);

extern int
bfenv_eproc_func_register(bfenv_eproc_func_t *func);

extern void
bfenv_eproc_func_unregister(bfenv_eproc_func_t *func);

BFDEV_END_DECLS

#endif /* _BFENV_EPROC_H_ */
