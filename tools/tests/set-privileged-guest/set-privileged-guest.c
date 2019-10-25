/*
 * set-privileged-guest.c
 *
 * Set the "is_privileged" flag for the specified domain.
 *
 * Copyright (c) 2019 Bitdefender S.R.L.
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
    domid_t domain_id;
    int rc;
    char* progname = argv[0];

    argv++;
    argc--;

    if ( argc != 1 )
    {
        usage(progname);
        return -1;
    }

    domain_id = atoi(argv[0]);

    xch = xc_interface_open(NULL, NULL, 0);
    if ( xch == NULL )
    {
        ERROR("Error initialising the xencontrol interface");
        return -2;
    }

    DPRINTF("Setting \"is_privileged\" flag for domain %d ... ", domain_id);
    rc = xc_domain_set_privileged(xch, domain_id);
    if ( rc != 0 )
        PERROR("ERROR");
    else
        DPRINTF("DONE.\n");

    rc = xc_interface_close(xch);
    if ( rc != 0 )
    {
        ERROR("Error closing the xencontrol interface");
        return -2;
    }

    xch = NULL;
    return 0;
}
