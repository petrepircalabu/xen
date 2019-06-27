/******************************************************************************
 * mock.h
 *
 * Mock operation support.
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
#include <xen/mock.h>
#include <xen/vmap.h>

struct mock_domain
{
    void *va;
    uint32_t nr_frames;
};

static int mock_enable(struct mock_domain **p_mock, uint32_t nr_frames)
{
    struct mock_domain *mock;
    int i, rc;

    if ( *p_mock != NULL )
        return -EBUSY;

    mock = xzalloc(struct mock_domain);
    if ( !mock )
        return -ENOMEM;

    mock->va = vzalloc(nr_frames * PAGE_SIZE);
    if ( mock->va == NULL )
    {
        rc = -ENOMEM;
        goto err;
    }

    mock->nr_frames = nr_frames;

    for ( i = 0; i < nr_frames; i++ )
    {
        rc = assign_pages(current->domain,
                          vmap_to_page(mock->va + i * PAGE_SIZE), 0, 0);
        if ( rc )
            goto err;
    }

    *p_mock = mock;

    return 0;

err:
    vfree(mock->va);
    xfree(mock);
    return rc;
}

static int mock_disable(struct mock_domain **p_mock)
{
    struct mock_domain *mock = *p_mock;

    vfree(mock->va);

    xfree(mock);
    *p_mock = NULL;

    return 0;
}

int mock_domctl(struct domain *d, struct xen_domctl_mock_op *vec)
{
    int rc = -EINVAL;

    switch ( vec->op )
    {
    case XEN_DOMCTL_MOCK_OP_ENABLE:
        rc = mock_enable(&d->mock, vec->nr_frames);
        break;

    case XEN_DOMCTL_MOCK_OP_DISABLE:
        rc = mock_disable(&d->mock);
        break;
    }

    return 0;
}

int mock_get_frames(struct domain *d, unsigned int id,
                    unsigned long frame, unsigned int nr_frames,
                    xen_pfn_t mfn_list[])
{
    int i;
    struct mock_domain *mock = d->mock;

    if ( mock == NULL || mock->va == NULL )
        return -EINVAL;

    if ( frame != 0 || nr_frames != mock->nr_frames )
        return -EINVAL;

    for ( i = 0; i < mock->nr_frames; i++ )
    {
        mfn_list[i] = mfn_x(vmap_to_mfn(mock->va + i * PAGE_SIZE));
    }

    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
