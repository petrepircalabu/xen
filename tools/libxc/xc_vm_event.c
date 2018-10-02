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
    domctl.u.vm_event_op.mode = type;

    rc = do_domctl(xch, &domctl);
    if ( rc != 0 )
    {
        PERROR("Failed to enable vm_event\n");
        goto out;
    }

    *port = domctl.u.vm_event_op.port;

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

void *xc_vm_event_enable_ex(xc_interface *xch, uint32_t domain_id, int type,
                            int order, uint32_t *port)
{
    xenforeignmemory_resource_handle *fres = NULL;
    int saved_errno;
    void *ring_buffer = NULL;

    if ( !port )
    {
        errno = EINVAL;
        return NULL;
    }

    /* Pause the domain for ring page setup */
    if ( xc_domain_pause(xch, domain_id) )
    {
        PERROR("Unable to pause domain\n");
        return NULL;
    }

    fres = xenforeignmemory_map_resource(xch->fmem, domain_id,
                                         XENMEM_resource_vm_event_ring, type, 0,
                                         order, &ring_buffer,
                                         PROT_READ | PROT_WRITE, 0);
    if ( !fres )
    {
        PERROR("Unable to map vm_event ring pages resource\n");
        goto out;
    }

    if ( xc_vm_event_control(xch, domain_id, XEN_VM_EVENT_GET_PORT, type, port) )
        PERROR("Unable to get vm_event channel port\n");

out:
    saved_errno = errno;
    if ( xc_domain_unpause(xch, domain_id) != 0 )
    {
        if (fres)
            saved_errno = errno;
        PERROR("Unable to unpause domain");
    }

    free(fres);
    errno = saved_errno;
    return ring_buffer;
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
