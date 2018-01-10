
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sched.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <pcre.h>
#include <cmpp.h>
#include "server.h"
#include "config.h"
#include "socket.h"
#include "keyword.h"
#include "security.h"
#include "channel.h"
#include "command.h"
#include "message.h"

static int aid;
static int mt, mo;
static int scheduler;
static int deliverd;
static lamb_status_t *status;
static lamb_config_t *config;
static lamb_global_t *global;

int main(int argc, char *argv[]) {
    bool background = false;
    char *file = "server.conf";
    
    int opt = 0;
    char *optstring = "a:c:d";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'a':
            aid = atoi(optarg);
            break;
        case 'c':
            file = optarg;
            break;
        case 'd':
            background = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    if (aid < 1) {
        fprintf(stderr, "Invalid account number\n");
        return -1;
    }
    
    /* Read lamb configuration file */
    config = (lamb_config_t *)calloc(1, sizeof(lamb_config_t));
    if (lamb_read_config(config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (background) {
        lamb_daemon();
    }

    if (setenv("logfile", config->logfile, 1) == -1) {
        lamb_vlog(LOG_ERR, "setenv error: %s", strerror(errno));
        return -1;
    }

    /* Signal event processing */
    lamb_signal_processing();

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;

    lamb_set_process("lamb-server");

    status = (lamb_status_t *)calloc(1, sizeof(lamb_status_t));
    global = (lamb_global_t *)calloc(1, sizeof(lamb_global_t));

    err = lamb_signal(SIGHUP, lamb_reload);
    if (err) {
        lamb_debug("lamb signal initialization  failed\n");
    } else {
        lamb_debug("lamb signal initialization successfull\n");
    }

    /* Storage Queue Initialization */
    global->storage = lamb_list_new();
    if (!global->storage) {
        lamb_log(LOG_ERR, "storage queue initialization failed");
        return;
    }

    lamb_debug("storage queue initialization successfull\n");
    
    /* Billing Queue Initialization */
    global->billing = lamb_list_new();
    if (!global->billing) {
        lamb_log(LOG_ERR, "billing queue initialization failed");
        return;
    }

    lamb_debug("billing queue initialization successfull\n");
    
    /* Redis Initialization */
    err = lamb_cache_connect(&global->rdb, config->redis_host, config->redis_port,
                             NULL, config->redis_db);
    if (err) {
        lamb_log(LOG_ERR, "Can't connect to redis server");
        return;
    }

    lamb_debug("connect to cache server %s successfull\n", config->redis_host);
    
    /* Cache Cluster Initialization */
    lamb_nodes_connect(&global->cache, LAMB_MAX_CACHE, config->nodes, 7, 1);
    if (global->cache.len != 7) {
        lamb_log(LOG_ERR, "connect to cache cluster server failed");
        return;
    }

    lamb_debug("connect to cache node successfull\n");
    
    /* Postgresql Database  */
    err = lamb_db_init(&global->db);
    if (err) {
        lamb_log(LOG_ERR, "postgresql database initialization failed");
        return;
    }

    err = lamb_db_connect(&global->db, config->db_host, config->db_port,
                          config->db_user, config->db_password, config->db_name);
    if (err) {
        lamb_log(LOG_ERR, "Can't connect to postgresql database");
        return;
    }

    lamb_debug("connect to postgresql %s successfull\n", config->db_host);
    
    /* Postgresql Database  */
    err = lamb_db_init(&global->mdb);
    if (err) {
        lamb_log(LOG_ERR, "postgresql database initialization failed");
        return;
    }

    err = lamb_db_connect(&global->mdb, config->msg_host, config->msg_port,
                          config->msg_user, config->msg_password, config->msg_name);
    if (err) {
        lamb_log(LOG_ERR, "Can't connect to message database");
        return;
    }

    /* fetch  account */
    err = lamb_account_fetch(&global->db, aid, &global->account);
    if (err) {
        lamb_log(LOG_ERR, "Can't fetch account '%d' information", aid);
        return;
    }

    lamb_debug("fetch account information successfull\n");

    /* Connect to MT server */
    mt = lamb_nn_reqrep(config->mt_host, config->mt_port, aid, config->timeout);

    if (mt < 0) {
        lamb_log(LOG_ERR, "can't connect to MT %s server", config->mt_host);
        return;
    }

    
    lamb_debug("connect to mt %s successfull\n", config->mt_host);
    
    /* Connect to MO server */
    mo = lamb_nn_pair(config->mo_host, config->mo_port, aid, config->timeout);

    if (mo < 0) {
        lamb_log(LOG_ERR, "can't connect to MO %s server", config->mo_host);
        return;
    }

    lamb_debug("connect to mo %s successfull\n", config->mo_host);

    /* Connect to Scheduler server */
    scheduler = lamb_nn_pair(config->scheduler_host, config->scheduler_port, aid, config->timeout);

    if (scheduler < 0) {
        lamb_log(LOG_ERR, "can't connect to Scheduler %s server", config->scheduler_host);
        return;
    }

    lamb_debug("connect to scheduler %s successfull\n", config->scheduler_host);
    
    /* Connect to Deliver server */
    deliverd = lamb_nn_reqrep(config->deliver_host, config->deliver_port, aid, config->timeout);

    if (deliverd < 0) {
        lamb_log(LOG_ERR, "can't connect to deliver %s server", config->deliver_host);
        return;
    }

    lamb_debug("connect to deliver %s successfull\n", config->deliver_host);

    /* Fetch company information */
    err = lamb_company_get(&global->db, global->account.company, &global->company);
    if (err) {
        lamb_log(LOG_ERR, "Can't fetch id %d company information", global->account.company);
        return;
    }

    lamb_debug("fetch company information successfull\n");
    
    /* Template information Initialization */
    global->templates = lamb_list_new();
    if (!global->templates) {
        lamb_log(LOG_ERR, "template queue initialization failed");
        return;
    }

    err = lamb_get_template(&global->db, global->account.username, global->templates);
    if (err) {
        lamb_log(LOG_ERR, "Can't fetch template information");
        return;
    }

    lamb_debug("fetch template information successfull\n");

    /* debug templage */
    lamb_template_t *t;
    lamb_node_t *node = NULL;
    lamb_list_iterator_t *it = lamb_list_iterator_new(global->templates, LIST_HEAD);

    while ((node = lamb_list_iterator_next(it))) {
        t = (lamb_template_t *)node->val;
        lamb_debug("template -> id: %d, rexp: %s, name: %s, contents: %s\n", t->id, t->rexp, t->name, t->contents);
    }

    lamb_list_iterator_destroy(it);

    /* Keyword information Initialization */
    global->keywords = lamb_list_new();
    if (!global->keywords) {
        lamb_log(LOG_ERR, "keyword queue initialization failed");
        return;
    }

    if (global->account.keyword) {
        err = lamb_keyword_get_all(&global->db, global->keywords);
        if (err) {
            lamb_log(LOG_ERR, "Can't fetch keyword information");
        }
    }

    lamb_debug("fetch keywords information successfull\n");
    
    lamb_keyword_t *k;
    it = lamb_list_iterator_new(global->keywords, LIST_HEAD);

    while ((node = lamb_list_iterator_next(it))) {
        k = (lamb_keyword_t *)node->val;
        lamb_debug("keyword -> id: %d, val: %s\n", k->id, k->val);
    }

    lamb_list_iterator_destroy(it);

    /* Thread lock initialization */
    pthread_mutex_init(&global->lock, NULL);

    /* Message Table initialization */
    //lamb_new_table(&global->mdb);
    
    /* Start sender thread */
    //long cpus = lamb_get_cpu();
    //lamb_start_thread(lamb_work_loop, NULL, config->work_threads > cpus ? cpus : config->work_threads);

    lamb_start_thread(lamb_work_loop, NULL, 1);

    /* Start deliver thread */
    lamb_start_thread(lamb_deliver_loop, NULL, 1);
    
    /* Start billing thread */
    lamb_start_thread(lamb_billing_loop, NULL, 1);

    /* Start storage thread */
    lamb_start_thread(lamb_store_loop, NULL, 1);

    /* Start status thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);

    /* Master control loop*/
    while (true) {
        lamb_sleep(3000);
    }

    return;
}

void lamb_reload(int signum) {
    if (signal(SIGHUP, lamb_reload) == SIG_ERR) {
        printf("-> [signal] Can't setting SIGHUP signal to lamb_reload()\n");
    }
    printf("lamb reload configuration file success!\n");
    return;
}

void *lamb_work_loop(void *data) {
    int err;
    void *pk;
    char *buf;
    int rc, len;
    bool success;
    Submit *message;
    lamb_bill_t *bill;
    lamb_node_t *node;
    lamb_submit_t *storage;
    lamb_template_t *template;
    lamb_keyword_t *keyword;

    lamb_cpu_affinity(pthread_self());
    
    int rlen;
    char *req;

    rlen = lamb_pack_assembly(&req, LAMB_REQ, NULL, 0);

    while (true) {

        /* Request */
        rc = nn_send(mt, req, rlen, NN_DONTWAIT);

        if (rc != rlen) {
            lamb_sleep(1000);
            continue;
        }

        /* Response */
        rc = nn_recv(mt, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_EMPTY) {
            nn_freemsg(buf);
            lamb_debug("no available messages\n");
            lamb_sleep(1000);
            continue;
        }

        if (CHECK_COMMAND(buf) != LAMB_SUBMIT) {
            nn_freemsg(buf);
            continue;
        }

        message = lamb_submit_unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));

        if (!message) {
            nn_freemsg(buf);
            continue;
        }

        nn_freemsg(buf);
        status->toal++;

        /* Message Encoded Convert */
        char *content;
        char *fromcode;

        if (message->msgfmt == 0) {
            fromcode = "ASCII";
        } else if (message->msgfmt == 8) {
            fromcode = "UCS-2BE";
        } else if (message->msgfmt == 11) {
            fromcode = NULL;
        } else if (message->msgfmt == 15) {
            fromcode = "GBK";
        } else {
            status->fmt++;
            goto done;
        }

        if (fromcode != NULL) {
            content = (char *)malloc(512);

            if (!content) {
                goto done;
            }

            err = lamb_encoded_convert((char *)message->content.data, message->length, content,
                                       512, fromcode, "UTF-8", &message->length);
            if (err || (message->length < 1)) {
                status->fmt++;
                free(content);
                goto done;
            }

            message->msgfmt = 11;
            free(message->content.data);
            message->content.len = message->length;
            message->content.data = (uint8_t *)content;
        }

        /* 
           lamb_debug("id: %"PRIu64", phone: %s, spcode: %s, msgFmt: %d, content: %s, length: %d\n",
           message->id, message->phone, message->spcode, message->msgfmt, message->content.data, message->length);
        */

        /* Template Processing */
        if (global->account.template) {
            success = false;
            lamb_list_iterator_t *ts;
            ts = lamb_list_iterator_new(global->templates, LIST_HEAD);

            while ((node = lamb_list_iterator_next(ts))) {
                template = (lamb_template_t *)node->val;
                if (lamb_check_content(template, (char *)message->content.data, message->content.len)) {
                    lamb_debug("-> check template successfull\n");
                    success = true;
                    break;
                }
            }

            if (!success) {
                status->tmp++;
                goto done;
            }
        }

        /* Keywords Filtration */
        if (global->account.keyword) {
            success = true;
            lamb_list_iterator_t *ks;
            ks = lamb_list_iterator_new(global->keywords, LIST_HEAD);
            
            while ((node = lamb_list_iterator_next(ks))) {
                keyword = (lamb_keyword_t *)node->val;
                if (lamb_check_keyword(keyword, (char *)message->content.data)) {
                    success = false;
                    break;
                }
            }
               
            if (!success) {
                status->key++;
                goto done;
            }
        }

        /* Scheduling */
        len = lamb_submit_get_packed_size(message);
        pk = malloc(len);

        if (pk) {
            char *packet;
            lamb_submit_pack(message, pk);
            len = lamb_pack_assembly(&packet, LAMB_SUBMIT, pk, len);
            if (len > 0) {
                while (true) {
                    rc = nn_send(scheduler, packet, len, 0);
                    if (rc == len) {
                        rc = nn_recv(scheduler, &buf, NN_MSG, 0);
                        if (rc >= HEAD) {
                            if (CHECK_COMMAND(buf) == LAMB_OK) {
                                nn_freemsg(buf);
                                status->sub++;
                                break;
                            } else if (CHECK_COMMAND(buf) == LAMB_BUSY) {
                                lamb_debug("-> the scheduler is busy!\n");
                            }
                        }

                        if (rc > 0) {
                            nn_freemsg(buf);
                        }
                    }
                    lamb_sleep(1000);
                }
                free(packet);
            }
            free(pk);
        }

        /* Save message to billing queue */
        if (global->account.charge == LAMB_CHARGE_SUBMIT) {
            bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
            if (bill) {
                bill->id = global->company.id;
                bill->money = -1;
                lamb_list_rpush(global->billing, lamb_node_new(bill));
            }
        }

        /* Save message to storage queue */
        storage = (lamb_submit_t *)calloc(1, sizeof(lamb_submit_t));
        if (storage) {
            storage->type = LAMB_SUBMIT;
            storage->id = message->id;
            storage->account = global->account.id;
            storage->company = global->company.id;
            strncpy(storage->spid, message->spid, 6);
            strncpy(storage->spcode, message->spcode, 20);
            strncpy(storage->phone, message->phone, 11);
            storage->msgfmt = message->msgfmt;
            storage->length = message->length;
            memcpy(storage->content, message->content.data, message->content.len);
            lamb_list_rpush(global->storage, lamb_node_new(storage));
        }

    done:
        lamb_submit_free_unpacked(message, NULL);
    }

    free(req);
    pthread_exit(NULL);
}

