/******************************************************************************
 * domstate_notify.h
 *
 * Domain state notification interface
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2019 Bitdefender S.R.L.
 */

#ifndef __XEN_PUBLIC_DOMSTATE_NOTIFY_H__
#define __XEN_PUBLIC_DOMSTATE_NOTIFY_H__

#include "xen.h"
#include "io/ring.h"

#define XEN_DOMSTATE_NOTIFY_register        1
#define XEN_DOMSTATE_NOTIFY_unregister      2
#define XEN_DOMSTATE_NOTIFY_enable          3
#define XEN_DOMSTATE_NOTIFY_disable         4

/*
 * XEN_DOMSTATE_NOTIFY_register: registers a page as a domain state change
 * notify sink.
 */
struct domstate_notify_register {
    uint64_t page_gfn;              /* IN */
};

typedef struct domstate_notify_st {
    uint32_t version;
    uint32_t domain_id;
    uint32_t state;
    uint32_t extra;
} domstate_notify_event_t;

DEFINE_RING_TYPES(domstate_notify, domstate_notify_event_t, domstate_notify_event_t);
#endif /* __XEN_PUBLIC_DOMSTATE_NOTIFY_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
