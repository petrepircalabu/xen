#ifndef __XEN_MOCK_H__
#define __XEN_MOCK_H__

#include <public/domctl.h>

struct domain;
struct xen_domctl_mock_op;

struct mock_domain
{
    void *page;
    struct page_info *pg_struct;
};

int mock_domctl(struct domain *d, struct xen_domctl_mock_op *op,
                XEN_GUEST_HANDLE_PARAM(xen_domctl_t));

#endif /* __XEN_MOCK_H__ */