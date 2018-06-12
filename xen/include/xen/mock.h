#ifndef __XEN_MOCK_H__
#define __XEN_MOCK_H__

struct domain;
struct xen_domctl_mock_op;

struct mock_domain
{
    void *page;
    struct page_info *pg_struct;
};

int mock_domctl(struct domain *d, struct xen_domctl_mock_op *op);

#endif /* __XEN_MOCK_H__ */