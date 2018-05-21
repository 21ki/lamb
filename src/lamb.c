
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "lamb.h"
#include "account.h"
#include "gateway.h"
#include "channel.h"

#define LAMB_VERSION "1.2"
#define CHECK(cmd,val) !strncmp(cmd, val, strlen((val)))

static lamb_db_t *db;
static lamb_cache_t *rdb;
static lamb_caches_t *blk;

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
            } else if (CHECK(command, "show channel")) {
                lamb_show_channel(command);
            } else if (CHECK(command, "show log")) {
                lamb_show_log(command);
            } else if (CHECK(command, "start server")){
                lamb_start_server(command);
            } else if (CHECK(command, "start channel")){
                lamb_start_channel(command);
            } else if (CHECK(command, "kill client")){
                lamb_kill_client(command);
            } else if (CHECK(command, "kill server")){
                lamb_kill_server(command);
            } else if (CHECK(command, "kill channel")){
                lamb_kill_channel(command);
            } else if (CHECK(command, "check blacklist")){
                lamb_check_blacklist(command);
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
    printf(" show channel                Display carrier channel status information\n");
    printf(" show gateway                Display carrier gateway information\n");
    printf(" show delivery               Display downlink routing information\n");
    printf(" show routing <id>           Display uplink routing information\n");
    printf(" show log <type>             Display module log information\n");
    printf(" kill client <id>            Kill a client disconnected\n");
    printf(" kill server <id>            Kill a service processing module\n");
    printf(" kill channel <id>           Kill a gateway disconnected\n");
    printf(" start server <id>           Start a service processing module\n");
    printf(" start channel <id>          Start a gateway channel service\n");
    printf(" check blacklist <phone>     Check whether the mobile phone number is blacklisted\n");
    printf(" change password <password>  Change user login password\n");
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
    lamb_kv_t *q;
    lamb_node_t *node;
    lamb_list_t *queues;
    lamb_list_iterator_t *it;

    queues = lamb_list_new();
    queues->free = free;

    if (!queues) {
        printf("\033[31m%s\033[0m\n", "Error: The kernel can't allocate memory\n");
        return;
    }

    err = lamb_get_queue(rdb, "mt", queues);

    if (err) {
        lamb_list_destroy(queues);
        printf("\033[31m%s\033[0m\n", "Error getting mt queue information");
        return;
    }

    
    if (queues->len < 1) {
        lamb_list_destroy(queues);
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-8s%-7s%-16s\n", "Id","Account","Total","Description");
    printf("---------------------------------------------------\n");
    printf("\033[37m");

    it = lamb_list_iterator_new(queues, LIST_HEAD);

    while ((node = lamb_list_iterator_next(it))) {
        q = (lamb_kv_t *)node->val;
        printf(" %3d", q->id);
        printf(" %-7.7s", q->acc);
        printf(" %-6ld", q->total);
        printf(" %-15.15s", q->desc);
        printf("\n");
        lamb_list_remove(queues, node);
    }

    printf("\033[0m\n");
    lamb_list_destroy(queues);
    lamb_list_iterator_destroy(it);

    return;
}

void lamb_show_mo(const char *line) {
    int err;
    lamb_kv_t *q;
    lamb_node_t *node;
    lamb_list_t *queues;
    lamb_list_iterator_t *it;

    queues = lamb_list_new();
    queues->free = free;

    if (!queues) {
        printf("\033[31m%s\033[0m\n", "Error: The kernel can't allocate memory\n");
        return;
    }

    err = lamb_get_queue(rdb, "mo", queues);

    if (err) {
        lamb_list_destroy(queues);
        printf("\033[31m%s\033[0m\n", "Error getting mo queue information");
        return;
    }

    if (queues->len < 1) {
        lamb_list_destroy(queues);
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-8s%-7s%-16s\n", "Id","Account","Total","Description");
    printf("---------------------------------------------------\n");
    printf("\033[37m");

    it = lamb_list_iterator_new(queues, LIST_HEAD);

    while ((node = lamb_list_iterator_next(it))) {
        q = (lamb_kv_t *)node->val;
        printf(" %3d", q->id);
        printf(" %-7.7s", q->acc);
        printf(" %-6ld", q->total);
        printf(" %-15.15s", q->desc);
        printf("\n");
        lamb_list_remove(queues, node);
    }

    printf("\033[0m\n");
    lamb_list_destroy(queues);
    lamb_list_iterator_destroy(it);

    return;
}

void lamb_show_core(const char *line) {
    int pid;
    int status;
    
    printf("\n");
    printf("%6s %-10s%-7s%-16s\n", "Pid","Module","Status","Description");
    printf("-----------------------------------------------------------\n");
    printf("\033[37m");

    /* ismg */
    lamb_check_status("/tmp/ismg.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "ismg",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m","client cmpp access gateway");

    /* mt */
    lamb_check_status("/tmp/mt.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "mt",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m", "core uplink message queue service");

    /* mo */
    lamb_check_status("/tmp/mo.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "mo",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m", "core downlink message queue service");

    /* scheduler */
    lamb_check_status("/tmp/scheduler.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "scheduler",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m", "core routing scheduler service");

    /* delivery */
    lamb_check_status("/tmp/delivery.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "delivery",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m", "core delivery scheduler service");

    /* testd */
    lamb_check_status("/tmp/testd.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "testd",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m", "carrier gateway test service");

    /* daemon */
    lamb_check_status("/tmp/daemon.lock", &pid, &status);
    printf(" %5d %-9.9s   %-6s   %-35s\n", pid, "daemon",
           status ? "\033[32mok\033[37m" : "\033[31mno\033[37m", "daemon management service");

    printf("\033[0m\n");

    return;
}

void lamb_show_client(const char *line) {
    char host[16];
    int len, status, speed, error;
    lamb_account_t *accounts[128] = {NULL};

    len = lamb_get_accounts(db, accounts, 128);

    if (len < 1) {
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-7s%-8s  %-16s%-7s%-6s%-6s\n",
           "Id", "User", "company", "Host", "Status", "Speed", "Error");
    printf("----------------------------------------------------------\n");
    printf("\033[37m");

    for (int i = 0; i < len; i++) {
        if (accounts[i]) {
            memset(host, 0, sizeof(host));
            lamb_check_client(rdb, accounts[i]->id, host, &status, &speed, &error);
            if (status) {
                printf(" %3d", accounts[i]->id);
                printf(" %-6.6s", accounts[i]->username);
                printf(" %-9d", accounts[i]->company);
                printf(" %-15.15s", host);
                printf("   %-6s  ", "\033[32mok\033[37m");
                printf(" %-5d", speed);
                printf(" %-5d", error);
                printf("\n");
            }
            free(accounts[i]);
        }
    }

    printf("\033[0m\n");
    return;
}

void lamb_show_server(const char *line) {
    int len, pid, status;
    lamb_server_statistics_t stat;
    lamb_account_t *accounts[128] = {NULL};

    len = lamb_get_accounts(db, accounts, 128);

    if (len < 1) {
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-6s%-7s%-6s%-6s%-6s%-6s%-6s%-6s%-6s%-6s\n",
           "Id", "Pid", "Status", "store", "bill", "blk", "usb", "limt", "rejt", "tmp", "key");
    printf("-----------------------------------------------------------------\n");
    printf("\033[37m");

    for (int i = 0; i < len; i++) {
        lamb_check_server(accounts[i]->id, &pid, &status);
        memset(&stat, 0, sizeof(lamb_server_statistics_t));
        lamb_server_statistics(rdb, accounts[i]->id, &stat);
        printf(" %3d", accounts[i]->id);
        printf(" %-5d", pid);
        printf("   %-6s  ", status ? "\033[32mok\033[37m" : "\033[31mno\033[37m");
        printf(" %-5ld", stat.store);
        printf(" %-5ld", stat.bill);
        printf(" %-5ld", stat.blk);
        printf(" %-5ld", stat.usb);
        printf(" %-5ld", stat.limt);
        printf(" %-5ld", stat.rejt);
        printf(" %-5ld", stat.tmp);
        printf(" %-5ld", stat.key);
        printf("\n");
        free(accounts[i]);
    }

    printf("\033[0m\n");
    return;
}

void lamb_show_account(const char *line) {
    int len;
    lamb_account_t *accounts[128] = {NULL};

    len = lamb_get_accounts(db, accounts, 128);

    if (len < 1) {
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

    if (len < 1) {
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
    if (channels->len < 1) {
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

    if (deliverys->len < 1) {
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

void lamb_show_channel(const char *line) {
    int len, status, speed, error;
    lamb_gateway_t *gateways[128] = {NULL};

    len = lamb_get_gateways(db, gateways, 128);

    if (len < 1) {
        printf(" There is no available data\n");
        return;
    }

    printf("\n");
    printf("%4s %-12s%-5s%-16s%-7s%-6s%-6s\n",
           "Id","Name", "Type", "Host","Status","Speed","Error");
    printf("----------------------------------------------------------\n");
    printf("\033[37m");

    for (int i = 0; i < len; i++) {
        if (gateways[i]) {
            lamb_check_channel(rdb, gateways[i]->id, &status, &speed, &error);
            printf(" %3d", gateways[i]->id);
            printf(" %-11.11s", gateways[i]->name);
            printf(" %-4.4s", "cmpp");
            printf(" %-15.15s", gateways[i]->host);
            printf("   %-6s  ", status ? "\033[32mok\033[37m" : "\033[31mno\033[37m");
            printf(" %-5d", speed);
            printf(" %-5d", error);
            printf("\n");
            free(gateways[i]);
        }
    }

    printf("\033[0m\n");
    return;
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
        if (strcasecmp(type, "?") == 0) {
            printf("\n");
            printf(" client       Client log information\n");
            printf(" server       Server log information\n");
            printf(" channel      Channel log information\n");
            printf(" mt           MT module log information\n");
            printf(" mo           MO module log information\n");
            printf(" scheduler    Scheduler log information\n");
            printf(" delivery     Delivery log information\n");
            printf(" testd        Test module log information\n");
            printf(" daemon       Daemon module log information\n");
            printf(" database     Database log information\n");
            printf("\n");
        } else if (strcasecmp(type, "client") == 0) {
            system("tail -n 100 /var/log/lamb-ismg.log");
        } else if (strcasecmp(type, "server") == 0) {
            system("tail -n 100 /var/log/lamb-server.log");
        } else if (strcasecmp(type, "channel") == 0) {
            system("tail -n 100 /var/log/lamb-gateway.log");
        } else if (strcasecmp(type, "mt") == 0) {
            system("tail -n 100 /var/log/lamb-mt.log");
        } else if (strcasecmp(type, "mo") == 0) {
            system("tail -n 100 /var/log/lamb-mo.log");
        } else if (strcasecmp(type, "scheduler") == 0) {
            system("tail -n 100 /var/log/lamb-scheduler.log");
        } else if (strcasecmp(type, "delivery") == 0) {
            system("tail -n 100 /var/log/lamb-delivery.log");
        } else if (strcasecmp(type, "testd") == 0) {
            system("tail -n 100 /var/log/lamb-testd.log");
        } else if (strcasecmp(type, "daemon") == 0) {
            system("tail -n 100 /var/log/lamb-daemon.log");
        } else if (strcasecmp(type, "database") == 0) {
            system("tail -n 100 /var/log/postgresql.log");
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
    }

    lamb_opt_free(&opt);
    return;
}

void lamb_start_channel(const char *line) {
    int id, err;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "start channel", &opt);

    if (err || !opt.val[0]) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);

    err = lamb_add_taskqueue(db, id, "sp", "sp.conf", "-d");

    if (err) {
        printf(" \033[31m%s\033[0m\n", "Start channel service failed");
    }

    lamb_opt_free(&opt);
    return;
}


void lamb_kill_client(const char *line) {
    int id, err;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "kill client", &opt);
    if (err || opt.len < 1) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);

    if (lamb_is_valid(rdb, "client", id)) {
        lamb_set_signal(rdb, "client", id, 9);
    } else {
        printf(" \033[31m%s\033[0m\n", "Sorry, the client does not exist");
    }
    
    lamb_opt_free(&opt);
    return;
}

void lamb_kill_server(const char *line) {
    int id, err;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "kill server", &opt);
    if (err || opt.len < 1) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);

    if (lamb_is_valid(rdb, "server", id)) {
        lamb_set_signal(rdb, "server", id, 9);
    } else {
        printf(" \033[31m%s\033[0m\n", "Sorry, the server does not exist");
    }
    
    lamb_opt_free(&opt);
    return;
}

void lamb_kill_channel(const char *line) {
    int id, err;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "kill channel", &opt);
    if (err || opt.len < 1) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    id = atoi(opt.val[0]);

    if (lamb_is_valid(rdb, "gateway", id)) {
        lamb_set_signal(rdb, "gateway", id, 9);
    } else {
        printf(" \033[31m%s\033[0m\n", "Sorry, the channel does not exist");
    }
    
    lamb_opt_free(&opt);
    return;
}

