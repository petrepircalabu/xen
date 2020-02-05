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

#include <xen/sched.h>
#include <xen/vm_event.h>
#include <asm/monitor.h>
#include <asm/p2m.h>

int vm_event_can_be_enabled(struct domain *d, uint32_t type)
{
    switch ( type )
    {
#ifdef CONFIG_HAS_MEM_PAGING
    case XEN_VM_EVENT_TYPE_PAGING:
        /* hvm fixme: p2m_is_foreign types need addressing */
        if ( is_hvm_domain(hardware_domain) )
            return -EOPNOTSUPP;

        /* Only HAP is supported */
        if ( !hap_enabled(d) )
            return -ENODEV;

        /* No paging if iommu is used */
        if ( unlikely(is_iommu_enabled(d)) )
            return -EMLINK;

        /* Disallow paging in a PoD guest */
        if ( p2m_pod_entry_count(p2m_get_hostp2m(d)) )
            return -EXDEV;

        break;
#endif /* CONFIG_HAS_MEM_PAGING */

    case XEN_VM_EVENT_TYPE_MONITOR:
        return arch_monitor_init_domain(d);

#ifdef CONFIG_MEM_SHARING
    case XEN_VM_EVENT_TYPE_SHARING:
        if ( is_hvm_domain(hardware_domain) )
            return -EOPNOTSUPP;

        /* Only HAP is supported */
        if ( !hap_enabled(d) )
            return -ENODEV;

        break;
#endif /* CONFIG_MEM_SHARING */

    default:
        return -ENOSYS;
    }

    return 0;
}

/* Clean up on domain destruction */
void vm_event_cleanup(struct domain *d)
{
#ifdef CONFIG_HAS_MEM_PAGING
    if ( vm_event_check(d->vm_event_paging) )
        d->vm_event_paging->ops->cleanup(&d->vm_event_paging);
#endif

    if ( vm_event_check(d->vm_event_monitor) )
        d->vm_event_monitor->ops->cleanup(&d->vm_event_monitor);

#ifdef CONFIG_MEM_SHARING
    if ( vm_event_check(d->vm_event_share) )
        d->vm_event_share->ops->cleanup(&d->vm_event_share);
#endif
}

int vm_event_has_active_waitqueue(const struct vm_event_domain *ved)
{
    if ( ved == NULL )
        return 0;

    return ved->ops->has_active_waitqueue(ved);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
