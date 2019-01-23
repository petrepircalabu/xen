#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <poll.h>

#include <xenctrl.h>
#include <xendevicemodel.h>
#include <xenevtchn.h>
#include <xenforeignmemory.h>
#include <xen/hvm/dm_op.h>
#include <xen/hvm/ioreq.h>
#include <xen/vm_event.h>

#include <xen-tools/libs.h>

#if defined(__arm__) || defined(__aarch64__)
#include <xen/arch-arm.h>
#define START_PFN (GUEST_RAM0_BASE >> 12)
#elif defined(__i386__) || defined(__x86_64__)
#define START_PFN 0ULL
#endif

#define DPRINTF(a, b...) fprintf(stderr, a, ## b)
#define ERROR(a, b...) fprintf(stderr, a "\n", ## b)
#define PERROR(a, b...) fprintf(stderr, a ": %s\n", ## b, strerror(errno))

/* From xen/include/asm-x86/processor.h */
#define X86_TRAP_DEBUG  1
#define X86_TRAP_INT3   3

/* From xen/include/asm-x86/x86-defns.h */
#define X86_CR4_PGE        0x00000080 /* enable global pages */

#ifndef PFN_UP
#define PFN_UP(x)     (((x) + XC_PAGE_SIZE-1) >> XC_PAGE_SHIFT)
#endif /* PFN_UP */

#ifndef round_pgup
#define round_pgup(p)    (((p) + (XC_PAGE_SIZE - 1)) & XC_PAGE_MASK)
#endif /* round_pgup */

typedef struct monitor_state
{
    domid_t domain_id;
    int num_vcpus;

    xc_interface *xch;
    xenforeignmemory_handle *fmem;
    xendevicemodel_handle *xdm;

    ioservid_t ioservid;
    shared_iopage_t *shared_page;
    buffered_iopage_t *buffered_io_page;

    /* the evtchn port for polling the notification, */
    evtchn_port_t *ioreq_local_port;
    /* evtchn remote and local ports for buffered io */
    evtchn_port_t bufioreq_remote_port;
    evtchn_port_t bufioreq_local_port;
    /* the evtchn fd for polling */
    xenevtchn_handle *xce_handle;

    void *ring_buffer;
    void *channels_buffer;

    vm_event_back_ring_t back_ring;

    xen_pfn_t max_gpfn;

} monitor_state_t;

static int interrupted;
static void close_handler(int sig)
{
    interrupted = sig;
}

static inline uint32_t xen_vcpu_eport(shared_iopage_t *shared_page, int i)
{
    return shared_page->vcpu_ioreq[i].vp_eport;
}
static inline ioreq_t *xen_vcpu_ioreq(shared_iopage_t *shared_page, int vcpu)
{
    return &shared_page->vcpu_ioreq[vcpu];
}

static int monitor_map_ioreq_server(monitor_state_t *state)
{
    void *addr = NULL;
    xenforeignmemory_resource_handle *fres;
    evtchn_port_t bufioreq_evtchn;
    int rc;

    fres = xenforeignmemory_map_resource(state->fmem, state->domain_id,
                                         XENMEM_resource_ioreq_server,
                                         state->ioservid, 0, 2,
                                         &addr,
                                         PROT_READ | PROT_WRITE, 0);
    if (fres == NULL)
    {
        ERROR("Mapping failure");
        return -1;
    }

    state->buffered_io_page = addr;
    state->shared_page = addr + XC_PAGE_SIZE;

    rc = xendevicemodel_get_ioreq_server_info(state->xdm,
                                              state->domain_id,
                                              state->ioservid,
                                              NULL, NULL,
                                              &bufioreq_evtchn);
    if (rc < 0) {
        ERROR("failed to get ioreq server info: error %d\n", errno);
        return rc;
    }

    state->bufioreq_remote_port = bufioreq_evtchn;

    return 0;
}