void lamb_check_blacklist(const char *line) {
    int err;
    char *phone;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(opt));
    err = lamb_opt_parsing(line, "check blacklist", &opt);
    if (err || opt.len < 1) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    phone = opt.val[0];

    if (lamb_is_blacklist(blk, phone)) {
        printf("The number %s is blacklisted\n", phone);
    } else {
        printf("The number %s is not blacklisted\n", phone);
    }

    lamb_opt_free(&opt);
    return;
}

void lamb_change_password(const char *line) {
    int err;
    char md5[33];
    char string[41];
    char *password;
    lamb_opt_t opt;

    memset(&opt, 0, sizeof(lamb_opt_t));
    err = lamb_opt_parsing(line, "change password", &opt);

    if (err || opt.len < 1) {
        printf(" \033[31m%s\033[0m\n", "Error: Incorrect command parameters");
        return;
    }

    password = opt.val[0];

    /* MD5 algorithm processing */
    lamb_md5(password, strlen(password), md5);

    /* SHA1 algorithm processing */
    lamb_sha1(md5, strlen(md5), string);

    err = lamb_set_password(rdb, string);

    if (err) {
        printf(" \033[31m%s\033[0m\n", "Error: change password failed\n");
    }

    lamb_opt_free(&opt);
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

    /* Blacklist database initialization */
    char *config[] = {
        "127.0.0.1:7001",
        "127.0.0.1:7002",
        "127.0.0.1:7003",
        "127.0.0.1:7004",
        "127.0.0.1:7005",
        "127.0.0.1:7006",
        "127.0.0.1:7007"
    };

    lamb_nodes_connect(blk, LAMB_MAX_CACHE, config, 7, 1);

    if (blk->len != 7) {
        printf("\033[31m%s\033[0m\n", "Error: can't connect to blacklist database\n");
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

int lamb_get_queue(lamb_cache_t *cache, const char *type, lamb_list_t *queues) {
    lamb_kv_t *q;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGETALL %s.queue", type);

    if (reply != NULL && reply->type == REDIS_REPLY_ARRAY) {
        for (int i = 0; i < reply->elements; i += 2) {
            q = (lamb_kv_t *)malloc(sizeof(lamb_kv_t));
            if (q) {
                q->id = reply->element[i]->str ? atoi(reply->element[i]->str) : 0;
                q->acc = "null";
                q->total = reply->element[i + 1]->str ? atoi(reply->element[i + 1]->str) : 0;
                q->desc = "no description";
                lamb_list_rpush(queues, lamb_node_new(q));
            }
        }

        freeReplyObject(reply);
    }

    return 0;
}

void lamb_md5(const void *data, unsigned long len, char *string) {
    MD5_CTX ctx;
    unsigned char digest[MD5_DIGEST_LENGTH];

    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    MD5_Final(digest, &ctx);

    lamb_hex_string(digest, sizeof(digest), string);

    return;
}

void lamb_sha1(const void *data, size_t len, char *string) {
    SHA_CTX ctx;
    unsigned char digest[SHA_DIGEST_LENGTH];

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    SHA1_Final(digest, &ctx);

    lamb_hex_string(digest, sizeof(digest), string);

    return;
}

void lamb_hex_string(unsigned char* digest, size_t len, char* string) {
    for (int i = 0; i < len; ++i) {
        sprintf(string + (i * 2), "%.2x", digest[i]);
    }
    string[len * 2] = '\0';

    return;
}

int lamb_set_password(lamb_cache_t *cache, const char *password) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HSET admin password %s", password);

    if (!reply) {
        return -1;
    }

    freeReplyObject(reply);
    
    return 0;
}

void lamb_check_status(const char *lock, int *pid, int *status) {
    char buf[16];
    char path[64];
    FILE *fp = NULL;

    *pid = *status = 0;
    fp = fopen(lock, "r");

    if (!fp) {
        return;
    }

    if (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
        *pid = atoi(buf);
        snprintf(path, sizeof(path), "/proc/%d", *pid);
        *status = (access(path, F_OK) == 0) ? 1 : 0;
    }

    fclose(fp);

    return;
}

void lamb_check_server(int id, int *pid, int *status) {
    char lock[128];

    snprintf(lock, sizeof(lock), "/tmp/serv-%d.lock", id);
    lamb_check_status(lock, pid, status);
    return;
}

void lamb_server_statistics(lamb_cache_t *cache, int id, lamb_server_statistics_t *stat) {
    redisReply *reply = NULL;

    if (!cache || id < 1) {
        return;
    }

    reply = redisCommand(cache->handle, "HMGET server.%d store bill blk usb limt rejt tmp key", id);

    if (reply) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            if (reply->elements == 8) {
                stat->store = reply->element[0]->str ? atol(reply->element[0]->str) : 0;
                stat->bill = reply->element[1]->str ? atol(reply->element[1]->str) : 0;
                stat->blk = reply->element[2]->str ? atol(reply->element[2]->str) : 0;
                stat->usb = reply->element[3]->str ? atol(reply->element[3]->str) : 0;
                stat->limt = reply->element[4]->str ? atol(reply->element[4]->str) : 0;
                stat->rejt = reply->element[5]->str ? atol(reply->element[5]->str) : 0;
                stat->tmp = reply->element[6]->str ? atol(reply->element[6]->str) : 0;
                stat->blk = reply->element[7]->str ? atol(reply->element[7]->str) : 0;
            }
        }
        freeReplyObject(reply);
    }

    return;
}

