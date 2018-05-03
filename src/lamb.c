
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lamb.h"
#include "account.h"
#include "gateway.h"
#include "channel.h"

#define LAMB_VERSION "1.2"
#define CHECK(cmd,val) !strncmp(cmd, val, strlen((val)))

static lamb_db_t *db;
static lamb_cache_t *rdb;

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
                if (db) {lamb_db_close(db);}
                if (rdb) {lamb_cache_close(rdb);}
                exit(EXIT_SUCCESS);
            } else if (CHECK(command, "help")) {
                lamb_help(command);
            } else if (CHECK(command, "show version")) {
                lamb_show_version(command);
            } else if (CHECK(command, "show mt")) {
                lamb_show_mt(command);
            } else if (CHECK(command, "show mo")) {
                lamb_show_mo(command);
            } else if (CHECK(command, "show core")) {
                lamb_show_core(command);
            } else if (CHECK(command, "show client")) {
                lamb_show_client(command);
            } else if (CHECK(command, "show server")) {
                lamb_show_server(command);
            } else if (CHECK(command, "show account")) {
                lamb_show_account(command);
            } else if (CHECK(command, "show gateway")) {
                lamb_show_gateway(command);
            } else if (CHECK(command, "show routing")) {
                lamb_show_routing(command);
            } else if (CHECK(command, "show delivery")) {
                lamb_show_delivery(command);
            } else if (CHECK(command, "show inbound")) {
                lamb_show_inbound(command);
            } else if (CHECK(command, "show outbound")) {
                lamb_show_outbound(command);
            } else if (CHECK(command, "show log")) {
                lamb_show_log(command);
            } else if (CHECK(command, "start server")){
                lamb_start_server(command);
            } else if (CHECK(command, "start gateway")){
                lamb_start_gateway(command);
            } else if (CHECK(command, "kill client")){
                lamb_kill_client(command);
            } else if (CHECK(command, "kill server")){
                lamb_kill_server(command);
            } else if (CHECK(command, "kill gateway")){
                lamb_kill_gateway(command);
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

void lamb_help(const char *line) {
    printf("\n");
    printf(" help                        Display help information\n");
    printf(" show mt                     Display mt queue information\n");
    printf(" show mo                     Display mo queue information\n");
    printf(" show core                   Display core module status information\n");
    printf(" show client                 Display client connection information\n");
    printf(" show server                 Display service module status information\n");
    printf(" show account                Display account information\n");
    printf(" show gateway                Display carrier gateway information\n");
    printf(" show routing                Display uplink routing information\n");
    printf(" show delivery               Display downlink routing information\n");
    printf(" show log [type]             Display module log information\n");
    printf(" kill client [id]            Kill the client disconnected\n");
    printf(" kill server [id]            Stop a service processing module\n");
    printf(" kill gateway [id]           Kill the gateway disconnected\n");
    printf(" start server [id]           Start a service processing module\n");
    printf(" start gateway [id]          Start a gateway service module\n");
    printf(" change password [password]  Change user login password\n");
    printf(" show version                Display software version information\n");
    printf(" exit                        Exit system login\n");
    printf("\n");

    return;
}

void lamb_show_version(const char *line) {
    printf("\033[37m");
    printf("lamb gateway version %s\n", LAMB_VERSION);
    printf("Copyright (C) 2018 typefo <typefo@qq.com>");
    printf("\033[0m\n");
    return;
}

void lamb_show_mt(const char *line) {
    int err;

    err = lamb_get_queue(rdb, "mt");

    if (err) {
        printf("\033[31m%s\033[0m\n", "Error getting mt queue information");
    }

    return;
}

void lamb_show_mo(const char *line) {

}

void lamb_show_core(const char *line) {
    return;
}

void lamb_show_client(const char *line) {
    return;
}

void lamb_show_server(const char *line) {
    return;
}

void lamb_show_account(const char *line) {
    int len;
    lamb_account_t *accounts[128] = {NULL};

    len = lamb_get_accounts(db, accounts, 128);

    if (0 > len) {
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-8s%-9s%-20s%-8s%-16s%-11s%-7s\n",
           "Id","User","Password","Spcode","company","address","concurrent","options");
    printf("------------------------------------------------------------------------------------\n");
    printf("\033[37m");

    for (int i = 0; i < len; i++) {
        if (accounts[i]) {
            printf(" %3d", accounts[i]->id);
            printf(" %-7.7s", accounts[i]->username);
            printf(" %-8.8s", accounts[i]->password);
            printf(" %-19.19s", accounts[i]->spcode);
            printf(" %-7d", accounts[i]->company);
            printf(" %-15.15s", accounts[i]->address);
            printf(" %-10d", accounts[i]->concurrent);
            printf(" %-6d", accounts[i]->options);
            printf("\n");
            free(accounts[i]);
        }
    }

    printf("\033[0m\n");
    return;
}

void lamb_show_gateway(const char *line) {
    int len;
    lamb_gateway_t *gateways[128] = {NULL};

    len = lamb_get_gateways(db, gateways, 128);

    if (0 > len) {
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-12s%-16s%-6s%-7s%-9s%-22s%-9s%-9s%-11s\n",
           "Id","Name","Host","Port","User","Password","Spcode","Encoding","Extended","Concurrent");
    printf("---------------------------------------------------------------------------------------------------------\n");
    printf("\033[37m");


    for (int i = 0; i < len; i++) {
        if (gateways[i]) {
            printf(" %3d", gateways[i]->id);
            printf(" %-11.11s", gateways[i]->name);
            printf(" %-15.15s", gateways[i]->host);
            printf(" %-5d", gateways[i]->port);
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

    printf("\033[0m\n");
    return;
}

void lamb_show_routing(const char *line) {
    int id, err;
    lamb_opt_t opt;
    lamb_list_t *channels;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "show routing", &opt);

    if (err) {
        printf("\033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    if (!opt.val[0]) {
        printf("\033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);
    channels = lamb_list_new();
    channels->free = free;

    if (!channels) {
        lamb_opt_free(&opt);
        printf("\033[31m%s\033[0m\n", "Error: The kernel can't allocate memory\n");
        return;
    }

    lamb_node_t *node;
    lamb_channel_t *channel;
    lamb_list_iterator_t *it;

    err = lamb_get_channels(db, id, channels);
    if (channels->len <= 0) {
        lamb_opt_free(&opt);
        printf(" There is no available data\n");
        return;
    }

    it = lamb_list_iterator_new(channels, LIST_HEAD);

    printf("\n");
    printf("%4s %-8s%-7s%-6s%-6s%-6s%-6s\n",
           "Id","Account","Weight","Cmcc","Ctcc","Cucc","Other");
    printf("---------------------------------------------------\n");
    printf("\033[37m");

    while ((node = lamb_list_iterator_next(it))) {
        channel = (lamb_channel_t *)node->val;
        printf(" %3d", channel->id);
        printf(" %-7d", channel->acc);
        printf(" %-6d", channel->weight);
        printf(" %-5.5s", (channel->operator & 1) ? "true" : "false");
        printf(" %-5.5s", (channel->operator & (1 << 1)) ? "true" : "false");
        printf(" %-5.5s", (channel->operator & (1 << 2)) ? "true" : "false");
        printf(" %-5.5s", (channel->operator & (1 << 3)) ? "true" : "false");
        printf("\n");
        lamb_list_remove(channels, node);
    }

    printf("\033[0m\n");
    lamb_opt_free(&opt);
    lamb_list_destroy(channels);
    lamb_list_iterator_destroy(it);
    return;
}

void lamb_show_delivery(const char *line) {
    int err;
    lamb_list_t *deliverys;

    deliverys = lamb_list_new();

    if (!deliverys) {
        printf("\033[31m%s\033[0m\n", "Error: The kernel can't allocate memory\n");
        return;
    }

    err = lamb_get_delivery(db, deliverys);
    if (err) {
        lamb_list_destroy(deliverys);
        printf("\033[31m%s\033[0m\n", "Error: reading database error\n");
        return;
    }

    if (deliverys->len <= 0) {
        lamb_list_destroy(deliverys);
        printf(" There is no available data\n");
        return;
    }

    lamb_node_t *node;
    lamb_delivery_t *delivery;
    lamb_list_iterator_t *it;

    it = lamb_list_iterator_new(deliverys, LIST_HEAD);

    printf("\n");
    printf("%4s %-25s%-8s\n", "Id", "Rexp", "Account");
    printf("---------------------------------------------------\n");
    printf("\033[37m");

    while ((node = lamb_list_iterator_next(it))) {
        delivery = (lamb_delivery_t *)node->val;
        printf(" %3d", delivery->id);
        printf(" %-24s", delivery->rexp);
        printf(" %-7d", delivery->target);
        printf("\n");
        lamb_list_remove(deliverys, node);
    }

    printf("\033[0m\n");

    lamb_list_destroy(deliverys);
    lamb_list_iterator_destroy(it);
    return;
}

void lamb_show_inbound(const char *line) {

}

void lamb_show_outbound(const char *line) {

}

void lamb_show_log(const char *line) {
    int err;
    lamb_opt_t opt;
    const char *type;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "show log", &opt);

    if (err) {
        printf("\033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    type = opt.val[0];

    if (type) {
        if (strcasecmp(type, "client") == 0) {
            system("tail -n 100 /var/log/lamb-ismg.log");
        } else if (strcasecmp(type, "server") == 0) {
            system("tail -n 100 /var/log/lamb-server.log");
        } else if (strcasecmp(type, "gateway") == 0) {
            system("tail -n 100 /var/log/lamb-gateway.log");
        } else {
            printf("\033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        }
    }

    lamb_opt_free(&opt);

    return;
}

void lamb_start_server(const char *line) {
    int id, err;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "start server", &opt);

    if (err || !opt.val[0]) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);

    err = lamb_add_taskqueue(db, id, "server", "server.conf", "-d");

    if (err) {
        printf(" \033[31m%s\033[0m\n", "Start server service failed");
    } else {
        printf(" \033[32m%s\033[0m\n", "Start server service successfull");
    }

    lamb_opt_free(&opt);
    return;
}

void lamb_start_gateway(const char *line) {
    int id, err;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "start gateway", &opt);

    if (err || !opt.val[0]) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);

    err = lamb_add_taskqueue(db, id, "sp", "sp.conf", "-d");

    if (err) {
        printf(" \033[31m%s\033[0m\n", "Start gateway service failed");
    } else {
        printf(" \033[32m%s\033[0m\n", "Start gateway service successfull");
    }

    lamb_opt_free(&opt);
    return;
}


void lamb_kill_client(const char *line) {
    printf(" kill client successfull\n");
    return;
}

void lamb_kill_server(const char *line) {
    printf(" kill server successfull\n");
    return;
}

void lamb_kill_gateway(const char *line) {
    printf(" kill gateway successfull\n");
    return;
}

void lamb_change_password(const char *line) {
    printf(" change password successfull\n");
    return;
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

void completion(const char *buf, linenoiseCompletions *lc) {
    if (buf[0] == 'h') {
        linenoiseAddCompletion(lc,"hello");
        linenoiseAddCompletion(lc,"hello there");
    }
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

    /* Redis Initialization */
    rdb = (lamb_cache_t *)malloc(sizeof(lamb_cache_t));
    if (!rdb) {
        printf("\033[31m%s\033[0m\n", "Error: The kernel can't allocate memory\n");
        return;
    }

    err = lamb_cache_connect(rdb, "127.0.0.1", 6379, NULL, 0);
    if (err) {
        printf("\033[31m%s\033[0m\n", "Error: Can't connect to redis database\n");
        return;
    }

    return;
}

int lamb_get_delivery(lamb_db_t *db, lamb_list_t *deliverys) {
    int rows;
    char sql[128];
    PGresult *res = NULL;

    deliverys->len = 0;
    snprintf(sql, sizeof(sql), "SELECT id, rexp, target FROM delivery");
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return -1;
    }

    rows = PQntuples(res);
    
    for (int i = 0; i < rows; i++) {
        lamb_delivery_t *delivery;
        delivery = (lamb_delivery_t *)calloc(1, sizeof(lamb_delivery_t));
        if (delivery) {
            delivery->id = atoi(PQgetvalue(res, i, 0));
            strncpy(delivery->rexp, PQgetvalue(res, i, 1), 127);
            delivery->target = atoi(PQgetvalue(res, i, 2));
            lamb_list_rpush(deliverys, lamb_node_new(delivery));
        }
    }

    PQclear(res);

    return 0;
}

int lamb_add_taskqueue(lamb_db_t *db, int eid, char *mod, char *config, char *argv) {
    char sql[128];
    char *column = NULL;
    PGresult *res = NULL;

    column = "eid, mod, config, argv, create_time";
    snprintf(sql, sizeof(sql), "INSERT INTO taskqueue(%s) VALUES(%d, '%s', '%s', '%s', now())",
             column, eid, mod, config, argv);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

int lamb_get_queue(lamb_cache_t *cache, const char *type) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGETALL %s.queue", type);

    if (reply != NULL) {
        freeReplyObject(reply);
    }

    return 0;
}
