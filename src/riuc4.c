#include "node.h"
#include <stdio.h>
#include "ansi-utils.h"
#include "my-pjlib-utils.h"
#include "gb-sender.h"
#include "proto-constants.h"
#include "riuc4_uart.h"

#include <sqlite3.h>
#include "mysqlite.h"

typedef struct riuc_data_s riuc_data_t;

#define MAX_NODE 4

int auto_invite = 0;

struct riuc_data_s {
    node_t node[MAX_NODE];
    gb_sender_t gb_sender;
    riuc4_t riuc4;
    serial_t serial;

    char serial_file[30];
};

riuc_data_t riuc_data;

void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus) {
    SHOW_LOG(4, "on_riuc4_status port:%d, signal:%s\n", port, RIUC4_SIGNAL_NAME[signal]);

    static pj_thread_desc s_desc;
    static pj_thread_t *s_thread;
    ANSI_CHECK(__FILE__, pj_thread_register("adv_server", s_desc, &s_thread));

    switch(signal) {
        case RIUC_SIGNAL_SQ:
            SHOW_LOG(3, "Received SQ signal\n");
            gb_sender_report_sq(&riuc_data.gb_sender, riuc_data.node[port].id, port, ustatus->sq);
            if (ustatus->sq == 1) {
                node_start_session(&riuc_data.node[port]);
                gb_sender_report_rx(&riuc_data.gb_sender, riuc_data.node[port].id, port, 1);
            }
            else {
                node_stop_session(&riuc_data.node[port]);
                gb_sender_report_rx(&riuc_data.gb_sender, riuc_data.node[port].id, port, 0);
            }
            break;
        case RIUC_SIGNAL_PTT:
            SHOW_LOG(3, "Received PTT signal - node[%d]\n", port);
            gb_sender_report_tx(&riuc_data.gb_sender, riuc_data.node[port].id, port, ustatus->ptt);
            break;
        case RIUC_SIGNAL_RX:
            SHOW_LOG(3, "Received RX signal\n");
            break;
        case RIUC_SIGNAL_TX:
            SHOW_LOG(3, "Received TX signal\n");
            break;
        default:
            EXIT_IF_TRUE(1, "Unknow signal\n");
    } 
}

void on_adv_info_riuc(adv_server_t *adv_server, adv_request_t *request, char *caddr_str) {
    node_t *node = adv_server->user_data;

    int i, found;
    int idx; 

    for (i = 0; i < MAX_NODE; i++) {
        SHOW_LOG(3, "LIST TABLE\n");
        ht_list_item(&node[i].group_table);
        SHOW_LOG(3, "=========== NODE :%s ========\n", node[i].id);
        found = node_in_group(&node[i], "OIUC-FTW");
        SHOW_LOG(3, "node_id: %s, adv_owner:%s, found:%d \n", node[i].id, "OIUC-FTW", found);
        found = node_in_group(&node[i], "OIUC-UBUNTU");
        SHOW_LOG(3, "node_id: %s, adv_owner:%s, found:%d \n", node[i].id, "OIUC-UBUNTU", found);
        SHOW_LOG(3, "=============================\n");
    }

    for (i = 0; i < MAX_NODE; i++) {
        found = node_in_group(&node[i], request->adv_info.adv_owner);
    
        if (found >= 0) {
            SHOW_LOG(3, "New session: %s(%s:%d)\n", request->adv_info.adv_owner, request->adv_info.sdp_mip, request->adv_info.sdp_port);

            if(!node_has_media(&node[i])) {
                SHOW_LOG(1, "Node does not have media endpoints configured\n");
                return;
            }

            idx = ht_get_item(&node[i].group_table, request->adv_info.adv_owner);
            SHOW_LOG(3, "idx(ht_get_item): %d for owner: %s\n", idx, request->adv_info.adv_owner);
#if 1
            if( request->adv_info.sdp_port > 0 ) {
                receiver_stop(node[i].receiver, idx);

                //for (i = 0; i < node->receiver->nstreams; i++) {
                    receiver_config_stream(node[i].receiver, request->adv_info.sdp_mip, request->adv_info.sdp_port, idx);
                //}

                receiver_start(node[i].receiver);
                riuc4_on_ptt(&riuc_data.riuc4, node[i].radio_port);
            }
            else {
                receiver_stop(node[i].receiver, idx);
                riuc4_off_ptt(&riuc_data.riuc4, node[i].radio_port);
            }
            usleep(250*1000);
#endif
        }
    }
}

void on_leaving_server(char *owner_id, char *adv_ip) {
    int i, ret;
    int count = 0;

    for (i = 0; i < MAX_NODE; i++) {
        ret = node_in_group(&riuc_data.node[i], owner_id);
        if (ret < 0) {
            count++;
        }
    }

    if (count == MAX_NODE) {
        SHOW_LOG(3, "Leave %s\n", adv_ip);
        adv_server_leave(riuc_data.node[0].adv_server, adv_ip);
    }
}