void *lamb_deliver_loop(void *data) {
    void *pk;
    Report *rpack;
    Deliver *dpack;
    char *req, *buf;
    char spcode[24];
    lamb_bill_t *bill;
    int rc, len, rlen, money;

    Report report = LAMB_REPORT_INIT;
    Deliver deliver = LAMB_DELIVER_INIT;
    rlen = lamb_pack_assembly(&req, LAMB_REQ, NULL, 0);

    while (true) {
        rc = nn_send(deliverd, req, rlen, NN_DONTWAIT);

        if (rc != rlen) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(deliverd, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }
            lamb_sleep(1000);
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_EMPTY) {
            nn_freemsg(buf);
            lamb_sleep(1000);
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_REPORT) {
            status->rep++;
            rpack = lamb_report_unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!rpack) {
                continue;
            }

            money = 0;

            if (rpack->status == 1) {
                if (global->account.charge == LAMB_CHARGE_SUCCESS) {
                    lamb_debug("account->charge lamb charge success\n");
                    money = -1;
                }
            } else {
                if (global->account.charge == LAMB_CHARGE_SUBMIT) {
                    lamb_debug("account->charge lamb charge submit\n");
                    money = 1;
                }
            }

            if (money != 0) {
                bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
                if (bill) {
                    bill->id = global->company.id;
                    bill->money = money;
                    lamb_list_rpush(global->billing, lamb_node_new(bill));
                }
            }

            report.id = rpack->id;
            report.account = rpack->account;
            report.company = rpack->company;
            report.spcode = rpack->spcode;
            report.phone = rpack->phone;
            report.status = rpack->status;
            report.submittime = rpack->submittime;
            report.donetime = rpack->donetime;
            len = lamb_report_get_packed_size(&report);
            pk = malloc(len);

            if (pk) {
                lamb_report_pack(&report, pk);
                len = lamb_pack_assembly(&buf, LAMB_REPORT, pk, len);
                if (len > 0) {
                    nn_send(mo, buf, len, 0);
                    free(buf);
                }
                free(pk);
            }

            /* Store report to database */
            lamb_report_t *r;
            r = (lamb_report_t *)calloc(1, sizeof(lamb_report_t));

            if (r) {
                r->type = LAMB_REPORT;
                r->id = report.id;
                r->account = report.account;
                r->company = report.company;
                strncpy(r->phone, report.phone, 11);
                strncpy(r->spcode, report.spcode, 20);
                r->status = report.status;
                strncpy(r->submittime, report.submittime, 10);
                strncpy(r->donetime, report.donetime, 10);
                lamb_list_rpush(global->storage, lamb_node_new(r));
            }

            lamb_report_free_unpacked(rpack, NULL);
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_DELIVER) {
            status->delv++;
            dpack = lamb_deliver_unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!dpack) {
                continue;
            }

            deliver.id = dpack->id;
            deliver.account = global->account.id;
            deliver.company = global->company.id;
            deliver.phone = dpack->phone;
            memset(spcode, 0, sizeof(spcode));
            sprintf(spcode, "%s%s", global->account.spcode, dpack->spcode);
            deliver.spcode = spcode;
            deliver.msgfmt = dpack->msgfmt;
            deliver.length =dpack->length;
            deliver.content.len = dpack->content.len;
            deliver.content.data = dpack->content.data;
            len = lamb_deliver_get_packed_size(&deliver);
            pk = malloc(len);

            if (pk) {
                lamb_deliver_pack(&deliver, pk);
                len = lamb_pack_assembly(&buf, LAMB_DELIVER, pk, len);
                if (len > 0) {
                    nn_send(mo, buf, len, 0);
                    free(buf);
                }
                free(pk);
            }

            lamb_deliver_t *d;
            d = (lamb_deliver_t *)calloc(1, sizeof(lamb_deliver_t));

            if (d) {
                d->type = LAMB_DELIVER;
                d->id = deliver.id;
                d->account = global->account.id;
                d->company = global->company.id;
                strncpy(d->phone, deliver.phone, 11);
                strncpy(d->spcode, deliver.spcode, 20);
                strncpy(d->serviceid, deliver.serviceid, 10);
                d->msgfmt = deliver.msgfmt;
                d->length = deliver.length;
                memcpy(d->content, deliver.content.data, deliver.content.len);
                lamb_list_rpush(global->storage, lamb_node_new(d));  
            }

            lamb_deliver_free_unpacked(dpack, NULL);
            continue;
        }

        nn_freemsg(buf);
    }

    pthread_exit(NULL);
}