void lamb_check_channel(lamb_cache_t *cache, int id, int *status, int *speed, int *error) {
    char lock[128];
    int pid, stat;
    redisReply *reply = NULL;

    *status = *speed = *error = 0;
    snprintf(lock, sizeof(lock), "/tmp/gtw-%d.lock", id);
    lamb_check_status(lock, &pid, &stat);

    if (stat != 1) {
        return;
    }
    
    reply = redisCommand(cache->handle, "HMGET gateway.%d status speed error", id);
    
    if (reply) {
        if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 3)) {
            *status = (reply->element[0]->str != NULL) ? atoi(reply->element[0]->str) : 0;
            *speed = (reply->element[1]->str != NULL) ? atoi(reply->element[1]->str) : 0;
            *error = (reply->element[2]->str != NULL) ? atoi(reply->element[2]->str) : 0;
        }
        freeReplyObject(reply);
    }
    

    return;
}

void lamb_check_client(lamb_cache_t *cache, int id, char *host, int *status, int *speed, int *error) {
    long online = 0;
    redisReply *reply = NULL;

    *status = *speed = *error = 0;
    reply = redisCommand(cache->handle, "HMGET client.%d online addr speed error", id);
    if (reply) {
        if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 4)) {
            online = (reply->element[0]->str != NULL) ? atol(reply->element[0]->str) : 0;
            if ((time(NULL) - online) < 7) {
                *status = 1;
            }
            if (reply->element[1]->len > 15) {
                strncpy(host, reply->element[1]->str, 15);
            } else {
                strncpy(host, reply->element[1]->str, reply->element[1]->len);
            }
            
            *speed = (reply->element[2]->str != NULL) ? atoi(reply->element[2]->str) : 0;
            *error = (reply->element[3]->str != NULL) ? atoi(reply->element[3]->str) : 0;
        }
        freeReplyObject(reply);
    }

    return;
}

