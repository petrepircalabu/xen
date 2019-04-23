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

#include <xen/sched.h>
#include <xen/event.h>
#include <xen/vm_event.h>
#include <xen/vmap.h>
#include <asm/monitor.h>
#include <asm/vm_event.h>
#include <xsm/xsm.h>

#define to_channels(_ved) container_of((_ved), \
                                        struct vm_event_channels_domain, ved)

struct vm_event_channels_domain
{
    /* VM event domain */
    struct vm_event_domain ved;
    /* shared channels buffer */
    struct vm_event_slot *slots;
    /* the buffer size (number of frames) */
    unsigned int nr_frames;
    /* buffer's mfn list */
    mfn_t mfn[0];
};

static const struct vm_event_ops vm_event_channels_ops;

static void vm_event_channels_free_buffer(struct vm_event_channels_domain *impl)
{
    int i;

    vunmap(impl->slots);
    impl->slots = NULL;

    for ( i = 0; i < impl->nr_frames; i++ )
    {
        struct page_info *pg = mfn_to_page(impl->mfn[i]);

        if ( pg == NULL )
            continue;

        free_shared_domheap_page(pg);
    }
}

static int vm_event_channels_alloc_buffer(struct vm_event_channels_domain *impl)
{
    int i;

    impl->slots = vzalloc(impl->nr_frames * PAGE_SIZE);
    if ( !impl->slots )
        return -ENOMEM;

    for ( i = 0; i < impl->nr_frames; i++ )
        impl->mfn[i] = vmap_to_mfn(impl->slots + i * PAGE_SIZE);

    for ( i = 0; i < impl->nr_frames; i++ )
        share_xen_page_with_guest(mfn_to_page(impl->mfn[i]), impl->ved.d,
                                  SHARE_rw);

    return 0;
}

int vm_event_channels_enable(
    struct domain *d,
    struct xen_domctl_vm_event_op *vec,
    struct vm_event_domain **p_ved)
{
    int rc, i = 0;
    xen_event_channel_notification_t fn = vm_event_notification_fn(vec->type);
    unsigned int nr_frames = PFN_UP(d->max_vcpus * sizeof(struct vm_event_slot));
    struct vm_event_channels_domain *impl;

    if ( *p_ved )
        return -EBUSY;

    impl = _xzalloc(sizeof(struct vm_event_channels_domain) +
                           nr_frames * sizeof(mfn_t),
                    __alignof__(struct vm_event_channels_domain));
    if ( unlikely(!impl) )
        return -ENOMEM;

    spin_lock_init(&impl->ved.lock);

    impl->nr_frames = nr_frames;
    impl->ved.d = d;
    impl->ved.ops = &vm_event_channels_ops;

    rc = vm_event_init_domain(d);
    if ( rc < 0 )
        goto err;

    rc = vm_event_channels_alloc_buffer(impl);
    if ( rc )
        goto err;

    for ( i = 0; i < d->max_vcpus; i++ )
    {
        rc = alloc_unbound_xen_event_channel(d, i, current->domain->domain_id, fn);
        if ( rc < 0 )
            goto err;

        impl->slots[i].port = rc;
        impl->slots[i].state = STATE_VM_EVENT_SLOT_IDLE;
    }

    *p_ved = &impl->ved;

    return 0;

err:
    while ( --i >= 0 )
        evtchn_close(d, impl->slots[i].port, 0);
    xfree(impl);

    return rc;
}

static int vm_event_channels_disable(struct vm_event_domain **p_ved)
{
    struct vcpu *v;
    struct domain *d = (*p_ved)->d;
    struct vm_event_channels_domain *impl = to_channels(*p_ved);
    int i;

    spin_lock(&impl->ved.lock);

    for_each_vcpu( impl->ved.d, v )
    {
        if ( atomic_read(&v->vm_event_pause_count) )
            vm_event_vcpu_unpause(v);
    }

    for ( i = 0; i < impl->ved.d->max_vcpus; i++ )
        evtchn_close(impl->ved.d, impl->slots[i].port, 0);

    vm_event_channels_free_buffer(impl);

    vm_event_cleanup_domain(d);

    spin_unlock(&impl->ved.lock);

    xfree(impl);
    *p_ved = NULL;

    return 0;
}