void *lamb_store_loop(void *data) {
    void *message;
    lamb_node_t *node;
    
    while (true) {
        node = lamb_list_lpop(global->storage);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        message = node->val;

        if (CHECK_TYPE(message) == LAMB_SUBMIT) {
            lamb_write_message(&global->mdb, (lamb_submit_t *)message);
        } else if (CHECK_TYPE(message) == LAMB_REPORT) {
            lamb_write_report(&global->mdb, (lamb_report_t *)message);
        } else if (CHECK_TYPE(message) == LAMB_DELIVER) {
            lamb_write_deliver(&global->mdb, (lamb_deliver_t *)message);
        }

        free(node);
        free(message);
    }

    pthread_exit(NULL);
}

void *lamb_billing_loop(void *data) {
    int err;
    lamb_bill_t *bill;
    lamb_node_t *node;
    
    while (true) {
        node = lamb_list_lpop(global->billing);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        bill = (lamb_bill_t *)node->val;
        pthread_mutex_lock(&(global->rdb.lock));
        err = lamb_company_billing(&global->rdb, bill->id, bill->money);
        pthread_mutex_unlock(&(global->rdb.lock));
        if (err) {
            lamb_log(LOG_ERR, "Account %d billing money %d failure", bill->id, bill->money);
        }

        free(bill);
        free(node);
    }

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *data) {
    // int err;
    redisReply *reply = NULL;

    while (true) {
        pthread_mutex_lock(&(global->rdb.lock));
        reply = redisCommand(global->rdb.handle, "HMSET server.%d pid %u", aid, getpid());
        pthread_mutex_unlock(&(global->rdb.lock));
        if (reply != NULL) {
            freeReplyObject(reply);
        }

        lamb_debug("store: %u, bill: %u, toal: %lld, sub: %llu, rep: %llu, delv: %llu, fmt: %llu, blk: %llu, tmp: %llu, key: %llu\n",
                   global->storage->len, global->billing->len, status->toal, status->sub, status->rep, status->delv, status->fmt, status->blk, status->tmp, status->key);

        sleep(3);
    }

    
    pthread_exit(NULL);
}

