#include "node.h"
#include <stdio.h>
#include "ansi-utils.h"
#include "my-pjlib-utils.h"
#include "gb-sender.h"
#include "proto-constants.h"
#include "riuc4_uart.h"
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia-audiodev/audiotest.h>

typedef struct riuc_data_s riuc_data_t;

#define MAX_NODE 1

struct riuc_data_s {
    node_t node[MAX_NODE];
    gb_sender_t gb_sender;
    riuc4_t riuc4;
    serial_t serial;

    char serial_file[30];
};

riuc_data_t riuc_data;

static void list_devices(void)
{
    unsigned dev_count;
    unsigned i;
    pj_status_t status;
    
    dev_count = pjmedia_aud_dev_count();
    if (dev_count == 0) {
	SHOW_LOG(3, "No devices found");
    return;
    }

    SHOW_LOG(3, "Found %d devices:\n", dev_count);

    for (i=0; i<dev_count; ++i) {
        pjmedia_aud_dev_info info;

        status = pjmedia_aud_dev_get_info(i, &info);
        if (status != PJ_SUCCESS)
            continue;

        SHOW_LOG(3, " %2d: %s [%s] (%d/%d)\n", i, info.driver, info.name, info.input_count, info.output_count);
    }
}

int check_snd_device(int idx) {
    pjmedia_aud_param param;
    pjmedia_aud_test_results result;
    pj_status_t status;

    CHECK(__FILE__, pjmedia_aud_dev_default_param(idx, &param));

    unsigned ptime = 500;

    param.dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param.rec_id = idx;
    param.play_id = idx;
    param.clock_rate = 8000;
    param.channel_count = 2;

    param.samples_per_frame = param.clock_rate * param.channel_count * ptime / 1000;

    /* Latency settings */
    param.flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param.input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param.output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    SHOW_LOG(3,"Performing test for device %d...\n", idx);

    status = pjmedia_aud_test(&param, &result);
    if (status != PJ_SUCCESS) {
        SHOW_LOG(3, "Device %d: Test has completed with error\n", idx, status);
        return 0;
    }
    else {
        SHOW_LOG(3, "Device %d...OK\n", idx);
        return 1;
    }
}

int is_device_used(riuc_data_t *riuc_data, int device_idx) {
    int i;

    for (i = 0; i < MAX_NODE; i++) {
        if (device_idx == riuc_data->node[i].streamer_dev_idx || device_idx == riuc_data->node[i].receiver_dev_idx) {
            return 1;
        }
    }
    SHOW_LOG(3, "Noone use device=%d\n", device_idx);
    return 0;
}

void set_snd_device(riuc_data_t *riuc_data) {
    int idx = 0, node_idx, device_idx;
    unsigned MAX_DEVICE;

    MAX_DEVICE = pjmedia_aud_dev_count();

    list_devices();

    SHOW_LOG(3, "MAX_NODE = %d, MAX_DEVICE = %d\n", MAX_NODE, MAX_DEVICE);

    for (node_idx = 0; node_idx < MAX_NODE; node_idx ++) {
        for (device_idx = idx; device_idx < MAX_DEVICE; device_idx++) {
            if (check_snd_device(device_idx) && !is_device_used(riuc_data, device_idx)) {
                SHOW_LOG(3, "********* Found Device %d available**********\n", device_idx);
                riuc_data->node[idx].streamer_dev_idx = device_idx;
                riuc_data->node[idx].receiver_dev_idx = device_idx;
                break;
            }
        }
        idx = device_idx;
    }
}
void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus) {
    SHOW_LOG(4, "on_riuc4_status port:%d, signal:%s\n", port, RIUC4_SIGNAL_NAME[signal]);

    static pj_thread_desc s_desc;
    static pj_thread_t *s_thread;
    ANSI_CHECK(__FILE__, pj_thread_register("adv_server", s_desc, &s_thread));

    switch(signal) {
        case RIUC_SIGNAL_SQ:
            gb_sender_report_sq(&riuc_data.gb_sender, riuc_data.node[0].id, port, ustatus->sq);
            if (ustatus->sq == 1) {
                node_start_session(&riuc_data.node[0]);
                riuc4_enable_rx(&riuc_data.riuc4, port);
            }
            else {
                node_stop_session(&riuc_data.node[0]);
                riuc4_disable_rx(&riuc_data.riuc4, port);
            }
            break;
        case RIUC_SIGNAL_TX:
            gb_sender_report_tx(&riuc_data.gb_sender, riuc_data.node[0].id, port, ustatus->tx);
            break;
        case RIUC_SIGNAL_RX:
            gb_sender_report_rx(&riuc_data.gb_sender, riuc_data.node[0].id, port, ustatus->rx);
            break;
        default:
            EXIT_IF_TRUE(1, "Unknow signal\n");
    } 
}

