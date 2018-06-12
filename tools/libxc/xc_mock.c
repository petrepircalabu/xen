#include "xc_private.h"

void* xc_mock_alloc(xc_interface *xch, uint32_t domain_id)
{
    DECLARE_DOMCTL;
    void *ring_page = NULL;
    xen_pfn_t mmap_pfn;
    int rc;

    DBGPRINTF("%s: .\n", __func__);

    domctl.cmd = XEN_DOMCTL_mock_op;
    domctl.domain = domain_id;
    domctl.u.mock_op.op = XEN_DOMCTL_MOCK_OP_ALLOC;
    domctl.u.mock_op.alloc.size = 0;

    rc = do_domctl(xch, &domctl);
    if ( rc )
    {
        PERROR("%s: do_domctl rc = %d.\n", __func__, rc);
        goto out;
    }

    mmap_pfn = domctl.u.mock_op.alloc.handle;

    ring_page = xc_map_foreign_pages(xch, domain_id, PROT_READ | PROT_WRITE, &mmap_pfn, 1);
    if ( !ring_page )
    {
        PERROR("Could not map the ring page\n");
        goto out;
    }

out:
    return ring_page;
}

void xc_mock_free(xc_interface *xch, uint32_t domain_id)
{
    DECLARE_DOMCTL;
    int rc;

    DBGPRINTF("%s: .\n", __func__);

    domctl.cmd = XEN_DOMCTL_mock_op;
    domctl.domain = domain_id;
    domctl.u.mock_op.op = XEN_DOMCTL_MOCK_OP_FREE;

    rc = do_domctl(xch, &domctl);

    DBGPRINTF("%s: do_domctl rc = %d.\n", __func__, rc);
}
