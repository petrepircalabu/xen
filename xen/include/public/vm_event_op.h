/******************************************************************************
 * vm_event_op.h
 *
 * VM event management operations.
 *
 * Copyright (c) 2019 by Bitdefender S.R.L
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
 */

#ifndef __XEN_PUBLIC_VM_EVENT_OP_H__
#define __XEN_PUBLIC_VM_EVENT_OP_H__

#include "xen.h"

#if defined(__XEN__) || defined(__XEN_TOOLS__)

#define XEN_VM_EVENT_OP_enable          0
struct xen_vm_event_op_enable {
    /*
     * IN - Number of frames of the VM Event channels buffer.
     */
    uint32_t nr_frames;
    uint32_t pad;

    /*
     * IN - VM Event channels buffer list of frames.
     */
    XEN_GUEST_HANDLE(xen_pfn_t) frame_list;

    /*
     * OUT - VM Event interface version.
     */
    uint32_t version;
};

#define XEN_VM_EVENT_OP_disable         1

#endif /* defined(__XEN__) || defined(__XEN_TOOLS__) */
#endif /* __XEN_PUBLIC_VM_EVENT_OP_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