static int monitor_map_vm_event(monitor_state_t *state)
{
    void *buffer = NULL;
    xenforeignmemory_resource_handle *fres;
    /* int rc; */

    unsigned long nr_frames = 1 + PFN_UP( state->num_vcpus * sizeof( struct vm_event_slot) );

    fres = xenforeignmemory_map_resource(state->fmem, state->domain_id,
                                         XENMEM_resource_vm_event,
                                         XEN_VM_EVENT_TYPE_MONITOR << 8 | state->ioservid,
                                         0, nr_frames, &buffer,
                                         PROT_READ | PROT_WRITE, 0);

    if ( !fres )
        return -errno;

    state->ring_buffer = buffer;
    state->channels_buffer = buffer + 1 * XC_PAGE_SIZE;

    return 0;
}

static void monitor_teardown(monitor_state_t *state)
{
    int rc;

    if ( state == NULL )
        return;

    rc = xendevicemodel_set_ioreq_server_state(state->xdm, state->domain_id,
                                               state->ioservid, 0);
    if ( rc != 0 )
        ERROR("Failed to set the ioreq_server state rc = %d\n", rc);

    if ( state->ring_buffer )
        munmap(state->ring_buffer, XC_PAGE_SIZE);

    if ( state->channels_buffer )
        munmap(state->channels_buffer,
               round_pgup(state->num_vcpus * sizeof (struct vm_event_slot)));

    xc_monitor_disable(state->xch, state->domain_id);

    xenevtchn_close(state->xce_handle);

    rc = xendevicemodel_close(state->xdm);
    if ( rc != 0 )
        ERROR("Error closing the xen device model interface rc = %d\n", rc);

    rc = xenforeignmemory_close(state->fmem);
    if ( rc != 0 )
        ERROR("Error closing the xenforeignmemory interface rc = %d\n", rc);

    rc = xc_interface_close(state->xch);
    if ( rc != 0 )
        ERROR("Error closing connection to xen rc = %d\n", rc);

    state->xch = NULL;
    free(state);
}

static int monitor_init(domid_t domain_id, monitor_state_t ** state_)
{
    monitor_state_t *state = NULL;
    xc_interface *xch;
    xendevicemodel_handle *xdm;
    xenforeignmemory_handle *fmem;
    xc_dominfo_t info;
    int rc = -1, i;

    xch = xc_interface_open(NULL, NULL, 0);
    if ( !xch )
    {
        ERROR("Error opening the xenctrl interface\n");
        return -1;
    }

    fmem = xenforeignmemory_open(0, 0);
    if ( !fmem )
    {
        ERROR("Error opening xenforeignmemory interface\n");
        xc_interface_close(xch);
        return -1;
    }

    xdm = xendevicemodel_open(NULL, 0);
    if ( !xdm )
    {
        ERROR("Error opening the xen devicemodel interface\n");
        xc_interface_close(xch);
        xenforeignmemory_close(fmem);
        return -1;
    }

    DPRINTF("Monitor init\n");

    state = malloc(sizeof(monitor_state_t));
    memset(state, 0, sizeof(monitor_state_t));

    state->domain_id = domain_id;
    state->xch = xch;
    state->fmem = fmem;
    state->xdm = xdm;

    state->xce_handle = xenevtchn_open(NULL, 0);
    if (state->xce_handle == NULL) {
        ERROR("xen: event channel open");
        goto err;
    }

    rc = xc_domain_getinfo(xch, domain_id, 1, &info);
    if ( rc != 1 )
    {
        ERROR("xc_domain_getinfo failed. rc = %d\n", rc);
        goto err;
    }
    state->num_vcpus = info.max_vcpu_id + 1;

    rc = xendevicemodel_create_ioreq_server(state->xdm, domain_id,
                                            HVM_IOREQSRV_BUFIOREQ_ATOMIC,
                                            &state->ioservid);
    if ( rc )
    {
        ERROR("Failed to create ioreq server: rc = %d\n", rc);
        goto err;
    }

    rc = monitor_map_ioreq_server(state);
    if ( rc )
    {
        ERROR("Failed to map the ioreq server rc = %d\n", rc);
        goto err;
    }

    rc = monitor_map_vm_event(state);
    if ( rc )
    {
        ERROR("Failed to map vm_event rc = %d\n", rc);
        goto err;
    }

    rc = xendevicemodel_set_ioreq_server_state(state->xdm, domain_id,
                                               state->ioservid, 1);
    if ( rc )
    {
        ERROR("Failed to set the ioreq_server state rc = %d\n", rc);
        goto err;
    }

    /* Initialise ring */
    SHARED_RING_INIT((vm_event_sring_t *)state->ring_buffer);
    BACK_RING_INIT(&state->back_ring, (vm_event_sring_t *)state->ring_buffer, XC_PAGE_SIZE);

    state->ioreq_local_port = calloc(state->num_vcpus, sizeof(evtchn_port_t));
    for ( i = 0; i < state->num_vcpus; i++ )
    {
        rc = xenevtchn_bind_interdomain(state->xce_handle, state->domain_id,
                                        xen_vcpu_eport(state->shared_page, i));
        if ( rc == -1 )
        {
            ERROR("Failed to bind shared evtchn errno=%d\n", errno);
            goto err;
        }
        state->ioreq_local_port[i] = rc;
    }

    rc = xenevtchn_bind_interdomain(state->xce_handle, state->domain_id,
                                    state->bufioreq_remote_port);
    if ( rc == -1 )
    {
        ERROR("Failed to bind evtchn buffer errno=%d\n", errno);
        goto err;
    }
    state->bufioreq_local_port = rc;

    *state_ = state;
    return 0;

err:
    monitor_teardown(state);
    free(state);
    return rc;
}