int lamb_write_report(lamb_db_t *db, lamb_report_t *message) {
    char sql[512];
    PGresult *res = NULL;

    if (!message) {
        return -1;
    }

    sprintf(sql, "UPDATE message SET status = %d WHERE id = %lld",
            message->status, (long long)message->id);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);

    return 0;
}

int lamb_write_message(lamb_db_t *db, lamb_submit_t *message) {
    char *column;
    char sql[512];
    PGresult *res = NULL;

    if (!message) {
        return -1;
    }

    column = "id, spid, spcode, phone, content, status, account, company";
    sprintf(sql, "INSERT INTO message(%s) VALUES(%lld, '%s', '%s', '%s', '%s', %d, %d, %d)",
            column, (long long int)message->id, message->spid, message->spcode, message->phone,
            message->content, 0, message->account, message->company);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);

    return 0;
}

int lamb_write_deliver(lamb_db_t *db, lamb_deliver_t *message) {
    int err;
    char *column;
    char sql[512];
    char *fromcode;
    char content[512];
    PGresult *res = NULL;

    if (!message) {
        return -1;
    }

    switch (message->msgfmt) {
    case 0:
        fromcode = "ASCII";
        break;
    case 8:
        fromcode = "UCS-2BE";
        break;
    case 11:
        fromcode = NULL;
        break;
    case 15:
        fromcode = "GBK";
        break;
    default:
        return -1;
    }

    if (fromcode != NULL) {
        memset(content, 0, sizeof(content));
        err = lamb_encoded_convert(message->content, message->length, content, sizeof(content),
                                   fromcode, "UTF-8", &message->length);
        if (err || (message->length < 1)) {
            return -1;
        }
    }

    column = "id, spcode, phone, content, account, company";
    sprintf(sql, "INSERT INTO delivery(%s) VALUES(%lld, '%s', '%s', '%s', %d, %d)",
            column, (long long int)message->id, message->spcode,
            message->phone, fromcode ? content : message->content,
            message->account, message->company);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);

    return 0;
}

