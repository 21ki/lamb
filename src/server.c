
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
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
#include "socket.h"
#include "keyword.h"
#include "security.h"
#include "channel.h"

#define LAMB_LIMIT   3
#define MAX_LIFETIME 60

static int aid;
static int mt, mo;
static int scheduler;
static int deliverd;
static lamb_status_t *status;
static lamb_config_t *config;
static lamb_global_t *global;
static lamb_caches_t *blacklist;
static lamb_caches_t *frequency;
static lamb_caches_t *unsubscribe;
static volatile bool sleeping = false;
static volatile bool arrears = false;

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
        fprintf(stderr, "Invalid account id number\n");
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
        return -1;
    }

    /* Check lock protection */
    int lock;
    char lockfile[128];

    snprintf(lockfile, sizeof(lockfile), "/tmp/serv-%d.lock", aid);

    if (lamb_lock_protection(&lock, lockfile)) {
        lamb_log(LOG_ERR, "Already started, please do not repeat the start!\n");
        return -1;
    }

    /* Save pid to file */
    lamb_pid_file(lock, getpid());

    /* Signal event processing */
    lamb_signal_processing();

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Start main event thread */
    lamb_event_loop();

    /* Release lock protection */
    lamb_lock_release(&lock);

    return 0;
}

void lamb_event_loop(void) {
    int err;

    lamb_set_process("lamb-server");

    err = lamb_component_initialization(config);
    if (err) {
        lamb_debug("component initialization failed\n");
        return;
    }

    /* Thread lock initialization */
    pthread_mutex_init(&global->lock, NULL);

    /* Start sender thread */
    lamb_start_thread(lamb_work_loop, NULL, 1);

    /* Start deliver thread */
    lamb_start_thread(lamb_deliver_loop, NULL, 1);
    
    /* Start billing thread */
    lamb_start_thread(lamb_billing_loop, NULL, 1);

    /* Start storage thread */
    lamb_start_thread(lamb_store_loop, NULL, 1);

    /* Start unsubscribe thread */
    lamb_start_thread(lamb_unsubscribe_loop, NULL, 1);

    /* Start status thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);

    /* Master control loop*/
    while (true) {
        lamb_sleep(3000);
    }

    return;
}

void lamb_reload(int signum) {
    int err;
    lamb_node_t *node;
    lamb_list_iterator_t *it;

    if (signal(SIGHUP, lamb_reload) == SIG_ERR) {
        lamb_log(LOG_ERR, "signal setting process failed\n");
    }

    sleeping = true;
    lamb_sleep(3000);

    /* fetch account information */
    err = lamb_account_fetch(&global->db, aid, &global->account);
    if (err) {
        lamb_log(LOG_ERR, "can't fetch account '%d' information", aid);
    }

    /* fetch company information */
    err = lamb_company_get(&global->db, global->account.company, &global->company);
    if (err) {
        lamb_log(LOG_ERR, "can't fetch id %d company information", global->account.company);
    }

    /* fetch template information */
    if (global->account.options & 1) {
        global->templates->free = free;
        it = lamb_list_iterator_new(global->templates, LIST_TAIL);
        while ((node = lamb_list_iterator_next(it))) {
            lamb_list_remove(global->templates, node);
        }

        lamb_list_iterator_destroy(it);

        err = lamb_get_template(&global->db, aid, global->templates);
        if (err) {
            lamb_log(LOG_ERR, "can't fetch template information");
        }
    }

    /* fetch keyword information */
    if (global->account.options & (1 << 1)) {
        global->keywords->free = free;
        it = lamb_list_iterator_new(global->keywords, LIST_TAIL);
        while ((node = lamb_list_iterator_next(it))) {
            lamb_list_remove(global->keywords, node);
        }
        err = lamb_keyword_get_all(&global->db, global->keywords);
        if (err) {
            lamb_log(LOG_ERR, "can't fetch keyword information");
        }
    }

    int len;
    char *bye;
    len = lamb_pack_assembly(&bye, LAMB_BYE, NULL, 0);
    if (len > 0) {
        nn_send(scheduler, bye, len, 0);
        nn_close(scheduler);

        /* connect to scheduler server */
    recont:
        scheduler = lamb_nn_pair(config->scheduler, aid, config->timeout);

        if (scheduler < 0) {
            lamb_log(LOG_ERR, "can't connect to scheduler %s", config->scheduler);
            goto recont;
        }

        free(bye);
    }
    
    sleeping = false;
    lamb_log(LOG_NOTICE, "reload the configuration complete");
    lamb_debug("-> reload the configuration complete\n");

    return;
}

