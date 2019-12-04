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


#include <xen/event.h>
#include <xen/guest_access.h>
#include <xen/hypercall.h>
#include <xen/notifier.h>
#include <public/domstate_notify.h>

/* for public/io/ring.h macros */
#define xen_mb()   smp_mb()
#define xen_rmb()  smp_rmb()
#define xen_wmb()  smp_wmb()

struct domstate_observer
{
    struct domain *d;
    void *ring_page;
    struct page_info *ring_pg_struct;
    domstate_notify_front_ring_t front_ring;
    struct list_head list;
};

static LIST_HEAD(domstate_observers_list);
/*
 * FIXME: TODO figure out locking
static DEFINE_SPINLOCK(domstate_observers_lock);
static DEFINE_RCU_READ_LOCK(rcu_domstate_observers_lock);
 */

const struct domstate_observer * find_domstate_observer(struct domain *d)
{
    const struct domstate_observer *obs;

    list_for_each_entry( obs, &domstate_observers_list, list )
    {
        if ( obs->d->domain_id == d->domain_id )
            return obs;
    }
    return NULL;
}

static int domstate_notify_register(struct domstate_notify_register *reg)
{
    int rc = 0;
    struct domain *d = current->domain;
    struct domstate_observer *obs;

    if ( d->observer != NULL )
        return -EBUSY;

    obs = xzalloc(struct domstate_observer);
    if ( obs == NULL )
        return -ENOMEM;

    rc = prepare_ring_for_helper(d, reg->page_gfn, &obs->ring_pg_struct,
                                 &obs->ring_page);
    if ( rc < 0 )
        goto err;

    FRONT_RING_INIT(&obs->front_ring,
                    (domstate_notify_sring_t *)obs->ring_page,
                    PAGE_SIZE);

    obs->d = d;
    list_add_tail_rcu(&obs->list, &domstate_observers_list);
    d->observer = obs;

    return 0;

err:
    destroy_ring_for_helper(&obs->ring_page, obs->ring_pg_struct);
    xfree(obs);

    return rc;
}

static int domstate_notify_unregister(void)
{
    struct domain *d = current->domain;
    struct domstate_observer *obs = d->observer;

    if ( obs == NULL )
        return -EINVAL;

    list_del_rcu(&obs->list);

    destroy_ring_for_helper(&obs->ring_page, obs->ring_pg_struct);

    xfree(obs);
    d->observer = NULL;

    return 0;
}

long do_domstate_notify_op(unsigned int cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    long rc = 0;

    switch ( cmd )
    {
    case XEN_DOMSTATE_NOTIFY_register:
    {
        struct domstate_notify_register reg;

        if ( copy_from_guest(&reg, arg, 1) != 0 )
            return -EFAULT;
        rc = domstate_notify_register(&reg);
        if ( !rc && __copy_to_guest(arg, &reg, 1) )
            rc = -EFAULT;

        break;
    }

    case XEN_DOMSTATE_NOTIFY_unregister:
        rc = domstate_notify_unregister();
        break;

    case XEN_DOMSTATE_NOTIFY_enable:
    case XEN_DOMSTATE_NOTIFY_disable:
        break;

    default:
        rc = -EINVAL;
    }

    return rc;
}

static int domstate_notify_dispatch(struct domstate_observer *obs,
                                    struct domain *d, int state)
{
    domstate_notify_front_ring_t *front_ring;
    int free_req;
    RING_IDX req_prod;
    domstate_notify_event_t evt;

    evt.version = 1;
    evt.domain_id = d->domain_id;
    evt.state = state;
    evt.extra = 0;

    front_ring = &obs->front_ring;
    free_req = RING_FREE_REQUESTS(front_ring);

    if ( free_req <= 0 )
        return -EBUSY;

    req_prod = front_ring->req_prod_pvt;
    memcpy(RING_GET_REQUEST(front_ring, req_prod), &evt, sizeof(evt));
    req_prod++;

    /* Update ring */
    front_ring->req_prod_pvt = req_prod;
    RING_PUSH_REQUESTS(front_ring);

    send_guest_global_virq(obs->d, VIRQ_DOMSTATE);

    return 0;
}

int domstate_notify(struct domain *d, int state)
{
    struct domstate_observer *obs;
    int rc = 0;

    list_for_each_entry ( obs, &domstate_observers_list, list )
    {
        rc = domstate_notify_dispatch(obs, d, state);
        if ( rc )
            goto exit;
    }

exit:
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
