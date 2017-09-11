
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "lamb.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    int err;
    char *host = "127.0.0.1";
    int port = 1276;
    char *prompt = "lamb";

    /* Signal event processing */
    lamb_signal_processing();

    int opt = 0;
    char *optstring = "sg:c:";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'c':
            file = optarg;
            break;
        case 'd':
            daemon = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    return 0;
}

