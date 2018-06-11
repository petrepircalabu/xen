#include <xen/types.h>
#include <xen/lib.h>
#include <xen/mock.h>

int mock_domctl(struct domain *d, struct xen_domctl_mock_op *mop)
{
    printk("[DEBUG] Calling mock_domctl.\n");
    return 0;	
}