void *lamb_work_loop(void *data) {
    int err;
    int rc, len;
    void *pk;
    char *buf;
    bool success;
    lamb_bill_t *bill;
    lamb_node_t *node;
    Submit *message;
    lamb_submit_t *storage;
    lamb_template_t *template;
    lamb_keyword_t *keyword;
    Report resp = REPORT__INIT;

    resp.account = global->account.id;
    resp.company = global->company.id;

    lamb_cpu_affinity(pthread_self());
    
    int rlen;
    char *req;

    rlen = lamb_pack_assembly(&req, LAMB_REQ, NULL, 0);

    while (true) {
        if (sleeping || arrears) {
            lamb_sleep(1000);
            continue;
        }

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

        message = submit__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));

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
            lamb_direct_response(mo, &resp, message, 7);
            goto done;
        }

        /* Check global blacklist */
        if (global->account.options & (1 << 4)) {
            if (lamb_check_blacklist(blacklist, message->phone)) {
                status->blk++;
                lamb_direct_response(mo, &resp, message, 7);
                goto done;
            }
        }
        /* Check user unsubscribe */
        if (global->account.options & (1 << 3)) {
            if (lamb_check_unsubscribe(unsubscribe, aid, message->phone)) {
                status->usb++;
                lamb_direct_response(mo, &resp, message, 7);
                goto done;
            }
        }

        /* Check limit frequency */
        if (global->account.options & (1 << 2)) {
            if (lamb_check_frequency(frequency, aid, message->phone)) {
                status->limt++;
                lamb_direct_response(mo, &resp, message, 7);
                goto done;
            }
        }

        if (fromcode != NULL) {
            content = (char *)malloc(512);

            if (!content) {
                lamb_direct_response(mo, &resp, message, 4);
                goto done;
            }

            /* Message coding conversion */
            err = lamb_encoded_convert((char *)message->content.data, message->length, content,
                                       512, fromcode, "UTF-8", &message->length);
            if (err || (message->length < 1)) {
                status->fmt++;
                free(content);
                lamb_direct_response(mo, &resp, message, 4);
                goto done;
            }

            message->msgfmt = 11;
            free(message->content.data);
            message->content.len = message->length;
            message->content.data = (uint8_t *)content;
        }

        /* Template Processing */
        if (global->account.options & 1) {
            success = false;
            lamb_list_iterator_t *ts;
            ts = lamb_list_iterator_new(global->templates, LIST_HEAD);

            while ((node = lamb_list_iterator_next(ts))) {
                template = (lamb_template_t *)node->val;
                if (lamb_check_content(template, (char *)message->content.data, message->content.len)) {
                    success = true;
                    break;
                }
            }

            if (!success) {
                status->tmp++;
                lamb_direct_response(mo, &resp, message, 7);
                goto done;
            }
        }

        /* Keywords Filtration */
        if (global->account.options & (1 << 1)) {
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
                lamb_direct_response(mo, &resp, message, 7);
                goto done;
            }
        }

        len = submit__get_packed_size(message);
        pk = malloc(len);

        if (!pk) {
            lamb_direct_response(mo, &resp, message, 4);
            goto done;
        }

        char *packet;
        submit__pack(message, pk);
        len = lamb_pack_assembly(&packet, LAMB_SUBMIT, pk, len);
        if (len < 1) {
            lamb_direct_response(mo, &resp, message, 4);
            goto done;
        }

        /* Scheduling */
        while (true) {
            rc = nn_send(scheduler, packet, len, 0);
            if (rc != len) {
                lamb_sleep(1000);
                continue;
            }

            rc = nn_recv(scheduler, &buf, NN_MSG, 0);
            if (rc < HEAD) {
                if (rc > 0) {
                    nn_freemsg(buf);
                }
                lamb_sleep(1000);
                continue;
            }

            if (CHECK_COMMAND(buf) == LAMB_OK) {
                nn_freemsg(buf);
                status->sub++;
                break;
            } else if (CHECK_COMMAND(buf) == LAMB_BUSY) {
                lamb_debug("-> the scheduler is busy!\n");
            } else if (CHECK_COMMAND(buf) == LAMB_NOROUTE) {
                nn_freemsg(buf);
                lamb_direct_response(mo, &resp, message, 4);
                lamb_debug("-> the scheduler is no route!\n");
                lamb_sleep(1000);
                goto done;
            } else if (CHECK_COMMAND(buf) == LAMB_REJECT) {
                status->rejt++;
                nn_freemsg(buf);
                lamb_direct_response(mo, &resp, message, 7);
                lamb_debug("-> the scheduler is rejected!\n");
                goto done;
            }

            lamb_sleep(100);
        }

        free(packet);
        free(pk);


        /* Save message to billing queue */
        bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
        if (bill) {
            bill->id = global->company.id;
            bill->money = -1;
            lamb_list_rpush(global->billing, lamb_node_new(bill));
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
        submit__free_unpacked(message, NULL);
    }

    free(req);
    pthread_exit(NULL);
}