static void init_adv_server(adv_server_t *adv_server, char *adv_cs, pj_pool_t *pool) {
    memset(adv_server, 0, sizeof(*adv_server));

    adv_server->on_request_f = &on_adv_info_riuc;
    adv_server->on_open_socket_f = &on_open_socket_adv_server;
    adv_server->user_data = riuc_data.node;

    adv_server_init(adv_server, adv_cs, pool, NULL);
    adv_server_start(adv_server);
}

void *auto_register(void *riuc_data) {
    int i;
    int loop = 1;
    riuc_data_t *riuc = (riuc_data_t *)riuc_data;   
    while (1) {
        for (i = 0; i < MAX_NODE; i++) {
            node_register(&riuc->node[i]);
            if (loop) {
                node_invite(&riuc->node[i], "OIUC");
            }
        }
        if (!auto_invite) {
            loop = 0;
        }

        usleep(5*1000*1000);
    }
}

void usage(char *app) {
    printf("usage: %s <serial_file> <database_file>\n", app);
    exit(-1);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
    }

    /*------------ CONFIG VARIABLES ------------*/
    sqlite3 *db;
    char *sql, sql_cmd[100];
    sqlite3_stmt *stmt;

    char id[10], location[30], desc[50];
    char gm_cs[50], gmc_cs[50], adv_cs[50], gb_cs[50];
    char gm_cs_tmp[50], gmc_cs_tmp[50], adv_cs_tmp[50];

    int snd_dev_r0, snd_dev_r1, snd_dev_r2, snd_dev_r3;

    int adv_port = ADV_PORT;
    int gb_port = GB_PORT; 

    /*------------ INIT & STREAM VARIABLES ------------*/
    pj_caching_pool cp;
    pj_pool_t *pool;
    pjmedia_endpt *ep;

    endpoint_t streamers[MAX_NODE];
    endpoint_t receivers[MAX_NODE];
    adv_server_t adv_server;
    
    /*------------ OTHER VARIABLES ------------*/
    pthread_t thread;
    char *dummy, option[10];
    int i, n, input, f_quit = 0;
    int snd_dev[4];

    /*-----------------------------------------*/

    SET_LOG_LEVEL(4);

    /*------------ START ------------*/
#if 1
    SHOW_LOG(3, "Press '1': Set sound devices configure\nPress 's': Show databases\nPress 'Space': Load databases\nPress 'q': Quit\n");

    CALL_SQLITE (open (argv[2], &db));
    while(!f_quit) {
        dummy = fgets(option, sizeof(option), stdin);
        switch(option[0]) {
            case '1':
                SHOW_LOG(3, "Set device index for each radio...\n");
                for (i = 0; i < MAX_NODE; i++){
                    SHOW_LOG(3, "Radio %d: ", i);
                    dummy = fgets(option, sizeof(option), stdin);           
                    input = atoi(&option[0]);
                    n = sprintf(sql_cmd, "UPDATE riuc_config SET snd_dev_r%d =(?)", i);
                    sql_cmd[n] = '\0';
                    CALL_SQLITE (prepare_v2 (db, sql_cmd, strlen (sql_cmd) + 1, & stmt, NULL));
                    CALL_SQLITE (bind_int (stmt, 1, input));
                    CALL_SQLITE_EXPECT (step (stmt), DONE);
                }
                SHOW_LOG(3, "Config completed\n");               
                f_quit = 1;
                break;
            case 's':
                sql = "SELECT *FROM riuc_config";
                CALL_SQLITE (prepare_v2 (db, sql, strlen (sql) + 1, &stmt, NULL));

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    printf("ID: %s\n", sqlite3_column_text(stmt, 0));
                    printf("Location: %s\n", sqlite3_column_text(stmt, 1));
                    printf("Desc: %s\n", sqlite3_column_text(stmt, 2));
                    printf("GM_CS: %s\n", sqlite3_column_text(stmt, 3));
                    printf("GMC_CS: %s\n", sqlite3_column_text(stmt, 4));
                    printf("snd_dev_r0: %u\n", sqlite3_column_int(stmt, 5));
                    printf("snd_dev_r1: %u\n", sqlite3_column_int(stmt, 6));
                    printf("snd_dev_r2: %u\n", sqlite3_column_int(stmt, 7));
                    printf("snd_dev_r3: %u\n", sqlite3_column_int(stmt, 8));
                }
                break;
            case ' ':
                f_quit = 1;
                break;
            case 'q':
                return 0;
            default:
                SHOW_LOG(3, "Unknown option\n");
        }
    }
    f_quit = 0;
