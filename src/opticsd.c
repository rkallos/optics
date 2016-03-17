/* opticsd.c
   Rémi Attab (remi.attab@gmail.com), 15 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "poller.h"
#include "utils/time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

void parse_carbon(struct optics_poller *poller, const char *args)
{
    char *buffer = strdup(args);

    const char *host = buffer;
    const char *port = "2003";

    char *sep = strchr(buffer, ':');
    if (sep) {
        *sep = '\0';
        port = sep + 1;
    }

    optics_dump_carbon(poller, host, port);

    free(buffer);
}

bool run_poller(struct optics_poller *poller, optics_ts_t freq)
{
    while (true) {
        optics_poller_poll(poller);
        nsleep(freq * 1000 * 1000 * 1000);
    }
}


// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

void print_usage()
{
    printf("Usage: \n"
            "  opticsd [--stdout] [--carbon=<host:port>]\n"
            "\n"
            "Options:\n"
            "  --dump-stdout              Dumps metrics to stdout\n"
            "  --dump-carbon=<host:port>  Dumps metrics to the given carbon host:port\n"
            "  -h --help                  Prints this message\n");
}


int main(int argc, char **argv)
{
    struct optics_poller *poller = optics_poller_alloc();
    optics_ts_t freq = 10;
    bool backend_selected = false;

    while (true) {
        static struct option options[] = {
            {"carbon", required_argument, 0, 'c'},
            {"stdout", no_argument, 0, 's'},
            {"freq", required_argument, 0, 'f'},
            {"help", no_argument, 0, 'h'},
            {0}
        };

        int opt_char = getopt_long(argc, argv, "h", options, NULL);
        if (opt_char < 0) break;

        switch (opt_char) {
        case 'f':
            freq = atol(optarg);
            if (!freq) {
                printf("invalid freq argument: %s\n", optarg);
                print_usage();
                return EXIT_FAILURE;
            }
            break;

        case 's':
            backend_selected = true;
            optics_dump_stdout(poller);
            break;

        case 'c':
            backend_selected = true;
            parse_carbon(poller, optarg);
            break;

        case 'h':
        default:
            print_usage();
            return opt_char == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    if (!backend_selected) {
        printf("No dump option selected.\n");
        print_usage();
        return EXIT_FAILURE;
    }

    return run_poller(poller, freq) ? EXIT_SUCCESS : EXIT_FAILURE;
}