void *lamb_deliver_loop(void *data) {
    void *pk;
    Report *rpack;
    Deliver *dpack;
    char *req, *buf;
    char spcode[21];
    lamb_bill_t *bill;
    int rc, len, rlen;

    Report report = REPORT__INIT;
    Deliver deliver = DELIVER__INIT;
    rlen = lamb_pack_assembly(&req, LAMB_REQ, NULL, 0);

    while (true) {
        if (sleeping) {
            lamb_sleep(1000);
            continue;
        }

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
            rpack = report__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!rpack) {
                continue;
            }

            if (rpack->status != 1) {
                bill = (lamb_bill_t *)malloc(sizeof(lamb_bill_t));
                if (bill) {
                    bill->id = global->company.id;
                    bill->money = 1;
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
            len = report__get_packed_size(&report);
            pk = malloc(len);

            if (pk) {
                report__pack(&report, pk);
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

            report__free_unpacked(rpack, NULL);
            continue;
        }

        if (CHECK_COMMAND(buf) == LAMB_DELIVER) {
            status->delv++;
            dpack = deliver__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
            nn_freemsg(buf);

            if (!dpack) {
                continue;
            }

            deliver.id = dpack->id;
            deliver.account = global->account.id;
            deliver.company = global->company.id;
            deliver.phone = dpack->phone;
            memset(spcode, 0, sizeof(spcode));
            snprintf(spcode, sizeof(spcode), "%s%s", global->account.spcode, dpack->serviceid);
            deliver.spcode = spcode;
            deliver.msgfmt = dpack->msgfmt;
            deliver.length = dpack->length;
            deliver.content.len = dpack->content.len;
            deliver.content.data = dpack->content.data;
            len = deliver__get_packed_size(&deliver);
            pk = malloc(len);

            if (pk) {
                deliver__pack(&deliver, pk);
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

            deliver__free_unpacked(dpack, NULL);
            continue;
        }

        nn_freemsg(buf);
    }

    pthread_exit(NULL);
}

void *lamb_store_loop(void *data) {
    int err;
    void *message;
    char *fromcode;
    char content[512];
    lamb_node_t *node;
    lamb_deliver_t *d;    

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
            d = (lamb_deliver_t *)message;
            switch (d->msgfmt) {
            case 0:
                fromcode = "ASCII";
                break;
            case 8:
                fromcode = "UCS-2BE";
                break;
            case 11:
                fromcode = "UTF-8";
                break;
            case 15:
                fromcode = "GBK";
                break;
            default:
                fromcode = NULL;
                break;
            }

            if (fromcode != NULL) {
                if (d->msgfmt != 11) {
                    /* message coding conversion */
                    memset(content, 0, sizeof(content));
                    err = lamb_encoded_convert(d->content, d->length, content, sizeof(content),
                                               fromcode, "UTF-8", &d->length);
                    if (err || (d->length < 1)) {
                        goto skip;
                    }

                    memset(d->content, 0, 160);
                    memcpy(d->content, content, d->length);
                }

                /* check unsubscribe content */
                if (global->account.options & (1 << 3)) {
                    if (lamb_check_unsubval(d->content, d->length)) {
                        lamb_list_rpush(global->unsubscribe, lamb_node_new(lamb_strdup(d->phone)));
                    }
                }
                /* save to message database */
                lamb_write_deliver(&global->mdb, d);
            }
        }

    skip:
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

void *lamb_unsubscribe_loop(void *arg) {
    int i;
    unsigned long phone;
    lamb_node_t *node;
    redisReply *reply = NULL;

    while (true) {
        node = lamb_list_lpop(global->unsubscribe);

        if (!node) {
            lamb_sleep(100);
            continue;
        }

        if (!node->val) {
            continue;
        }

        phone = atol((const char *)node->val);

        if (phone > 0) {
            i = (phone % unsubscribe->len);
            pthread_mutex_lock(&unsubscribe->nodes[i]->lock);
            reply = redisCommand(unsubscribe->nodes[i]->handle, "SET %d.%lu 1", aid, phone);
            pthread_mutex_unlock(&unsubscribe->nodes[i]->lock);

            if (reply) {
                freeReplyObject(reply);
            }
        }

        free(node->val);
        free(node);
    }

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *data) {
    int signal;

    while (true) {
        /* Check the arrears */
        arrears = lamb_check_arrears(&global->rdb, global->account.company);

        if (arrears) {
            lamb_log(LOG_WARNING, "company %d arrears, service has been temporarily stopped",
                     global->account.company);
        }

#ifdef _DEBUG
        /* Debug information */
        printf("store: %u, bill: %u, toal: %lld, sub: %llu, rep: %llu, delv: %llu, "
               "fmt: %llu, blk: %llu, tmp: %llu, key: %llu, usb: %llu, limt: %llu, rejt: %llu\n",
               global->storage->len, global->billing->len, status->toal, status->sub,
               status->rep, status->delv, status->fmt, status->blk, status->tmp,
               status->key, status->usb, status->limt, status->rejt);
#endif

        pthread_mutex_lock(&global->rdb.lock);
        signal = lamb_check_signal(&global->rdb, aid);
        pthread_mutex_unlock(&global->rdb.lock);
        if (!sleeping) {
            switch (signal) {
            case 1:
                lamb_reload(SIGHUP);
                break;
            case 9:
                lamb_exit_cleanup();
                break;
            }

        }

        lamb_sleep(3000);
    }

    
    pthread_exit(NULL);
}

int lamb_write_report(lamb_db_t *db, lamb_report_t *message) {
    char sql[512];
    PGresult *res = NULL;

    if (!message) {
        return -1;
    }

    snprintf(sql, sizeof(sql), "UPDATE message SET status = %d WHERE id = %lld",
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
    snprintf(sql, sizeof(sql), "INSERT INTO message(%s) VALUES(%lld, '%s', '%s', '%s', '%s', %d, %d, %d)",
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
    char *column;
    char sql[512];
    PGresult *res = NULL;

    if (!message) {
        return -1;
    }

    column = "id, spcode, phone, content, account, company";
    snprintf(sql, sizeof(sql), "INSERT INTO delivery(%s) VALUES(%lld, '%s', '%s', '%s', %d, %d)",
             column, (long long int)message->id, message->spcode, message->phone,
             message->content, message->account, message->company);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);

    return 0;
}

bool lamb_check_content(lamb_template_t *template, char *content, int len) {
    char pattern[512];

    memset(pattern, 0, sizeof(pattern));
    snprintf(pattern, sizeof(pattern), "^【%s】%s$", template->name, template->content);
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

bool lamb_check_unsubval(char *content, int len) {
    if (strstr(content, "t")) {
        return true;
    } else if (strstr(content, "T")) {
        return true;
    } else if (strstr(content, "td")) {
        return true;
    } else if (strstr(content, "TD")) {
        return true;
    } else if (strstr(content, "退")) {
        return true;
    } else if (strstr(content, "退订")) {
        return true;
    }

    return false;
}

bool lamb_check_blacklist(lamb_caches_t *cache, char *number) {
    int i = -1;
    bool r = false;
    unsigned long phone;
    redisReply *reply = NULL;

    phone = atol(number);

    if (phone > 0) {
        i = (phone % cache->len);
    }

    if (i < 0) {
        return r;
    }

    pthread_mutex_lock(&cache->nodes[i]->lock);
    reply = redisCommand(cache->nodes[i]->handle, "EXISTS %lu", phone);
    pthread_mutex_unlock(&cache->nodes[i]->lock);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            if (reply->integer == 1) {
                r = true;
            }
        }
        freeReplyObject(reply);
    }

    return r;
}

bool lamb_check_unsubscribe(lamb_caches_t *cache, int id, char *number) {
    int i = -1;
    bool r = false;
    unsigned long phone;
    redisReply *reply = NULL;

    phone = atol(number);

    if (phone > 0) {
        i = (phone % cache->len);
    }

    if (i < 0) {
        return r;
    }

    pthread_mutex_lock(&cache->nodes[i]->lock);
    reply = redisCommand(cache->nodes[i]->handle, "EXISTS %d.%lu", id, phone);
    pthread_mutex_unlock(&cache->nodes[i]->lock);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            if (reply->integer == 1) {
                r = true;
            }
        }
        freeReplyObject(reply);
    }

    return r;
}