static bool vm_event_channels_check(struct vm_event_domain *ved)
{
    return to_channels(ved)->slots != NULL;
}

static void vm_event_channels_put_request(struct vm_event_domain *ved,
                                          vm_event_request_t *req)
{
    struct vm_event_channels_domain *impl = to_channels(ved);
    struct vm_event_slot *slot;

    ASSERT( req->vcpu_id >= 0 && req->vcpu_id < ved->d->max_vcpus );

    slot = &impl->slots[req->vcpu_id];

    if ( current->domain != ved->d )
    {
        req->flags |= VM_EVENT_FLAG_FOREIGN;
#ifndef NDEBUG
        if ( !(req->flags & VM_EVENT_FLAG_VCPU_PAUSED) )
            gdprintk(XENLOG_G_WARNING, "d%dv%d was not paused.\n",
                     ved->d->domain_id, req->vcpu_id);
#endif
    }

    req->version = VM_EVENT_INTERFACE_VERSION;

    spin_lock(&impl->ved.lock);
    if ( slot->state != STATE_VM_EVENT_SLOT_IDLE )
    {
        gdprintk(XENLOG_G_WARNING, "The VM event slot for d%dv%d is not IDLE.\n",
                 impl->ved.d->domain_id, req->vcpu_id);
        spin_unlock(&impl->ved.lock);
        return;
    }

    slot->u.req = *req;
    slot->state = STATE_VM_EVENT_SLOT_SUBMIT;
    spin_unlock(&impl->ved.lock);
    notify_via_xen_event_channel(impl->ved.d, slot->port);
}

static int vm_event_channels_get_response(struct vm_event_channels_domain *impl,
                                          struct vcpu *v, vm_event_response_t *rsp)
{
    struct vm_event_slot *slot = &impl->slots[v->vcpu_id];
    int rc = 0;

    ASSERT( slot != NULL );
    spin_lock(&impl->ved.lock);

    if ( slot->state != STATE_VM_EVENT_SLOT_FINISH )
    {
        gdprintk(XENLOG_G_WARNING, "The VM event slot state for d%dv%d is invalid.\n",
                 impl->ved.d->domain_id, v->vcpu_id);
        rc = -1;
        goto out;
    }

    *rsp = slot->u.rsp;
    slot->state = STATE_VM_EVENT_SLOT_IDLE;

out:
    spin_unlock(&impl->ved.lock);

    return rc;
}

static int vm_event_channels_resume(struct vm_event_domain *ved, uint32_t vcpu_id)
{
    vm_event_response_t rsp;
    struct vm_event_channels_domain *impl = to_channels(ved);
    struct vcpu *v = domain_vcpu(ved->d, vcpu_id);

    if ( v == NULL )
        return -EINVAL;

    ASSERT(ved->d != current->domain);

    if ( vm_event_channels_get_response(impl, v, &rsp) ||
         rsp.version != VM_EVENT_INTERFACE_VERSION ||
         rsp.vcpu_id != vcpu_id )
        return -1;

    vm_event_handle_response(ved->d, v, &rsp);

    return 0;
}

static int vm_event_channels_acquire_resource(
    struct vm_event_domain *ved,
    unsigned long frame,
    unsigned int nr_frames,
    xen_pfn_t mfn_list[])
{
    struct vm_event_channels_domain *impl = to_channels(ved);
    int i;

    if ( frame != 0 || nr_frames != impl->nr_frames )
        return -EINVAL;

    for ( i = 0; i < impl->nr_frames; i++ )
        mfn_list[i] = mfn_x(impl->mfn[i]);

    return 0;
}

static const struct vm_event_ops vm_event_channels_ops = {
    .acquire_resource = vm_event_channels_acquire_resource,
    .check = vm_event_channels_check,
    .disable = vm_event_channels_disable,
    .put_request = vm_event_channels_put_request,
    .resume = vm_event_channels_resume,
};

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
