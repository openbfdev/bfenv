#define MODULE_NAME "eproc-core"
#define bfdev_log_fmt(fmt) MODULE_NAME ": " fmt

#include <bfenv/eproc.h>
#include <bfdev/log.h>
#include <bfdev/bug.h>
#include <string.h>
#include <export.h>

static
BFDEV_LIST_HEAD(eproc_funcs);

static bool
eproc_funcs_exist(bfenv_eproc_func_t *func)
{
    bfenv_eproc_func_t *walk;

    bfdev_list_for_each_entry(walk, &eproc_funcs, list) {
        if (func == walk)
            return true;
    }

    return false;
}

static bfenv_eproc_func_t *
eproc_funcs_find(const char *name)
{
    bfenv_eproc_func_t *walk;

    bfdev_list_for_each_entry(walk, &eproc_funcs, list) {
        if (!strcmp(walk->name, name))
            return walk;
    }

    return NULL;
}

#include "times.c"
#include "timer.c"
#include "event.c"

static int
eproc_timer_process(bfenv_eproc_t *eproc)
{
    bfenv_eproc_timer_t *timer;
    bfenv_msec_t recent;
    int retval;

    for (;;) {
        eproc_times_update(eproc);
        recent = eproc_timer_timeout(eproc);
        if (recent)
            break;

        timer = eproc_timer_first(eproc);
        BFDEV_BUG_ON(!timer);

        bfenv_eproc_timer_remove(eproc, timer);
        retval = timer->func(timer, timer->pdata);
        if (retval)
            return retval;
    }

    return -BFDEV_ENOERR;
}

static int
eproc_event_process(bfenv_eproc_t *eproc)
{
    bfenv_eproc_event_t *event;
    int retval;

    for (;;) {
        event = eproc_event_first(eproc);
        if (!event)
            break;

        eproc_event_remove(eproc, event);
        retval = event->func(event, event->pdata);
        if (retval)
            return retval;
    }

    return -BFDEV_ENOERR;
}

export int
bfenv_eproc_run(bfenv_eproc_t *eproc, bfenv_msec_t timeout)
{
    bfenv_eproc_func_t *func;
    bfenv_msec_t start, recent;
    int retval;

    func = eproc->func;
    for (;;) {
        bfdev_heap_init(&eproc->events);
        eproc_times_update(eproc);
        recent = bfdev_min(eproc_timer_timeout(eproc), timeout);

        start = eproc->current_msec;
        retval = func->fetch_events(eproc, recent);
        if (bfdev_unlikely(retval))
            return retval;

        retval = eproc_timer_process(eproc);
        if (retval)
            return retval;

        retval = eproc_event_process(eproc);
        if (retval)
            return retval;

        if (timeout != BFENV_TIMEOUT_MAX) {
            bfenv_msec_t time;

            eproc_times_update(eproc);
            time = eproc->current_msec - start;
            if (time >= timeout)
                break;

            timeout -= time;
        }
    }

    return -BFDEV_ENOERR;
}

export bfenv_eproc_t *
bfenv_eproc_create(const bfdev_alloc_t *alloc, const char *name)
{
    bfenv_eproc_func_t *func;
    bfenv_eproc_t *eproc;

    func = eproc_funcs_find(name);
    if (bfdev_unlikely(!func))
        return NULL;

    eproc = func->create(alloc);
    if (bfdev_unlikely(!eproc))
        return NULL;

    eproc->alloc = alloc;
    eproc->func = func;

    bfdev_heap_init(&eproc->timers);
    bfdev_rb_init(&eproc->processes);
    bfdev_rb_init(&eproc->signals);

    return eproc;
}

export void
bfenv_eproc_destory(bfenv_eproc_t *eproc)
{
    bfenv_eproc_func_t *func;

    func = eproc->func;
    func->destory(eproc);
}

export int
bfenv_eproc_func_register(bfenv_eproc_func_t *func)
{
    if (eproc_funcs_find(func->name))
        return -BFDEV_EALREADY;

    bfdev_list_add(&eproc_funcs, &func->list);

    return -BFDEV_ENOERR;
}

export void
bfenv_eproc_func_unregister(bfenv_eproc_func_t *func)
{
    if (!eproc_funcs_exist(func))
        return;

    bfdev_list_del(&func->list);
}