void on_adv_info_riuc(adv_server_t *adv_server, adv_request_t *request, char *caddr_str) {
    node_t *node = adv_server->user_data;
    SHOW_LOG(3, "New session: %s(%s:%d)\n", request->adv_info.adv_owner, request->adv_info.sdp_mip, request->adv_info.sdp_port);
    if(!node_has_media(node)) {
        SHOW_LOG(1, "Node does not have media endpoints configured\n");
        return;
    }
    if( request->adv_info.sdp_port > 0 ) {
        receiver_stop(node->receiver);
        receiver_config_stream(node->receiver, request->adv_info.sdp_mip, request->adv_info.sdp_port, 0);
        receiver_start(node->receiver);
        riuc4_enable_tx(&riuc_data.riuc4, node->radio_port);
    }
    else {
        receiver_stop(node->receiver);
        riuc4_disable_tx(&riuc_data.riuc4, node->radio_port);
    }
}

static void init_adv_server(adv_server_t *adv_server, char *adv_cs, node_t *node) {
    memset(adv_server, 0, sizeof(*adv_server));

    adv_server->on_request_f = &on_adv_info_riuc;
    adv_server->on_open_socket_f = &on_open_socket_adv_server;
    adv_server->user_data = node;

    adv_server_init(adv_server, adv_cs);
    adv_server_start(adv_server);
}

void *auto_register(void *riuc_data) {
    int i;
    riuc_data_t *riuc = (riuc_data_t *)riuc_data;   
    while (1) {
        for (i = 0; i < MAX_NODE; i++) {
            node_register(riuc->node);
            //node_invite(node, "OIUC");
        }
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
    char gm_cs_tmp[30], gmc_cs_tmp[30], adv_cs_tmp[30];

    int adv_port = ADV_PORT;
    int gb_port = GB_PORT; 
    int i, n;

    pj_caching_pool cp;
    pj_pool_t *pool;
    pjmedia_endpt *ep;

    endpoint_t streamers[MAX_NODE];
    endpoint_t receivers[MAX_NODE];
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

    /*------------ INIT ------------*/
    pj_init();
    pj_caching_pool_init(&cp, NULL, 10000);
    pool = pj_pool_create(&cp.factory, "pool1", 1024, 1024, NULL);

    SHOW_LOG(2, "INIT CP AND POOL...DONE\n");

    /*------------ NODE ------------*/
#if 1
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

        init_adv_server(&adv_server, adv_cs_tmp, &riuc_data.node[i]);
        node_init(&riuc_data.node[i], argv[1], argv[2], argv[3], i, gm_cs_tmp, gmc_cs_tmp, adv_cs_tmp);
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
    strncpy(riuc_data.serial_file, argv[8], strlen(argv[8]));
    riuc4_init(&riuc_data.serial, &riuc_data.riuc4, &on_riuc4_status);
    riuc4_start(&riuc_data.serial, riuc_data.serial_file);

    SHOW_LOG(2, "INIT RIUC4...DONE\n");
#endif
    /*----------- STREAM --------------*/
#if 1
    pjmedia_endpt_create(&cp.factory, NULL, 1, &ep);
#if 1
    pjmedia_codec_g711_init(ep);

    for (i = 0; i < MAX_NODE; i++) {
        node_media_config(&riuc_data.node[i], &streamers[i], &receivers[i]);
        riuc_data.node[i].streamer->pool = pool;
        riuc_data.node[i].receiver->pool = pool;

        riuc_data.node[i].receiver->ep = ep;
        riuc_data.node[0].streamer->ep = ep;

        streamer_init(riuc_data.node[i].streamer, riuc_data.node[i].streamer->ep, riuc_data.node[i].receiver->pool);
        receiver_init(riuc_data.node[i].receiver, riuc_data.node[i].receiver->ep, riuc_data.node[i].receiver->pool, 2);
    }

#if 0
    set_snd_device(&riuc_data);
    streamer_config_dev_source(riuc_data.node[0].streamer, riuc_data.node[0].streamer_dev_idx);
    receiver_config_dev_sink(riuc_data.node[0].receiver, riuc_data.node[0].receiver_dev_idx);
#endif

#if 1
    streamer_config_dev_source(riuc_data.node[0].streamer, 2);
    receiver_config_dev_sink(riuc_data.node[0].receiver, 2);
#endif
    SHOW_LOG(2, "INIT STREAM...DONE\n");
    /*---------------------------------*/
    pthread_create(&thread, NULL, auto_register, &riuc_data);
#endif
#endif
    while(1) {
        dummy = fgets(option, sizeof(option), stdin);

        switch(option[0]) {
            case 't':
                node_start_session(&riuc_data.node[0]);
                break;
            case 'y':
                node_stop_session(&riuc_data.node[0]);
                break;
            case 'j':
                node_invite(&riuc_data.node[0], "OIUC");
                break;
            case 'l':
                node_repulse(&riuc_data.node[0], "OIUC");
                break;
            default:
                RETURN_IF_TRUE(1, "Unknown option\n");               
        }   

    }
    pthread_join(thread, NULL);
    return 0;
}
