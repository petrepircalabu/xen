/******************************************************************************
 * domstate_notify.c
 *
 * Domain state change notification.
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

#include <xen/hypercall.h>
#include <xen/guest_access.h>
#include <public/domstate_notify.h>

long do_domstate_notify_op(unsigned int cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    long ret = 0;

    printk("%s: cmd=%d\n", __func__, cmd);

    switch ( cmd )
    {
        case XEN_DOMSTATE_NOTIFY_register:
        case XEN_DOMSTATE_NOTIFY_unregister:
        case XEN_DOMSTATE_NOTIFY_enable:
        case XEN_DOMSTATE_NOTIFY_disable:
            break;

        default:
            ret = -EINVAL;
    }

    return ret;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
