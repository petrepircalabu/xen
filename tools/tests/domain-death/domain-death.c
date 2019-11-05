/*
 * domain-death.c
 *
 * Copyright (c) 2019 Bitdefender S.R.L
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libxl.h>
#include <xenctrl.h>

#define DPRINTF(a, b...) fprintf(stderr, a, ## b)
#define ERROR(a, b...) fprintf(stderr, a "\n", ## b)
#define PERROR(a, b...) fprintf(stderr, a ": %s\n", ## b, strerror(errno))


void usage(char* progname)
{
    fprintf(stderr, "Usage: %s <domain_id>\n", progname);
}

int main(int argc, char *argv[])
{
    xc_interface *xch;
    libxl_ctx *ctx = NULL;
    domid_t domain_id;
    int rc;
    char* progname = argv[0];
    libxl_event *event;
    libxl_evgen_domain_death *deathw;
    xentoollog_logger_stdiostream *logger_s
        = xtl_createlogger_stdiostream(stderr, XTL_NOTICE,  0);

    argv++;
    argc--;

    if ( argc != 1 )
    {
        usage(progname);
        return -1;
    }

    domain_id = atoi(argv[0]);

    xch = xc_interface_open((xentoollog_logger *)logger_s, (xentoollog_logger *)logger_s, 0);
    if ( xch == NULL )
    {
        ERROR("Error initialising the xencontrol interface");
        return -2;
    }

    rc = libxl_ctx_alloc(&ctx, LIBXL_VERSION, 0, (xentoollog_logger *)logger_s);
    if ( rc != 0 )
    {
        ERROR("Error allocating libxl_ctx");
        goto cleanup;
    /*     return rc; */
    }

    rc = libxl_evenable_domain_death(ctx, domain_id, 0, &deathw);
    if ( rc != 0 )
    {
        ERROR("Error allocating libxl_ctx");
        goto cleanup;
    /*     return rc; */
    }

    rc = libxl_event_wait(ctx, &event, LIBXL_EVENTMASK_ALL, 0,0);
    if ( rc != 0 )
    {
        ERROR("Failed to get event, quitting (rc=%d)", rc);
        goto cleanup;
    }

    switch (event->type)
    {
    case LIBXL_EVENT_TYPE_DOMAIN_DEATH:
        DPRINTF("Domain %d has been destroyed\n", event->domid);
        break;
    case LIBXL_EVENT_TYPE_DOMAIN_SHUTDOWN:
        DPRINTF("Domain %d has been shut down, reason code %d",
            event->domid, event->u.domain_shutdown.shutdown_reason);
        break;
    default:
        DPRINTF("Unexpected event type %d", event->type);
        break;
    }
    libxl_event_free(ctx, event);
cleanup:
    libxl_ctx_free(ctx);
    xc_interface_close(xch);

    xch = NULL;
    return rc;
}

