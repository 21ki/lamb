
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lamb.h"

#define CHECK(cmd,val) !strncmp(cmd, val, strlen((val)))

int main(int argc, char **argv) {
    char *command;
    char *prompt = "lamb> ";
    char *history = ".lamb_history";

    //linenoiseSetCompletionCallback(completion);
    linenoiseHistoryLoad(history);

    while ((command = linenoise(prompt)) != NULL) {
        if (command && command[0] != '\0') {
            linenoiseHistoryAdd(command);
            linenoiseHistorySave(history);

            if (CHECK(command, "exit")) {
                exit(EXIT_SUCCESS);
            } else if (CHECK(command, "show version")) {
                lamb_show_version();
            } else if (CHECK(command, "show core")) {
                lamb_show_core();
            } else if (CHECK(command, "show client")) {
                lamb_show_client();
            } else if (CHECK(command, "show server")) {
                lamb_show_server();
            } else if (CHECK(command, "show gateway")) {
                lamb_show_gateway();
            } else if (CHECK(command, "change password")) {
                lamb_change_password(command);
            } else {
                printf("\033[31m%s\033[0m\n", "Error: Unknown command");
            }
        }
        
        linenoiseFree(command);
    }

    return 0;
}

void completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'h') {
        linenoiseAddCompletion(lc,"hello");
        linenoiseAddCompletion(lc,"hello there");
    }
}

void lamb_show_version(void) {
    printf("\033[37m");
    printf("lamb gateway version %s\n", "1.2");
    printf("Copyright (C) 2018 typefo <typefo@qq.com>");
    printf("\033[0m\n");
    return;
}

void lamb_show_core(void) {
    return;
}

void lamb_show_client(void) {
    return;
}

void lamb_show_server(void) {
    return;
}

void lamb_show_gateway(void) {
    return;
}

void lamb_change_password(const char *line) {
    printf("change password successfull\n");
    return;
}

void lamb_start_gateway(const char *line) {
    printf("start gateway successfull\n");
}

void lamb_start_gateway(const char *line) {
    printf("start gateway successfull\n");
    return;
}