#endif
    /*------------ LOAD CONFIG ------------*/
    //CALL_SQLITE (open ("databases/riuc.db", &db));
    sql = "SELECT * FROM riuc_config";
    CALL_SQLITE (prepare_v2 (db, sql, strlen (sql) + 1, &stmt, NULL));

    //WARNING: MAX NUMBER OF SOUND DEV = 4
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        strcpy(id, sqlite3_column_text(stmt, 0));
        strcpy(location, sqlite3_column_text(stmt, 1));
        strcpy(desc, sqlite3_column_text(stmt, 2));
        strcpy(gm_cs, sqlite3_column_text(stmt, 3));
        strcpy(gmc_cs, sqlite3_column_text(stmt, 4));
        snd_dev_r0 = sqlite3_column_int(stmt, 5);
        snd_dev_r1 = sqlite3_column_int(stmt, 6);
        snd_dev_r2 = sqlite3_column_int(stmt, 7);
        snd_dev_r3 = sqlite3_column_int(stmt, 8);
        auto_invite = sqlite3_column_int(stmt, 9);
    }

    snd_dev[0] = snd_dev_r0;
    snd_dev[1] = snd_dev_r1;
    snd_dev[2] = snd_dev_r2;
    snd_dev[3] = snd_dev_r3;

    n = sprintf(adv_cs, "udp:0.0.0.0:%d", adv_port);
    adv_cs[n] = '\0';
    n = sprintf(gb_cs, "udp:%s:%d",GB_MIP, gb_port);
    gb_cs[n] = '\0';
    
    SHOW_LOG(3, "========= LOADED CONFIG ========\n");
    SHOW_LOG(3, "ID: %s\nDesc: %s\nGM_CS: %s\nGMC_CS: %s\nADV_CS: %s\nGB_CS: %s\nsnd_r0: %d\nsnd_r1: %d\nsnd_r2: %d\nsnd_r3: %dAuto invite: %d\n", id, desc, gm_cs, gmc_cs, adv_cs, gm_cs, snd_dev_r0, snd_dev_r1, snd_dev_r2, snd_dev_r3, auto_invite);
    SHOW_LOG(3, "================================\n");

    /*------------ INIT ------------*/
    pj_init();
    pj_caching_pool_init(&cp, NULL, 10000);
    pool = pj_pool_create(&cp.factory, "pool1", 1024, 1024, NULL);

    SHOW_LOG(2, "INIT CP AND POOL...DONE\n");

    /*------------ NODE ------------*/
#if 1

    init_adv_server(&adv_server, adv_cs, pool);

    for (i = 0;i < MAX_NODE; i++) {
        memset(gm_cs_tmp, 0, sizeof(gm_cs_tmp));
        memset(gmc_cs_tmp, 0, sizeof(gmc_cs_tmp));
        memset(adv_cs_tmp, 0, sizeof(adv_cs_tmp));

        ansi_copy_str(gm_cs_tmp, gm_cs);
        ansi_copy_str(adv_cs_tmp, adv_cs);
        ansi_copy_str(gmc_cs_tmp, gmc_cs);

        n = strlen(gmc_cs);

        gmc_cs_tmp[n-1] = i + 1+ '0';

        //printf("gmc_cs_tmp = %s\n", gmc_cs_tmp);
        memset(&riuc_data.node[i], 0, sizeof(riuc_data.node[i]));
        riuc_data.node[i].on_leaving_server_f = &on_leaving_server;
        node_init(&riuc_data.node[i], id, location, desc, i, gm_cs_tmp, gmc_cs_tmp, pool);
        node_add_adv_server(&riuc_data.node[i], &adv_server);
    }

    SHOW_LOG(2, "INIT NODE...DONE\n");
#endif
    /*----------- GB --------------*/
#if 1
    memset(&riuc_data.gb_sender, 0, sizeof(riuc_data.gb_sender));
    n = sprintf(gb_cs, "udp:%s:%d", GB_MIP, GB_PORT);
    gb_cs[n] = '\0';
    gb_sender_create(&riuc_data.gb_sender, gb_cs);

    SHOW_LOG(2, "INIT GB SENDER...DONE\n");
#endif
    /*----------- RIUC4 --------------*/
#if 1
    memset(riuc_data.serial_file, 0, sizeof(riuc_data.serial_file));
    strncpy(riuc_data.serial_file, argv[1], strlen(argv[1]));
    riuc4_init(&riuc_data.serial, &riuc_data.riuc4, &on_riuc4_status, pool);
    riuc4_start(&riuc_data.serial, riuc_data.serial_file);

    SHOW_LOG(2, "INIT RIUC4...DONE\n");
