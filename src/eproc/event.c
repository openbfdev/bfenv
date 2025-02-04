/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2025 John Sanpe <sanpeqf@gmail.com>
 */

#include <bfenv/eproc.h>
#include <sys/fcntl.h>
#include <export.h>

static long
event_cmp(const bfdev_heap_node_t *key1,
          const bfdev_heap_node_t *key2, void *pdata)
{
    int prio1, prio2;

    prio1 = bfdev_container_of(key1, bfenv_eproc_event_t, node)->priority;
    prio2 = bfdev_container_of(key2, bfenv_eproc_event_t, node)->priority;

    if (prio1 == prio2)
        return 0;

    return bfdev_cmp(prio1 > prio2);
}

static bfenv_eproc_event_t *
eproc_event_first(bfenv_eproc_t *eproc)
{
    bfdev_heap_node_t *node;
    bfenv_eproc_event_t *event;

    node = BFDEV_HEAP_ROOT_NODE(&eproc->events);
    event = bfdev_heap_entry_safe(node, bfenv_eproc_event_t, node);

    return event;
}

static void
eproc_event_remove(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    event->pending = false;
    bfdev_heap_remove(&eproc->events, &event->node);
}

export void
bfenv_eproc_event_raise(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    event->pending = true;
    bfdev_heap_insert(&eproc->events, &event->node, event_cmp, NULL);
}

export int
bfenv_eproc_event_add(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    bfenv_eproc_func_t *func;
	unsigned int flags;
    int retval;

    func = eproc->func;
    event->eproc = eproc;

    if (!bfenv_eproc_blocking_test(&event->flags)) {
		flags = fcntl(event->fd, F_GETFL, 0);
		flags |= O_NONBLOCK;
		fcntl(event->fd, F_SETFL, flags);
	}

    retval = func->event_register(eproc, event);
    if (bfdev_unlikely(retval))
        return retval;

    return -BFDEV_ENOERR;
}

export void
bfenv_eproc_event_remove(bfenv_eproc_t *eproc, bfenv_eproc_event_t *event)
{
    bfenv_eproc_func_t *func;

    func = eproc->func;
    func->event_unregister(eproc, event);

    if (event->pending)
        eproc_event_remove(eproc, event);
}
