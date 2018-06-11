#include "xc_private.h"

void xc_mock_alloc(xc_interface *xch, uint32_t domain_id)
{
    DECLARE_DOMCTL;
    int rc;

    domctl.cmd = XEN_DOMCTL_mock_op;
    domctl.domain = domain_id;
    domctl.u.mock_op.op = XEN_DOMCTL_MOCK_OP_ALLOC;
    domctl.u.mock_op.alloc.size = 0;

    rc = do_domctl(xch, &domctl);
    if ( !rc )
    {
        printf("mfn = 0x%16lX.\n", domctl.u.mock_op.alloc.mfn);
    }
}