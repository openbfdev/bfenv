/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#ifndef _BFENV_TYPES_
#define _BFENV_TYPES_

#include <bfenv/config.h>
#include <bfdev/types.h>
#include <limits.h>

BFDEV_BEGIN_DECLS

typedef uint64_t bfenv_msec_t;
#define BFENV_TIMEOUT_MAX UINT64_MAX

BFDEV_END_DECLS

#endif /* _BFENV_TYPES_ */