void lamb_set_signal(lamb_cache_t *cache, const char *type, int id, int signal) {
    redisReply *reply = NULL;

    if (!cache || id < 1) {
        return;
    }

    reply = redisCommand(cache->handle, "HSET %s.%d signal %d", type, id, signal);

    if (reply) {
        freeReplyObject(reply);
    }

    return;
}

bool lamb_is_valid(lamb_cache_t *cache, const char *type, int id) {
    bool exist = false;
    redisReply *reply = NULL;

    if (!cache || id < 1) {
        return false;
    }

    reply = redisCommand(cache->handle, "EXISTS %s.%d", type, id);
    if (reply) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            if (reply->integer == 1) {
                exist = true;
            }
        }
        freeReplyObject(reply);
    }

    return exist;
}

bool lamb_is_blacklist(lamb_caches_t *cache, char *phone) {
    int i = -1;
    bool r = false;
    unsigned long number;
    redisReply *reply = NULL;

    if (!phone) {
        return false;
    }

    number = atol(phone);

    if (number > 0) {
        i = (number % cache->len);
        reply = redisCommand(cache->nodes[i]->handle, "EXISTS %lu", number);
        if (reply != NULL) {
            if (reply->type == REDIS_REPLY_INTEGER) {
                if (reply->integer == 1) {
                    r = true;
                }
            }
            freeReplyObject(reply);
        }
    }

    return r;
    
}
