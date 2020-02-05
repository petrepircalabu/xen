/******************************************************************************
 * vm_event_common.c
 *
 * Implementation independent vm_event helper functions.
 *
 * Copyright (c) 2019 Bitdefender S.R.L.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __VM_EVENT_COMMON_H__
#define __VM_EVENT_COMMON_H__

#include <xen/sched.h>

int vm_event_can_be_enabled(struct domain *d, uint32_t type);

static inline bool vm_event_type_is_supported(uint32_t type)
{
    switch ( type )
    {
#ifdef CONFIG_HAS_MEM_PAGING
        case XEN_VM_EVENT_TYPE_PAGING:
#endif            
        case XEN_VM_EVENT_TYPE_MONITOR:
#ifdef CONFIG_HAS_MEM_SHARING
        case XEN_VM_EVENT_TYPE_SHARING:
#endif
            return true;
    }

    return false;
}

static inline struct vm_event_domain **vm_event_get_pved(struct domain *d,
                                                         uint32_t type)
{
    switch ( type )
    {
#ifdef CONFIG_HAS_MEM_PAGING
        case XEN_VM_EVENT_TYPE_PAGING:
            return &d->vm_event_paging;
#endif
        case XEN_VM_EVENT_TYPE_MONITOR:
            return &d->vm_event_monitor;
#ifdef CONFIG_HAS_MEM_SHARING
        case XEN_VM_EVENT_TYPE_SHARING:
            return &d->vm_event_sharing;
#endif
    }

    return NULL;
}

#endif /* __VM_EVENT_COMMON_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