static ioreq_t *cpu_get_ioreq_from_shared_memory(monitor_state_t *state, int vcpu)
{
    ioreq_t * req = xen_vcpu_ioreq(state->shared_page, vcpu);
    if ( req->state != STATE_IOREQ_READY )
        return NULL;

    req->state = STATE_IOREQ_INPROCESS;
    return req;
}

static int monitor_wait_for_events(monitor_state_t *state, unsigned long ms, ioreq_t **req_)
{
    int rc, i;
    struct pollfd fds;
    evtchn_port_t port;

    fds.fd = xenevtchn_fd(state->xce_handle);
    fds.events = POLLIN | POLLERR;
    fds.revents = 0;

    rc = poll(&fds, 1, ms);
    if ( rc == -1 || rc == 0 )
    {
        if ( errno == EINTR )
            rc = 0;
        return rc;
    }

    if ( !(fds.revents & POLLIN) )
        return -1;

    port = xenevtchn_pending(state->xce_handle);
    if ( port == -1 )
    {
        ERROR("xenevtchn_pending returned -1\n");
        return -1;
    }

    if ( port == state->bufioreq_local_port)
    {
        xenevtchn_unmask(state->xce_handle, port);
        *req_ = NULL;
        return port;
    }

    for ( i = 0; i < state->num_vcpus; i++ )
        if ( state->ioreq_local_port[i] == port)
            break;

    if ( i == state->num_vcpus )
        return -2;

    /* unmask the wanted port again */
    xenevtchn_unmask(state->xce_handle, port);

    *req_ = cpu_get_ioreq_from_shared_memory(state, i);

    return port;
}

static bool get_request(monitor_state_t *state, int index, vm_event_request_t *vme_req)
{
    struct vm_event_slot *slot = &((struct vm_event_slot *)state->channels_buffer)[index];

    memcpy(vme_req, &slot->u.req, sizeof(*vme_req));
    return true;
}

static void get_ring_request(vm_event_back_ring_t *back_ring, vm_event_request_t *req)
{
    RING_IDX req_cons;

    req_cons = back_ring->req_cons;

    /* Copy request */
    memcpy(req, RING_GET_REQUEST(back_ring, req_cons), sizeof(*req));
    req_cons++;

    /* Update ring */
    back_ring->req_cons = req_cons;
    back_ring->sring->req_event = req_cons + 1;
}

static void put_response(monitor_state_t *state, ioreq_t *req, vm_event_response_t *vme_rsp)
{
    struct vm_event_slot *slot = &((struct vm_event_slot *)state->channels_buffer)[req->addr];
    memcpy(&slot->u.rsp, vme_rsp, sizeof(*vme_rsp));
    slot->state = VM_EVENT_SLOT_STATE_FINISH;

    req->state = STATE_IORESP_READY;
    xenevtchn_notify(state->xce_handle,
                     state->ioreq_local_port[req->addr]);
}

/*
 * X86 control register names
 */
