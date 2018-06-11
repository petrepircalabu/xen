#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#include <xenctrl.h>

#define DPRINTF(a, b...) fprintf(stderr, a, ## b)
#define ERROR(a, b...) fprintf(stderr, a "\n", ## b)
#define PERROR(a, b...) fprintf(stderr, a ": %s\n", ## b, strerror(errno))

int main(int argc, char *argv[])
{
    int rc;
    xc_interface *xch = xc_interface_open(NULL, NULL, 0);
    domid_t domain_id;

    argv++;
    argc--;

    domain_id = atoi(argv[0]);

    if ( !xch )
    {
        ERROR("Cannot open the xc_interface handle");
        return -1;
    }
    printf("Testing MOCK domctl...");
    xc_mock_alloc(xch, domain_id);

    rc = xc_interface_close(xch);
    if ( rc != 0 )
    {
        ERROR("Error closing connection to xen");
        return rc;
    }

    return 0;
}