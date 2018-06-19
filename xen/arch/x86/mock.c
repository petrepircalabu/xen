#include <xen/types.h>
#include <xen/lib.h>
#include <xen/mock.h>
#include <xen/sched.h>

static int mock_enable(struct domain *d, struct xen_domctl_mock_op *mop)
{
    char * buffer;

    printk("[DEBUG] %s.\n", __func__);

    if ( !d->mock )
        d->mock = xzalloc(struct mock_domain);
    if ( !d->mock )
    {
        printk("[ERROR] Failed to allocate mock object.\n");
        return -ENOMEM;
    }

    if ( d->mock->page )
    {
        printk("[ERROR] Mock page already allocated.\n");
        return -EBUSY;
    }

    d->mock->page = alloc_xenheap_page();
    if ( !d->mock->page )
    {
        printk("[ERROR] alloc_xenheap_page failed.\n");
        return -ENOMEM;
    }

    d->mock->pg_struct = virt_to_page(d->mock->page);

    mop->alloc.handle = mfn_x(page_to_mfn(d->mock->pg_struct));

    printk("[DEBUG] Handle = 0x%lX.\n", mop->alloc.handle);

    clear_page(d->mock->page);

    buffer = (char*)d->mock->page;
    buffer[0] = 'D';
    buffer[1] = 'E';
    buffer[2] = 'A';
    buffer[3] = 'D';
    buffer[4] = 'B';
    buffer[5] = 'A';
    buffer[6] = 'B';
    buffer[7] = 'E';

    return 0;
}

static int mock_disable(struct domain *d, struct xen_domctl_mock_op *mop)
{
    printk("[DEBUG] %s.\n", __func__);
    if ( !d->mock )
        return 0;

    if ( d->mock->page )
    {
	printk("[DEBUG] free_xenheap_page(d->mock->page)\n");
        free_xenheap_page(d->mock->page);
        d->mock->page = NULL;
    }

    printk("[DEBUG] xfree(d->mock)\n");
    xfree(d->mock);
    d->mock = NULL;

    return 0;
}

int mock_domctl(struct domain *d, struct xen_domctl_mock_op *mop,
        XEN_GUEST_HANDLE_PARAM(xen_domctl_t) u_domctl)
{
    int rc = 0;

    printk("[DEBUG] %s.\n", __func__);

    switch(mop->op)
    {
        case XEN_DOMCTL_MOCK_OP_ALLOC:
            rc = mock_enable(d, mop);
        break;
        case XEN_DOMCTL_MOCK_OP_FREE:
            rc = mock_disable(d, mop);
        break;
        default:
            rc = -EINVAL;
    }
    return rc;
}