static const char* get_x86_ctrl_reg_name(uint32_t index)
{
    static const char* names[] = {
        [VM_EVENT_X86_CR0]  = "CR0",
        [VM_EVENT_X86_CR3]  = "CR3",
        [VM_EVENT_X86_CR4]  = "CR4",
        [VM_EVENT_X86_XCR0] = "XCR0",
    };

    if ( index >= ARRAY_SIZE(names) || names[index] == NULL )
        return "";

    return names[index];
}

int main(int argc, char *argv[])
{
    struct sigaction act;
    domid_t domain_id;
    monitor_state_t *state;
    int shutting_down = 0;
    int rc = 0;
    /* Exec */
    xenmem_access_t default_access = XENMEM_access_rw;
    xenmem_access_t after_first_access = XENMEM_access_rwx;
    int memaccess = 0;
    int write_ctrlreg_cr4 = 0;


    /* char* progname = argv[0]; */
    argv++;
    argc--;

    if ( argc != 2 )
    {
        //usage(progname);
        return -1;
    }

    domain_id = atoi(argv[0]);
    argv++;
    argc--;

    if ( !strcmp(argv[0], "write") )
    {
        default_access = XENMEM_access_rx;
        after_first_access = XENMEM_access_rwx;
        memaccess = 1;
    }
    else if ( !strcmp(argv[0], "exec") )
    {
        default_access = XENMEM_access_rw;
        after_first_access = XENMEM_access_rwx;
        memaccess = 1;
    }
    else if ( !strcmp(argv[0], "write_ctrlreg_cr4") )
    {
        write_ctrlreg_cr4 = 1;
    }

    /* ensure that if we get a signal, we'll do cleanup, then exit */
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    rc = monitor_init(domain_id, &state);
    if ( rc )
    {
        ERROR("Error initializing monitor\n");
        goto exit;
    }

    if ( memaccess )
    {
        /* Get max_gpfn */
        rc = xc_domain_maximum_gpfn(state->xch, state->domain_id,
                                    &state->max_gpfn);
        if ( rc )
        {
            ERROR("Failed to get max gpfn");
            goto exit;
        }


        /* Set the default access type and convert all pages to it */
        rc = xc_set_mem_access(state->xch, domain_id, default_access, ~0ull, 0);
        if ( rc < 0 )
        {
            ERROR("Error %d setting default mem access type\n", rc);
            goto exit;
        }

        rc = xc_set_mem_access(state->xch, domain_id, default_access, START_PFN,
                                (state->max_gpfn - START_PFN) );

        if ( rc < 0 )
        {
            ERROR("Error %d setting all memory to access type %d\n", rc,
                    default_access);
            goto exit;
        }
    }

    if ( write_ctrlreg_cr4 )
    {
        /* Mask the CR4.PGE bit so no events will be generated for global TLB flushes. */
        rc = xc_monitor_write_ctrlreg(state->xch, state->domain_id, VM_EVENT_X86_CR4, 1, 0,
                                      0, 0);
        if ( rc < 0 )
        {
            ERROR("Error %d setting write control register trapping with vm_event\n", rc);
            goto exit;
        }
    }

    /* Wait for access */
    for (;;)
    {
        int port;
        ioreq_t *req = NULL;
        vm_event_request_t vm_req;
        vm_event_response_t vm_rsp;
        int index = -1;

        if ( interrupted  && !shutting_down )
        {
            rc = xc_set_mem_access(state->xch, state->domain_id, XENMEM_access_rwx, ~0ull, 0);
            rc = xc_set_mem_access(state->xch, state->domain_id, XENMEM_access_rwx, START_PFN,
                                    (state->max_gpfn - START_PFN) );
            if ( write_ctrlreg_cr4 )
                rc = xc_monitor_write_ctrlreg(state->xch, state->domain_id, VM_EVENT_X86_CR4, 0, 0, 0, 0);

            shutting_down = state->num_vcpus + 1;
        }

        port = monitor_wait_for_events(state, 100, &req);
        if (port == 0)
        {
            if ( !shutting_down )
                continue;
	    shutting_down--;
        }
        else if (port < 0)
        {
            interrupted = -1;
            continue;
        }

        if (!req)
        {
            if ( port != state->bufioreq_local_port || !RING_HAS_UNCONSUMED_REQUESTS(&state->back_ring))
                goto skip_invalid;

            DPRINTF("ASYNC REQ received\n");
            get_ring_request(&state->back_ring, &vm_req);
            index = state->num_vcpus + 1;
        }
        else if ( get_request(state, req->addr, &vm_req) )
        {
            DPRINTF("REQ received addr = 0x%08lX type=%d\n", req->addr, req->type);
            index = req->addr;
        }

        if (index >= 0)
        {
            memset( &vm_rsp, 0, sizeof (vm_rsp) );
            vm_rsp.version = VM_EVENT_INTERFACE_VERSION;
            vm_rsp.vcpu_id = vm_req.vcpu_id;
            vm_rsp.flags = (vm_req.flags & VM_EVENT_FLAG_VCPU_PAUSED);
            vm_rsp.reason = vm_req.reason;

            switch ( vm_req.reason )
            {
            case VM_EVENT_REASON_MEM_ACCESS:
                if ( !shutting_down )
                {
                    /*
                    * This serves no other purpose here then demonstrating the use of the API.
                    * At shutdown we have already reset all the permissions so really no use getting it again.
                    */
                    xenmem_access_t access;
                    rc = xc_get_mem_access(state->xch, state->domain_id, vm_req.u.mem_access.gfn, &access);
                    if (rc < 0)
                    {
                        ERROR("Error %d getting mem_access event\n", rc);
                        interrupted = -1;
                        continue;
                    }
                }

                printf("PAGE ACCESS: %c%c%c for GFN %"PRIx64" (offset %06"
                    PRIx64") gla %016"PRIx64" (valid: %c; fault in gpt: %c; fault with gla: %c) (vcpu %u [%c], altp2m view %u)\n",
                    (vm_req.u.mem_access.flags & MEM_ACCESS_R) ? 'r' : '-',
                    (vm_req.u.mem_access.flags & MEM_ACCESS_W) ? 'w' : '-',
                    (vm_req.u.mem_access.flags & MEM_ACCESS_X) ? 'x' : '-',
                    vm_req.u.mem_access.gfn,
                    vm_req.u.mem_access.offset,
                    vm_req.u.mem_access.gla,
                    (vm_req.u.mem_access.flags & MEM_ACCESS_GLA_VALID) ? 'y' : 'n',
                    (vm_req.u.mem_access.flags & MEM_ACCESS_FAULT_IN_GPT) ? 'y' : 'n',
                    (vm_req.u.mem_access.flags & MEM_ACCESS_FAULT_WITH_GLA) ? 'y': 'n',
                    vm_req.vcpu_id,
                    (vm_req.flags & VM_EVENT_FLAG_VCPU_PAUSED) ? 'p' : 'r',
                    vm_req.altp2m_idx);

                if ( default_access != after_first_access )
                {
                    rc = xc_set_mem_access(state->xch, state->domain_id, after_first_access,
                                            vm_req.u.mem_access.gfn, 1);
                    if (rc < 0)
                    {
                        ERROR("Error %d setting gfn to access_type %d\n", rc,
                            after_first_access);
                        interrupted = -1;
                        continue;
                    }
                }

                vm_rsp.u.mem_access = vm_req.u.mem_access;

                break;

            case VM_EVENT_REASON_WRITE_CTRLREG:
                printf("Control register written: rip=%016"PRIx64", vcpu %d: "
                       "reg=%s, old_value=%016"PRIx64", new_value=%016"PRIx64"\n",
                       vm_req.data.regs.x86.rip,
                       vm_req.vcpu_id,
                       get_x86_ctrl_reg_name(vm_req.u.write_ctrlreg.index),
                       vm_req.u.write_ctrlreg.old_value,
                       vm_req.u.write_ctrlreg.new_value);
                break;

            default:
                DPRINTF("Unsupported vm_event_ request: reason: %d", vm_req.reason);
            }

            /* Put the response on the ring */
            if (vm_req.flags & VM_EVENT_FLAG_VCPU_PAUSED)
                put_response(state, req, &vm_rsp);

        }

skip_invalid:
        if ( shutting_down == 1 )
        {
            DPRINTF("Shutting down monitor\n");
            break;
        }
    }

exit:
    monitor_teardown(state);

    return rc;
}
