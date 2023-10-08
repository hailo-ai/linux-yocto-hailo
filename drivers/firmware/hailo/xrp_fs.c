// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_fs.h"

#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

static DEFINE_HASHTABLE(xrp_known_files, 10);
static DEFINE_SPINLOCK(xrp_known_files_lock);

struct xrp_known_file {
    void *filp;
    struct hlist_node node;
};

bool xrp_is_known_file(struct file *filp)
{
    bool ret = false;
    struct xrp_known_file *p;

    spin_lock(&xrp_known_files_lock);
    hash_for_each_possible (xrp_known_files, p, node, (unsigned long)filp) {
        if (p->filp == filp) {
            ret = true;
            break;
        }
    }
    spin_unlock(&xrp_known_files_lock);
    return ret;
}

void xrp_add_known_file(struct file *filp)
{
    struct xrp_known_file *p = kmalloc(sizeof(*p), GFP_KERNEL);

    if (!p)
        return;

    p->filp = filp;
    spin_lock(&xrp_known_files_lock);
    hash_add(xrp_known_files, &p->node, (unsigned long)filp);
    spin_unlock(&xrp_known_files_lock);
}

void xrp_remove_known_file(struct file *filp)
{
    struct xrp_known_file *p;
    struct xrp_known_file *pf = NULL;

    spin_lock(&xrp_known_files_lock);
    hash_for_each_possible (xrp_known_files, p, node, (unsigned long)filp) {
        if (p->filp == filp) {
            hash_del(&p->node);
            pf = p;
            break;
        }
    }
    spin_unlock(&xrp_known_files_lock);
    if (pf)
        kfree(pf);
}
