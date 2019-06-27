/******************************************************************************
 *
 * xc_mock.c
 *
 * Interface to mock operations.
 *
 * Copyright (c) 2019, Bitdefender S.R.L
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

int xc_mock_enable(xc_interface *xch, uint32_t domain_id, uint32_t nr_frames,
                   void **p_addr, struct xenforeignmemory_resource_handle **fres)
{
    int rc;
    DECLARE_DOMCTL;

    domctl.cmd = XEN_DOMCTL_mock_op;
    domctl.domain = domain_id;
    domctl.u.mock_op.op = XEN_DOMCTL_MOCK_OP_ENABLE;
    domctl.u.mock_op.nr_frames = nr_frames;

    rc = do_domctl(xch, &domctl);
    if ( rc )
    {
        ERROR("Mock Enable domctl failed, rc = %d", rc);
        return rc;
    }

    *fres = xenforeignmemory_map_resource(xch->fmem, domain_id,
                                          XENMEM_resource_mock,
                                          0, 0, nr_frames, p_addr,
                                          PROT_READ | PROT_WRITE, 0);
    if ( !*fres )
    {
        rc = -errno;
        ERROR("Failed to map mock resource, errno = %d", errno);
        xc_mock_disable(xch, domain_id, fres);
    }

    return rc;
}

int xc_mock_disable(xc_interface *xch, uint32_t domain_id,
                    struct xenforeignmemory_resource_handle **fres)
{
    DECLARE_DOMCTL;

    xenforeignmemory_unmap_resource(xch->fmem, *fres);
    *fres = NULL;

    domctl.cmd = XEN_DOMCTL_mock_op;
    domctl.domain = domain_id;
    domctl.u.mock_op.op = XEN_DOMCTL_MOCK_OP_DISABLE;

    return do_domctl(xch, &domctl);
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