#if 1
    for (i = 0; i < MAX_NODE; i++) {
        riuc4_enable_rx(&riuc_data.riuc4, i);
        usleep(250*1000);
        riuc4_enable_tx(&riuc_data.riuc4, i);
        usleep(250*1000);
    }
#endif
    SHOW_LOG(2, "ENABLE TX & RX...DONE\n");
#endif
    /*----------- STREAM --------------*/
#if 1
    SHOW_LOG(3, "INIT STREAM...START\n");
    pjmedia_endpt_create(&cp.factory, NULL, 1, &ep);
#if 1
    SHOW_LOG(3, "CODEC INIT\n");
    pjmedia_codec_g711_init(ep);

    for (i = 0; i < MAX_NODE; i++) {
        SHOW_LOG(3, "NODE MEDIA CONFIG\n");
        node_media_config(&riuc_data.node[i], &streamers[i], &receivers[i]);
        SHOW_LOG(3, "SET POOL\n");
        riuc_data.node[i].streamer->pool = pool;
        riuc_data.node[i].receiver->pool = pool;
        
        SHOW_LOG(3, "SET ENDPOINT\n");
        riuc_data.node[i].receiver->ep = ep;
        riuc_data.node[i].streamer->ep = ep;

        SHOW_LOG(3, "INIT STREAMER & RECEIVER FOR NODE %d\n", i);
        streamer_init(riuc_data.node[i].streamer, riuc_data.node[i].streamer->ep, riuc_data.node[i].receiver->pool);
        receiver_init(riuc_data.node[i].receiver, riuc_data.node[i].receiver->ep, riuc_data.node[i].receiver->pool, 2);
    }
    
    SHOW_LOG(3, "CONFIG SOUND DEVICE\n");
    for (i = 0; i < MAX_NODE; i++) {
        streamer_config_dev_source(riuc_data.node[i].streamer, snd_dev[i]);
        receiver_config_dev_sink(riuc_data.node[i].receiver, snd_dev[i]);
    }

    SHOW_LOG(2, "INIT STREAM...DONE\n");
    /*---------------------------------*/
    pthread_create(&thread, NULL, auto_register, &riuc_data);
#endif
#endif
    while(!f_quit) {
        dummy = fgets(option, sizeof(option), stdin);

        switch(option[0]) {
            case 'c':
                for (i = 0; i < MAX_NODE; i++){
                    SHOW_LOG(3, "Set device index for each radio...\nRadio %d: ", i);
                    dummy = fgets(option, sizeof(option), stdin);           
                    input = atoi(&option[0]);
                    n = sprintf(sql_cmd, "UPDATE riuc_config SET snd_dev_r%d =(?)", i);
                    sql_cmd[n] = '\0';
                    CALL_SQLITE (prepare_v2 (db, sql_cmd, strlen (sql_cmd) + 1, & stmt, NULL));
                    CALL_SQLITE (bind_int (stmt, 1, input));
                    CALL_SQLITE_EXPECT (step (stmt), DONE);

                    streamer_config_dev_source(riuc_data.node[i].streamer, input);
                    receiver_config_dev_sink(riuc_data.node[i].receiver, input);
                }


                SHOW_LOG(3, "Config completed\n");               
                break;
            case 's':
                sql = "SELECT *FROM riuc_config";
                CALL_SQLITE (prepare_v2 (db, sql, strlen (sql) + 1, &stmt, NULL));

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    printf("ID: %s\n", sqlite3_column_text(stmt, 0));
                    printf("Location: %s\n", sqlite3_column_text(stmt, 1));
                    printf("Desc: %s\n", sqlite3_column_text(stmt, 2));
                    printf("GM_CS: %s\n", sqlite3_column_text(stmt, 3));
                    printf("GMC_CS: %s\n", sqlite3_column_text(stmt, 4));
                    printf("snd_dev_r0: %u\n", sqlite3_column_int(stmt, 5));
                    printf("snd_dev_r1: %u\n", sqlite3_column_int(stmt, 6));
                    printf("snd_dev_r2: %u\n", sqlite3_column_int(stmt, 7));
                    printf("snd_dev_r3: %u\n", sqlite3_column_int(stmt, 8));
                }
                break;
            case 't':
                node_start_session(&riuc_data.node[0]);
                break;
            case 'y':
                node_stop_session(&riuc_data.node[0]);
                break;
            case 'j':
                node_invite(&riuc_data.node[0], "FTW");
                break;
            case 'l':
                node_repulse(&riuc_data.node[0], "FTW");
                break;
            case 'q':
                f_quit = 1;
                break;
            default:
                SHOW_LOG(3, "Unknown option\n"); 
                break;
        }   

    }
    SHOW_LOG(3, "Quiting...\n");
    //pthread_join(thread, NULL);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}
