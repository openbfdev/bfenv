/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#include <bfenv/eproc.h>

static inline long
timer_cmp(const bfdev_heap_node_t *key1,
          const bfdev_heap_node_t *key2, void *pdata)
{
    bfenv_eproc_timer_t *node1, *node2;

    node1 = bfdev_heap_entry(key1, bfenv_eproc_timer_t, node);
    node2 = bfdev_heap_entry(key2, bfenv_eproc_timer_t, node);

    /* Ignoring conflicts */
    return bfdev_cmp(node1->time > node2->time);
}

static bfenv_eproc_timer_t *
eproc_timer_first(bfenv_eproc_t *eproc)
{
    bfdev_heap_node_t *node;
    bfenv_eproc_timer_t *timer;

    node = BFDEV_HEAP_ROOT_NODE(&eproc->timers);
    timer = bfdev_heap_entry_safe(node, bfenv_eproc_timer_t, node);

    return timer;
}

static void
eproc_timer_add(bfenv_eproc_t *eproc, bfenv_eproc_timer_t *timer, bfenv_msec_t timeout)
{
    timer->pending = true;
    timer->time = eproc->current_msec + timeout;
    bfdev_heap_insert(&eproc->timers, &timer->node, timer_cmp, NULL);
}

static void
eproc_timer_remove(bfenv_eproc_t *eproc, bfenv_eproc_timer_t *timer)
{
    timer->pending = false;
    bfdev_heap_remove(&eproc->timers, &timer->node);
}

static bfenv_msec_t
eproc_timer_timeout(bfenv_eproc_t *eproc)
{
    bfenv_eproc_timer_t *timer;

    timer = eproc_timer_first(eproc);
    if (!timer)
        return BFENV_TIMEOUT_MAX;

    if (timer->time <= eproc->current_msec)
        return 0;

    return timer->time - eproc->current_msec;
}