bool lamb_check_frequency(lamb_caches_t *cache, int id, char *number) {
    int i = -1;
    bool r = false;
    unsigned long phone;
    redisReply *reply = NULL;

    phone = atol(number);

    if (phone > 0) {
        i = (phone % cache->len);
    }

    if (i < 0) {
        return r;
    }

    pthread_mutex_lock(&cache->nodes[i]->lock);
    reply = redisCommand(cache->nodes[i]->handle, "INCRBY %d.%lu 1", id, phone);
    pthread_mutex_unlock(&cache->nodes[i]->lock);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            if (reply->integer == 1) {
                freeReplyObject(reply);
                pthread_mutex_lock(&cache->nodes[i]->lock);
                reply = redisCommand(cache->nodes[i]->handle, "EXPIRE %d.%lu %d", id, phone, MAX_LIFETIME);
                pthread_mutex_unlock(&cache->nodes[i]->lock);
            } else if (reply->integer > LAMB_LIMIT) {
                r = true;
            }
        }
        freeReplyObject(reply);
    }

    return r;
}

bool lamb_check_arrears(lamb_cache_t *rdb, int company) {
    bool arrear = true;
    redisReply *reply = NULL;

    pthread_mutex_lock(&rdb->lock);
    reply = redisCommand(rdb->handle, "HGET company.%d money", company);
    pthread_mutex_unlock(&rdb->lock);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING && reply->len > 0) {
            if (atoi(reply->str) > 0) {
                arrear = false;
            }
        }
        freeReplyObject(reply);
    }

    return arrear;
}

