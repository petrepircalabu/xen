/******************************************************************************
 *
 * xc_vm_event.c
 *
 * Interface to low-level memory event functionality.
 *
 * Copyright (c) 2009 Citrix Systems, Inc. (Patrick Colp)
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
#include "xenforeignmemory.h"

#include <xen/vm_event.h>

#ifndef PFN_UP
#define PFN_UP(x)     (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#endif /* PFN_UP */

int xc_vm_event_control(xc_interface *xch, uint32_t domain_id, unsigned int op,
                        unsigned int type)
{
    DECLARE_DOMCTL;

    domctl.cmd = XEN_DOMCTL_vm_event_op;
    domctl.domain = domain_id;
    domctl.u.vm_event_op.op = op;
    domctl.u.vm_event_op.type = type;

    return do_domctl(xch, &domctl);
}

static int xc_vm_event_ring_pfn_param(int type, int *param)
{
    if ( !param )
        return -EINVAL;

    switch ( type )
    {
    case XEN_VM_EVENT_TYPE_PAGING:
        *param = HVM_PARAM_PAGING_RING_PFN;
        break;

    case XEN_VM_EVENT_TYPE_MONITOR:
        *param = HVM_PARAM_MONITOR_RING_PFN;
        break;

    case XEN_VM_EVENT_TYPE_SHARING:
        *param = HVM_PARAM_SHARING_RING_PFN;
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

void *xc_vm_event_enable(xc_interface *xch, uint32_t domain_id, int type,
                         uint32_t *port)
{
    void *ring_page = NULL;
    uint64_t pfn;
    xen_pfn_t ring_pfn, mmap_pfn;
    int param, rc;
    DECLARE_DOMCTL;

    if ( !port || xc_vm_event_ring_pfn_param(type, &param) != 0 )
    {
        errno = EINVAL;
        return NULL;
    }

    /* Get the pfn of the ring page */
    rc = xc_hvm_param_get(xch, domain_id, param, &pfn);
    if ( rc != 0 )
    {
        PERROR("Failed to get pfn of ring page\n");
        goto out;
    }

    ring_pfn = pfn;
    mmap_pfn = pfn;
    rc = xc_get_pfn_type_batch(xch, domain_id, 1, &mmap_pfn);
    if ( rc || mmap_pfn & XEN_DOMCTL_PFINFO_XTAB )
    {
        /* Page not in the physmap, try to populate it */
        rc = xc_domain_populate_physmap_exact(xch, domain_id, 1, 0, 0,
                                              &ring_pfn);
        if ( rc != 0 )
        {
            PERROR("Failed to populate ring pfn\n");
            goto out;
        }
    }

    mmap_pfn = ring_pfn;
    ring_page = xc_map_foreign_pages(xch, domain_id, PROT_READ | PROT_WRITE,
                                     &mmap_pfn, 1);
    if ( !ring_page )
    {
        PERROR("Could not map the ring page\n");
        goto out;
    }

    domctl.cmd = XEN_DOMCTL_vm_event_op;
    domctl.domain = domain_id;
    domctl.u.vm_event_op.op = XEN_VM_EVENT_ENABLE;
    domctl.u.vm_event_op.type = type;

    rc = do_domctl(xch, &domctl);
    if ( rc != 0 )
    {
        PERROR("Failed to enable vm_event\n");
        goto out;
    }

    *port = domctl.u.vm_event_op.u.enable.port;

    /* Remove the ring_pfn from the guest's physmap */
    rc = xc_domain_decrease_reservation_exact(xch, domain_id, 1, 0, &ring_pfn);
    if ( rc != 0 )
        PERROR("Failed to remove ring page from guest physmap");

 out:
    if ( rc != 0 )
    {
        if ( ring_page )
            xenforeignmemory_unmap(xch->fmem, ring_page, 1);
        ring_page = NULL;
    }

    return ring_page;
}

struct xenforeignmemory_resource_handle;

xenforeignmemory_resource_handle *xc_vm_event_enable_ex(xc_interface *xch,
    uint32_t domain_id, int type,
    void **_ring_buffer, uint32_t ring_frames, uint32_t *ring_port,
    void **_sync_buffer, uint32_t *sync_ports, uint32_t nr_sync_channels)
{
    DECLARE_DOMCTL;
    DECLARE_HYPERCALL_BOUNCE(sync_ports, nr_sync_channels * sizeof(uint32_t),
                             XC_HYPERCALL_BUFFER_BOUNCE_OUT);
    xenforeignmemory_resource_handle *fres;
    unsigned long nr_frames;
    void *buffer;

    if ( !_ring_buffer || !ring_port || !_sync_buffer || !sync_ports )
    {
        errno = EINVAL;
        return NULL;
    }

    /* FIXME: Compute this properly */
    nr_frames = ring_frames + PFN_UP(nr_sync_channels * sizeof(vm_event_request_t));

    fres = xenforeignmemory_map_resource(xch->fmem, domain_id,
                                         XENMEM_resource_vm_event, type, 0,
                                         nr_frames, &buffer,
                                         PROT_READ | PROT_WRITE, 0);
    if ( !fres )
    {
        PERROR("Could not map the vm_event pages\n");
        return NULL;
    }

    domctl.cmd = XEN_DOMCTL_vm_event_op;
    domctl.domain = domain_id;
    domctl.u.vm_event_op.op = XEN_VM_EVENT_GET_PORTS;
    domctl.u.vm_event_op.type = type;

    if ( xc_hypercall_bounce_pre(xch, sync_ports) )
    {
        PERROR("Could not bounce memory for XEN_DOMCTL_vm_event_op");
        errno = ENOMEM;
        return NULL;
    }

    set_xen_guest_handle(domctl.u.vm_event_op.u.get_ports.sync, sync_ports);

    if ( do_domctl(xch, &domctl) )
    {
        PERROR("Failed to get vm_event ports\n");
        goto out;
    }

    xc_hypercall_bounce_post(xch, sync_ports);
    *ring_port = domctl.u.vm_event_op.u.get_ports.async;

    *_sync_buffer = buffer + ring_frames * PAGE_SIZE;
    *_ring_buffer = buffer;

    return fres;

out:
    xc_hypercall_bounce_post(xch, sync_ports);
    if ( fres )
        xenforeignmemory_unmap_resource(xch->fmem, fres);
    return NULL;
}


/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
