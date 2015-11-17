#include "node.h"
#include <stdio.h>
#include "ansi-utils.h"
#include "gb-sender.h"
#include "proto-constants.h"
#include "riuc4_uart.h"

typedef struct riuc_snd_device_s riuc_snd_device_t;
typedef struct riuc_data_s riuc_data_t;

struct riuc_snd_device_s {
    char id[30];
    int snd_indx;
};

struct riuc_data_s {
    node_t node;
    gb_sender_t gb_sender;
    riuc4_t riuc4;
    serial_t serial;
    riuc_snd_device_t snd_device[4];

    char serial_file[30];
};

riuc_data_t riuc_data;

#if 0
void set_snd_device(riuc_data_t *riuc_data, int idx) {
    check_snd_device(idx);
    for (i = 0; i < 4; i++ ) {
        if (is_used)
    }
}
#endif

void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus) {
    SHOW_LOG(4, "on_riuc4_status port:%d, signal:%s\n", port, RIUC4_SIGNAL_NAME[signal]);
    switch(signal) {
        case RIUC_SIGNAL_SQ:
            gb_sender_report_sq(&riuc_data.gb_sender, riuc_data.node.id, port, 1);
            break;
        case RIUC_SIGNAL_TX:
            break;
        case RIUC_SIGNAL_RX:
            break;
        default:
            EXIT_IF_TRUE(1, "Unknow signal\n");
    } 
}
/*
void on_adv_info(adv_server_t *adv_server, adv_request_t *request) {
    SHOW_LOG(4, "Received from %s\nSDP addr %s:%d\n", request->adv_info.adv_owner, request->adv_info.sdp_mip, request->adv_info.sdp_port);
    //Join to mutilcast add
    //Create stream
    //Start stream
    //call gb_sender_report_tx
}
*/

static void init_adv_server(adv_server_t *adv_server, char *adv_cs, node_t *node) {
    memset(adv_server, 0, sizeof(*adv_server));

    adv_server->on_request_f = &on_adv_info;
    adv_server->on_open_socket_f = &on_open_socket_adv_server;
    adv_server->user_data = node;
    
    adv_server_init(adv_server, adv_cs);
    adv_server_start(adv_server);
}

void *auto_register(void *node_data) {
    node_t *node = (node_t *)node_data;   
    while (1) {
        node_register(node);
        node_invite(&riuc_data.node, "OIUC");
        usleep(5*1000*1000);
    }
}

void usage(char *app) {
    printf("usage: %s <id> <location> <desc> <radio_port> <gm_cs> <gmc_cs> <guest> <serial_file>\n", app);
    exit(-1);
}

int main(int argc, char *argv[]) {
    if (argc < 8)
        usage(argv[0]);

    char *gm_cs;
    char *gmc_cs;
    char adv_cs[30];
    char gb_cs[30];

    int adv_port = ADV_PORT;
    int gb_port = GB_PORT; 
    int n;

    pj_caching_pool cp;
    endpoint_t streamer;
    endpoint_t receiver;
    adv_server_t adv_server;

    pthread_t thread;
    char *dummy, option[10];

    SET_LOG_LEVEL(4);

    gm_cs = argv[5];
    gmc_cs = argv[6];
    n = sprintf(adv_cs, "udp:0.0.0.0:%d", adv_port);
    adv_cs[n] = '\0';
    n = sprintf(gb_cs, "udp:%s:%d",GB_MIP, gb_port);
    gb_cs[n] = '\0';

    SHOW_LOG(5, "%s - %s - %s - %s - %s - %s - %s - %s - %s\n",argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], adv_cs, gb_cs, argv[8]);

    memset(&adv_server, 0, sizeof(adv_server));

    memset(&riuc_data.node, 0, sizeof(riuc_data.node));
    init_adv_server(&adv_server, adv_cs, &riuc_data.node);
    node_init(&riuc_data.node, argv[1], argv[2], argv[3], atoi(argv[4]), gm_cs, gmc_cs, adv_cs);
    node_add_adv_server(&riuc_data.node, &adv_server);

    memset(&riuc_data.gb_sender, 0, sizeof(riuc_data.gb_sender));
    n = sprintf(gb_cs, "udp:%s:%d", GB_MIP, GB_PORT);
    gb_cs[n] = '\0';
    gb_sender_create(&riuc_data.gb_sender, gb_cs);

    // Serial RIUC4 interface for radios ...
    memset(riuc_data.serial_file, 0, sizeof(riuc_data.serial_file));
    strncpy(riuc_data.serial_file, argv[8], strlen(argv[8]));
    riuc4_init(&riuc_data.serial, &riuc_data.riuc4, &on_riuc4_status);
    riuc4_start(&riuc_data.serial, riuc_data.serial_file);

    //Streame
    pj_init();

    pj_caching_pool_init(&cp, NULL, 1024);
    node_media_config(&riuc_data.node, &streamer, &receiver);
    riuc_data.node.streamer->pool = pj_pool_create(&cp.factory, "pool1", 1024, 1024, NULL);
    pjmedia_endpt_create(&cp.factory, NULL, 1, &riuc_data.node.streamer->ep);
    pjmedia_codec_g711_init(riuc_data.node.streamer->ep);

    streamer_init(riuc_data.node.streamer, riuc_data.node.streamer->ep, riuc_data.node.streamer->pool);
    streamer_config_dev_source(riuc_data.node.streamer, 2);

    pthread_create(&thread, NULL, auto_register, &riuc_data.node);   
    
    while(1) {
        dummy = fgets(option, sizeof(option), stdin);

        switch(option[0]) {
            case 't':
                node_start_session(&riuc_data.node);
                break;
            case 'y':
                node_stop_session(&riuc_data.node);
                break;
            case 'j':
                node_invite(&riuc_data.node, "OIUC");
                break;
            case 'l':
                node_repulse(&riuc_data.node, "OIUC");
                break;
            default:
                RETURN_IF_TRUE(1, "Unknown option\n");               
        }   

    }
    pthread_join(thread, NULL);
    return 0;
}
