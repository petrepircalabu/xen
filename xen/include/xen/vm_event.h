/******************************************************************************
 * vm_event.h
 *
 * Common interface for memory event support.
 *
 * Copyright (c) 2009 Citrix Systems, Inc. (Patrick Colp)
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


#ifndef __VM_EVENT_H__
#define __VM_EVENT_H__

#include <xen/errno.h>
#include <xen/event.h>
#include <xen/spinlock.h>
#include <xen/types.h>
#include <public/vm_event.h>

struct domain;
struct vm_event_domain;
struct xen_domctl_vm_event_op;

struct vm_event_ops
{
    int  (*acquire_resource)(struct vm_event_domain *ved,
                             unsigned long frame, unsigned int nr_frames,
                             xen_pfn_t mfn_list[]);
    bool (*check)(struct vm_event_domain *ved);
    void (*cleanup)(struct vm_event_domain *p_ved);
    int  (*claim_slot)(struct vm_event_domain *ved, bool allow_sleep);
    void (*cancel_slot)(struct vm_event_domain *ved);
    int  (*disable)(struct vm_event_domain **p_ved);
    void (*put_request)(struct vm_event_domain *ved, vm_event_request_t *req);
    int  (*resume)(struct vm_event_domain *ved, uint32_t vcpu_id);
};

struct vm_event_domain
{
    /* Domain reference */
    struct domain *d;

    /* vm_event_ops */
    const struct vm_event_ops *ops;

    /* vm_event domain lock */
    spinlock_t lock;
};

/* Clean up on domain destruction */
void vm_event_cleanup(struct domain *d);

/* Returns whether the VM event domain has been set up */
static inline bool vm_event_check(struct vm_event_domain *ved)
{
    return (ved) && ved->ops->check(ved);
}

/* Returns 0 on success, -ENOSYS if there is no ring, -EBUSY if there is no
 * available space and the caller is a foreign domain. If the guest itself
 * is the caller, -EBUSY is avoided by sleeping on a wait queue to ensure
 * that the ring does not lose future events.
 *
 * However, the allow_sleep flag can be set to false in cases in which it is ok
 * to lose future events, and thus -EBUSY can be returned to guest vcpus
 * (handle with care!).
 *
 * In general, you must follow a claim_slot() call with either put_request() or
 * cancel_slot(), both of which are guaranteed to
 * succeed.
 */
static inline int __vm_event_claim_slot(struct vm_event_domain *ved, bool allow_sleep)
{
    if ( !vm_event_check(ved) )
        return -EOPNOTSUPP;

    if ( ved->ops->claim_slot == NULL )
        return 0;

    return ved->ops->claim_slot(ved, allow_sleep);
}

static inline int vm_event_claim_slot(struct vm_event_domain *ved)
{
    return __vm_event_claim_slot(ved, true);
}

static inline int vm_event_claim_slot_nosleep(struct vm_event_domain *ved)
{
    return __vm_event_claim_slot(ved, false);
}

static inline void vm_event_cancel_slot(struct vm_event_domain *ved)
{
    if ( !vm_event_check(ved) || ved->ops->cancel_slot == NULL )
        return;

    ved->ops->cancel_slot(ved);
}

static inline void vm_event_put_request(struct vm_event_domain *ved,
                                        vm_event_request_t *req)
{
    if ( !vm_event_check(ved) )
        return;

    ved->ops->put_request(ved, req);
}

int vm_event_domctl(struct domain *d, struct xen_domctl_vm_event_op *vec);

void vm_event_vcpu_pause(struct vcpu *v);
void vm_event_vcpu_unpause(struct vcpu *v);

void vm_event_fill_regs(vm_event_request_t *req);
void vm_event_set_registers(struct vcpu *v, vm_event_response_t *rsp);

void vm_event_monitor_next_interrupt(struct vcpu *v);

int vm_event_channels_enable(struct domain *d,
                             struct xen_domctl_vm_event_op *vec,
                             struct vm_event_domain **p_ved);

int vm_event_acquire_resource(struct domain *d, unsigned int id,
                              unsigned long frame, unsigned int nr_frames,
                              xen_pfn_t mfn_list[]);

void vm_event_handle_response(struct domain *d, struct vcpu *v,
                              vm_event_response_t *rsp);

xen_event_channel_notification_t vm_event_notification_fn(uint32_t type);

#endif /* __VM_EVENT_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
