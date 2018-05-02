
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lamb.h"
#include "db.h"
#include "gateway.h"

#define LAMB_VERSION "1.2"
#define CHECK(cmd,val) !strncmp(cmd, val, strlen((val)))

static lamb_db_t *db;

int main(int argc, char **argv) {
    char *command;
    char *prompt = "lamb> ";
    char *history = ".lamb_history";

    //linenoiseSetCompletionCallback(completion);
    linenoiseHistoryLoad(history);

    /* Initialization component */
    lamb_component_initialization(NULL);
        
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
            } else if (CHECK(command, "show log")) {
                lamb_show_log(command);
            } else if (CHECK(command, "start gateway")){
                lamb_start_gateway(command);
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
    printf("lamb gateway version %s\n", LAMB_VERSION);
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
    int len;
    lamb_gateway_t *gateways[128] = {NULL};

    len = lamb_get_gateways(db, gateways, 128);

    if (len > 0) {
        printf("\n");
        printf("%4s %-12s%-16s%-6s%-7s%-9s%-22s%-9s%-9s%-11s\n",
               "Id","name","Host","Port","User","Password","Spcode","Encoding","Extended","Concurrent");
        printf("---------------------------------------------------------------------------------------------------------\n");
        printf("\033[37m");
    }

    for (int i = 0; i < len; i++) {
        if (gateways[i]) {
            printf(" %3d", gateways[i]->id);
            printf(" %-11.11s", gateways[i]->name);
            printf(" %-15.15s", gateways[i]->host);
            printf(" %-5.5d", gateways[i]->port);
            printf(" %-6.6s", gateways[i]->username);
            printf(" %-8.8s", gateways[i]->password);
            printf(" %-21.21s", gateways[i]->spcode);
            if (gateways[i]->encoding == 0) {
                printf(" %-8.8s", "ascii");
            } else if (gateways[i]->encoding == 8) {
                printf(" %-8.8s", "ucs-2");
            } else if (gateways[i]->encoding == 11) {
                printf(" %-8.8s", "utf-8");
            } else if (gateways[i]->encoding == 15) {
                printf(" %-8.8s", "gbk");
            } else {
                printf(" %-8.8s", "unkno");
            }
            printf(" %-8.8s", gateways[i]->extended ? "true" : "false");
            printf(" %-10d\n", gateways[i]->concurrent);
            free(gateways[i]);
        }
    }

    if (len > 0) {
        printf("\033[0m\n");
    }

    return;
}

void lamb_show_log(const char *line) {
    int err;
    lamb_opt_t opt;
    const char *type;

    err = lamb_opt_parsing(line, "show log", &opt);

    if (err) {
        printf("\033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    type = opt.val[0];

    if (type) {
        if (strcasecmp(type, "cli") == 0) {
            system("tail -n 100 /var/log/lamb-ismg.log");
        } else if (strcasecmp(type, "gw") == 0) {
            system("tail -n 100 /var/log/lamb-gateway.log");
        } else {
            printf("\033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        }
    }

    //lamb_opt_free(&opt);

    return;
}

void lamb_change_password(const char *line) {
    printf("change password successfull\n");
    return;
}

void lamb_start_gateway(const char *line) {
    printf("start gateway successfull\n");
}


int lamb_opt_parsing(const char *cmd, const char *prefix, lamb_opt_t *opt) {
    char *src;
    char *delims = " ";
    char *result = NULL;

    if (!cmd || !prefix) {
        return -1;
    }

    if (strlen(prefix) >= strlen(cmd)) {
        return -1;
    }

    src = strdup(cmd + strlen(prefix));
    result = strtok(src, delims);

    for (int i = 0; result != NULL && i < 15; i++) {
        opt->len++;
        opt->val[i] = strdup(result);
        result = strtok(NULL, delims);
    }

    if (src) {
        free(src);
        return 0;
    }

    return -1;
}

void lamb_opt_free(lamb_opt_t *opt) {
    if (opt) {
        for (int i = 0; i < opt->len; i++) {
            if (opt->val[i]) {
                free(opt->val[i]);
                opt->val[i] = NULL;
            }
        }
    }

    return;
}

void lamb_component_initialization(lamb_config_t *cfg) {
    int err;

    /* Postgresql Database  */
    db = (lamb_db_t *)malloc(sizeof(lamb_db_t));
    if (!db) {
        printf("\033[31m%s\033[0m\n", "Error: The kernel can't allocate memory\n");
        return;
    }

    err = lamb_db_init(db);
    if (err) {
        printf("\033[31m%s\033[0m\n", "Error: Database initialization failed\n");
        return;
    }

    err = lamb_db_connect(db, "127.0.0.1", 5432, "postgres", "postgres", "lamb");
    if (err) {
        printf("\033[31m%s\033[0m\n", "Error: Can't connect to database\n");
        return;
    }

    return;
}