int lamb_spcode_process(char *code, char *spcode, size_t size) {
    size_t len;

    len = strlen(code);
    
    if (len > strlen(spcode)) {
        memcpy(spcode, code, len >= size ? (size - 1) : len);
        return 0;
    }

    for (int i = 0; (i < len) && (i < (size - 1)); i++) {
        spcode[i] = code[i];
    }

    return 0;
}

int lamb_fetch_channels(lamb_db_t *db, const char *rexp, lamb_list_t *channels) {
    int rows;
    char sql[512];
    PGresult *res;
    
    sprintf(sql, "SELECT * FROM channels WHERE gid = (SELECT target FROM routing WHERE rexp = '%s') ORDER BY weight ASC ", rexp);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    rows = PQntuples(res);

    for (int i = 0; i < rows; i++) {
        lamb_channel_t *c = NULL;
        c = (lamb_channel_t *)calloc(1, sizeof(lamb_channel_t));
        if (c != NULL) {
            c->id = atoi(PQgetvalue(res, i, 0));
            c->gid = atoi(PQgetvalue(res, i, 1));
            c->weight = atoi(PQgetvalue(res, i, 2));
            lamb_list_rpush(channels, lamb_node_new(c));
        }
    }

    PQclear(res);

    return 0;
}

bool lamb_check_content(lamb_template_t *template, char *content, int len) {
    char pattern[512];

    memset(pattern, 0, sizeof(pattern));
    sprintf(pattern, "^【%s】%s$", template->name, template->contents);
    if (lamb_pcre_regular(pattern, content, len)) {
        return true;
    }

    return false;
}

