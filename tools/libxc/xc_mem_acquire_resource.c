/******************************************************************************
 *
 * xc_mem_acquire_resource.c
 *
 * Interface to guest resource page acquisition mecanism.
 *
 * Copyright (c) 2018 Bitdefender S.R.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, see <http://www.gnu.org/licenses/>.
 */

#include "xc_private.h"
#include <xen/memory.h>

int xc_mem_acquire_resource_vm_event(xc_interface *xch, uint32_t domain_id,
                                     int param, uint32_t nr, xen_pfn_t* pages)
{
    int rc;
    DECLARE_HYPERCALL_BOUNCE(pages, nr * sizeof(xen_pfn_t),
                             XC_HYPERCALL_BUFFER_BOUNCE_OUT);

    xen_mem_acquire_resource_t res = {
        .domid      = domain_id,
        .type       = XENMEM_resource_vm_event,
        .id         = param,
        .nr_frames  = nr,
        .flags      = 0,
        .frame      = 0,
    };

    if ( xc_hypercall_bounce_pre(xch, pages) )
    {
        PERROR("Could not bounce memory for XENMEM_resource_vm_event");
        return -1;
    }

    set_xen_guest_handle(res.frame_list, pages);

    rc = do_memory_op(xch, XENMEM_acquire_resource, &res, sizeof(res));

    xc_hypercall_bounce_post(xch, pages);

    
    return rc;
}
