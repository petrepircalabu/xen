/******************************************************************************
 * vm_event_ng.c
 *
 * VM event support (new generation).
 *
 * Copyright (c) 2019, Bitdefender S.R.L.
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

#include <xen/guest_access.h>
#include <xen/hypercall.h>
#include <xen/sched.h>
#include <xen/vm_event.h>

#include <public/vm_event_op.h>

long do_vm_event_op(domid_t domid, unsigned int cmd, unsigned int type,
                    XEN_GUEST_HANDLE_PARAM(void) arg)
{
    long rc;

    gdprintk(XENLOG_DEBUG, "vm_event_op: domid = %d cmd = %d, type = %d \n",
             domid, cmd, type);

    rc = 0;

    switch ( cmd )
    {
    case XEN_VM_EVENT_OP_enable:
    {
        struct xen_vm_event_op_enable enable_op;

        if ( copy_from_guest(&enable_op, arg, 1) )
        {
            rc = -EFAULT;
            break;
        }

        rc = __copy_to_guest(arg, &enable_op, 1) ? -EFAULT : 0;
        break;
    }

    case XEN_VM_EVENT_OP_disable:
    {
        break;
    }

    default:
        rc = -ENOSYS;
        break;
    }

    return rc;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