bool lamb_check_keyword(lamb_keyword_t *key, char *content) {
    if (strstr(content, key->val)) {
        return true;
    }

    return false;
}

int lamb_read_config(lamb_config_t *conf, const char *file) {
    if (!conf) {
        return -1;
    }

    config_t cfg;
    if (lamb_read_file(&cfg, file) != 0) {
        fprintf(stderr, "ERROR: Can't open the %s configuration file\n", file);
        goto error;
    }

    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Id' parameter\n");
        goto error;
    }
    
    if (lamb_get_bool(&cfg, "Debug", &conf->debug) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Debug' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "WorkThreads", &conf->work_threads) != 0) {
        fprintf(stderr, "ERROR: Can't read 'WorkThreads' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'LogFile' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "RedisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPort' parameter\n");
        goto error;
    }

    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "ERROR: Invalid redis port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "RedisPassword", conf->redis_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPassword' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisDb' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MtHost", conf->mt_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MT server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MtPort", &conf->mt_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MtPort' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MoHost", conf->mo_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MO server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MoPort", &conf->mo_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MoPort' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "SchedulerHost", conf->scheduler_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid scheduler server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "SchedulerPort", &conf->scheduler_port) != 0) {
        fprintf(stderr, "ERROR: Can't read scheduler server port parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DeliverHost", conf->deliver_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid deliver server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DeliverPort", &conf->deliver_port) != 0) {
        fprintf(stderr, "ERROR: Can't read deliver server port parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPort' parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid DB port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbName' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgHost", conf->msg_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MsgPort", &conf->msg_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgPort' parameter\n");
        goto error;
    }

    if (conf->msg_port < 1 || conf->msg_port > 65535) {
        fprintf(stderr, "ERROR: Invalid MsgPort number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgUser", conf->msg_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MsgPassword", conf->msg_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "MsgName", conf->msg_name, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MsgName' parameter\n");
        goto error;
    }

    char node[32];
    memset(node, 0, sizeof(node));

    if (lamb_get_string(&cfg, "node1", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node1' parameter\n");
        goto error;
    }
    conf->nodes[0] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node2", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node2' parameter\n");
        goto error;
    }
    conf->nodes[1] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node3", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node3' parameter\n");
        goto error;
    }
    conf->nodes[2] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node4", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node4' parameter\n");
        goto error;
    }
    conf->nodes[3] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node5", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node5' parameter\n");
        goto error;
    }
    conf->nodes[4] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node6", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node1' parameter\n");
        goto error;
    }
    conf->nodes[5] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node7", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node7' parameter\n");
        goto error;
    }
    conf->nodes[6] = lamb_strdup(node);

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}


