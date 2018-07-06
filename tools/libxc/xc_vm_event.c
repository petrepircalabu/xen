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

int xc_vm_event_control(xc_interface *xch, uint32_t domain_id, unsigned int op,
                        unsigned int mode, uint32_t *port)
{
    DECLARE_DOMCTL;
    int rc;

    domctl.cmd = XEN_DOMCTL_vm_event_op;
    domctl.domain = domain_id;
    domctl.u.vm_event_op.op = op;
    domctl.u.vm_event_op.mode = mode;

    rc = do_domctl(xch, &domctl);
    if ( !rc && port )
        *port = domctl.u.vm_event_op.port;
    return rc;
}

static int xc_vm_event_domctl(int type, unsigned int *domctl)
{
    const unsigned int domctls[VM_EVENT_COUNT] = {
        XEN_DOMCTL_VM_EVENT_OP_PAGING,
        XEN_DOMCTL_VM_EVENT_OP_MONITOR,
        XEN_DOMCTL_VM_EVENT_OP_SHARING
    };

    if ( !domctl || type < 0 || type >= VM_EVENT_COUNT )
        return -EINVAL;


    *domctl = domctls[type];
    return 0;
}

static int xc_vm_event_ring_pfn_param(int type, int *pfn_param)
{
    const int pfn_params[VM_EVENT_COUNT] = {
        HVM_PARAM_PAGING_RING_PFN,
        HVM_PARAM_MONITOR_RING_PFN,
        HVM_PARAM_SHARING_RING_PFN
    };

    if ( !pfn_param || type < 0 || type >= VM_EVENT_COUNT )
        return -EINVAL;

    *pfn_param = pfn_params[type];
    return 0;
}

static int xc_vm_event_ring_frames_param(int type, int *frames_param)
{
    const int frames_params[VM_EVENT_COUNT] = {
        HVM_PARAM_PAGING_RING_FRAMES,
        HVM_PARAM_MONITOR_RING_FRAMES,
        HVM_PARAM_SHARING_RING_FRAMES
    };

    if ( !frames_param || type < 0 || type >= VM_EVENT_COUNT )
        return -EINVAL;

    *frames_param = frames_params[type];
    return 0;
}

static int xc_vm_event_get_ring_page_legacy(xc_interface *xch, uint32_t domain_id,
                                            int type, uint32_t *port, void **ring)
{
    int rc, saved_errno;
    uint64_t pfn;
    xen_pfn_t ring_pfn, mmap_pfn;
    unsigned int mode;
    int param;

    if ( !ring || xc_vm_event_ring_pfn_param(type, &param) )
        return -EINVAL;

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
    *ring = xc_map_foreign_pages(xch, domain_id, PROT_READ | PROT_WRITE,
                                 &mmap_pfn, 1);
    if ( !*ring )
    {
        PERROR("Could not map the ring page\n");
        goto out;
    }

    rc = xc_vm_event_domctl(type, &mode);
    if ( rc != 0 )
    {
        PERROR("Invalid VM_EVENT parameter\n");
        errno = -rc;
        rc = -1;
        goto out;
    }

    rc = xc_vm_event_control(xch, domain_id, XEN_VM_EVENT_ENABLE, mode, port);
    if ( rc != 0 )
    {
        PERROR("Failed to enable vm_event\n");
        goto out;
    }

    /* Remove the ring_pfn from the guest's physmap */
    rc = xc_domain_decrease_reservation_exact(xch, domain_id, 1, 0, &ring_pfn);
    if ( rc != 0 )
    {
        PERROR("Failed to remove ring page from guest physmap");
        goto out;
    }

    return 0;

 out:
    saved_errno = errno;
    if ( *ring )
        xenforeignmemory_unmap(xch->fmem, *ring, 1);
    *ring = NULL;

    errno = saved_errno;

    return rc;
}

static int xc_vm_event_get_ring_pages(xc_interface *xch, uint32_t domain_id,
                                      int type, uint32_t *port, void **ring)
{
    xenforeignmemory_resource_handle *fres = NULL;
    unsigned int mode;
    int rc;
    unsigned long nr_frames = 1;
    int param;

    if ( !ring || xc_vm_event_ring_frames_param(type, &param) )
        return -EINVAL;

    /* Get the ring frames count param */
    rc = xc_hvm_param_get(xch, domain_id, param, &nr_frames);
    if ( rc != 0 )
    {
        PERROR("Failed to get pfn of ring page\n");
        return -EINVAL;
    }

    fres = xenforeignmemory_map_resource(xch->fmem, domain_id, XENMEM_resource_vm_event,
                                         type, 0, nr_frames, ring,
                                         PROT_READ | PROT_WRITE, 0);
    if ( !fres )
    {
        PERROR("%s: xenforeignmemory_map_resource failed: error %d", __func__, errno);
        return -errno;
    }

    rc = xc_vm_event_domctl(type, &mode);
    if ( rc != 0 )
    {
        PERROR("Invalid VM_EVENT parameter\n");
        errno = -rc;
        rc = -1;
        goto err;
    }

    rc = xc_vm_event_control(xch, domain_id, XEN_VM_EVENT_GET_PORT, mode, port);
    if ( rc != 0 )
    {
        PERROR("Failed to get vm_event port\n");
        goto err;
    }

    return 0;

err:
    xenforeignmemory_unmap_resource(xch->fmem, fres);
    return rc;
}

void *xc_vm_event_enable(xc_interface *xch, uint32_t domain_id, int type,
                         uint32_t *port)
{
    void *ring_page = NULL;
    int rc1, rc2, saved_errno;

    if ( !port )
    {
        errno = EINVAL;
        return NULL;
    }

    /* Pause the domain for ring page setup */
    rc1 = xc_domain_pause(xch, domain_id);
    if ( rc1 != 0 )
    {
        PERROR("Unable to pause domain\n");
        return NULL;
    }

    rc1 = xc_vm_event_get_ring_pages(xch, domain_id, type, port, &ring_page);
    if ( rc1 == EOPNOTSUPP )
        rc1 = xc_vm_event_get_ring_page_legacy(xch, domain_id, type, port, &ring_page);

 /*out:*/
    saved_errno = errno;

    rc2 = xc_domain_unpause(xch, domain_id);
    if ( rc1 != 0 || rc2 != 0 )
    {
        if ( rc2 != 0 )
        {
            if ( rc1 == 0 )
                saved_errno = errno;
            PERROR("Unable to unpause domain");
        }

        errno = saved_errno;
    }

    return ring_page;
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