void lamb_direct_response(int sock, Report *resp, Submit *message, int cause) {
    int len;
    void *pk;
    char *buf;

    resp->id = message->id;
    resp->phone = message->phone;
    resp->spcode = message->spcode;
    resp->status = cause;
    resp->submittime = "";
    resp->donetime = "";

    len = report__get_packed_size(resp);
    pk = malloc(len);

    if (pk) {
        report__pack(resp, pk);
        len = lamb_pack_assembly(&buf, LAMB_REPORT, pk, len);
        if (len > 0) {
            nn_send(sock, buf, len, NN_DONTWAIT);
            free(buf);
        }
        free(pk);
    }

    return;
}

void lamb_exit_cleanup(void) {
    lamb_nn_close(mt);
    lamb_nn_close(mo);
    lamb_nn_close(scheduler);
    lamb_nn_close(deliverd);
    lamb_db_close(&global->db);
    lamb_cache_close(&global->rdb);
    exit(EXIT_SUCCESS);

    return;
}

int lamb_component_initialization(lamb_config_t *cfg) {
    int err;

    if (!cfg) {
        return -1;
    }

    status = (lamb_status_t *)calloc(1, sizeof(lamb_status_t));
    global = (lamb_global_t *)calloc(1, sizeof(lamb_global_t));
    blacklist = (lamb_caches_t *)malloc(sizeof(lamb_caches_t));
    frequency = (lamb_caches_t *)malloc(sizeof(lamb_caches_t));
    unsubscribe = (lamb_caches_t *)malloc(sizeof(lamb_caches_t));

    if (!status || !global || !blacklist || !frequency || !unsubscribe) {
        return -1;
    }

    err = lamb_signal(SIGHUP, lamb_reload);
    lamb_debug("lamb signal initialization %s\n", err ? "failed" : "successfull");

    /* Storage Queue Initialization */
    global->storage = lamb_list_new();
    if (!global->storage) {
        lamb_log(LOG_ERR, "storage queue initialization failed");
        return -1;
    }

    lamb_debug("storage queue initialization successfull\n");
    
    /* Billing Queue Initialization */
    global->billing = lamb_list_new();
    if (!global->billing) {
        lamb_log(LOG_ERR, "billing queue initialization failed");
        return -1;
    }

    lamb_debug("billing queue initialization successfull\n");

    /* Unsubscribe queue initialization */
    global->unsubscribe = lamb_list_new();
    if (!global->unsubscribe) {
        lamb_log(LOG_ERR, "unsubscribe queue initialization failed");
        return -1;
    }

    lamb_debug("unsubscribe queue initialization successfull\n");

    /* Redis Initialization */
    err = lamb_cache_connect(&global->rdb, cfg->redis_host, cfg->redis_port,
                             NULL, cfg->redis_db);
    if (err) {
        lamb_log(LOG_ERR, "Can't connect to redis server");
        return -1;
    }

    lamb_debug("connect to redis server %s successfull\n", cfg->redis_host);

    /* Blacklist database initialization */
    lamb_nodes_connect(blacklist, LAMB_MAX_CACHE, cfg->nodes, 7, 1);
    if (blacklist->len != 7) {
        lamb_log(LOG_ERR, "connect to blacklist database failed");
        return -1;
    }

    lamb_debug("connect to blacklist database successfull\n");

    lamb_nodes_connect(unsubscribe, LAMB_MAX_CACHE, cfg->nodes, 7, 2);
    if (unsubscribe->len != 7) {
        lamb_log(LOG_ERR, "connect to unsubscribe database failed");
        return -1;
    }

    lamb_debug("connect to unsubscribe database successfull\n");

    lamb_nodes_connect(frequency, LAMB_MAX_CACHE, cfg->nodes, 7, 3);
    if (frequency->len != 7) {
        lamb_log(LOG_ERR, "connect to frequency database failed %d", frequency->len);
        return -1;
    }

    lamb_debug("connect to frequency database successfull\n");

    /* Postgresql Database  */
    err = lamb_db_init(&global->db);
    if (err) {
        lamb_log(LOG_ERR, "postgresql database initialization failed");
        return -1;
    }

    err = lamb_db_connect(&global->db, cfg->db_host, cfg->db_port,
                          cfg->db_user, cfg->db_password, cfg->db_name);
    if (err) {
        lamb_log(LOG_ERR, "Can't connect to postgresql database");
        return -1;
    }

    lamb_debug("connect to postgresql %s successfull\n", cfg->db_host);
    
    /* Postgresql Database  */
    err = lamb_db_init(&global->mdb);
    if (err) {
        lamb_log(LOG_ERR, "postgresql database initialization failed");
        return -1;
    }

    err = lamb_db_connect(&global->mdb, cfg->msg_host, cfg->msg_port,
                          cfg->msg_user, cfg->msg_password, cfg->msg_name);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to message database");
        return -1;
    }

    /* fetch  account */
    err = lamb_account_fetch(&global->db, aid, &global->account);
    if (err) {
        lamb_log(LOG_ERR, "can't fetch account '%d' information", aid);
        return -1;
    }

    lamb_debug("fetch account information successfull\n");

    /* Connect to MT server */
    mt = lamb_nn_reqrep(config->mt, aid, cfg->timeout);

    if (mt < 0) {
        lamb_log(LOG_ERR, "can't connect to MT %s", cfg->mt);
        return -1;
    }
    
    lamb_debug("connect to mt %s successfull\n", cfg->mt);
    
    /* Connect to MO server */
    mo = lamb_nn_pair(cfg->mo, aid, cfg->timeout);

    if (mo < 0) {
        lamb_log(LOG_ERR, "can't connect to MO %s", cfg->mo);
        return -1;
    }

    lamb_debug("connect to mo %s successfull\n", cfg->mo);

    /* Connect to Scheduler server */
    scheduler = lamb_nn_pair(cfg->scheduler, aid, cfg->timeout);

    if (scheduler < 0) {
        lamb_log(LOG_ERR, "can't connect to scheduler %s", cfg->scheduler);
        return -1;
    }

    lamb_debug("connect to scheduler %s successfull\n", cfg->scheduler);
    
    /* Connect to Deliver server */
    deliverd = lamb_nn_reqrep(cfg->deliver, aid, cfg->timeout);

    if (deliverd < 0) {
        lamb_log(LOG_ERR, "can't connect to deliver %s", cfg->deliver);
        return -1;
    }

    lamb_debug("connect to deliver %s successfull\n", cfg->deliver);

    /* Fetch company information */
    err = lamb_company_get(&global->db, global->account.company, &global->company);
    if (err) {
        lamb_log(LOG_ERR, "Can't fetch id %d company information", global->account.company);
        return -1;
    }

    lamb_debug("fetch company information successfull\n");
    
    /* Template information Initialization */
    global->templates = lamb_list_new();
    if (!global->templates) {
        lamb_log(LOG_ERR, "template queue initialization failed");
        return -1;
    }

    if (global->account.options & 1) {
        err = lamb_get_template(&global->db, aid, global->templates);
        if (err) {
            lamb_log(LOG_ERR, "Can't fetch template information");
            return -1;
        }
        lamb_debug("fetch template information successfull\n");
    }

    /* Keyword information Initialization */
    global->keywords = lamb_list_new();
    if (!global->keywords) {
        lamb_log(LOG_ERR, "keyword queue initialization failed");
        return -1;
    }

    if (global->account.options & (1 << 1)) {
        err = lamb_keyword_get_all(&global->db, global->keywords);
        if (err) {
            lamb_log(LOG_ERR, "Can't fetch keyword information");
            return -1;
        }
    }

    lamb_debug("fetch keywords information successfull\n");

    return 0;
}

int lamb_check_signal(lamb_cache_t *cache, int id) {
    int signal = 0;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGET server.%d signal", id);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING && reply->len > 0) {
            signal = atoi(reply->str);
        }
        freeReplyObject(reply);
    }

    /* Reset signal state */
    lamb_clear_signal(cache, id);

    return signal;
}

void lamb_clear_signal(lamb_cache_t *cache, int id) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HSET server.%d signal 0", id);

    if (reply != NULL) {
        freeReplyObject(reply);
    }

    return;
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

    if (lamb_get_string(&cfg, "Ac", conf->ac, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid AC server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Mt", conf->mt, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid MT server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Mo", conf->mo, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid MO server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Scheduler", conf->scheduler, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid scheduler server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Deliver", conf->deliver, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid deliver server address\n");
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


