#include "node.h"
#include <stdio.h>
#include "ansi-utils.h"
#include "gb-sender.h"
#include "proto-constants.h"
#include "riuc4_uart.h"

typedef struct riuc_data_s riuc_data_t;
struct riuc_data_s {
    node_t node[4];

    gb_sender_t gb_sender;
    riuc4_t riuc4;
    serial_t serial;

    char serial_file[30];
};

riuc_data_t riuc_data;

void on_riuc4_status(int port, riuc4_signal_t signal, uart4_status_t *ustatus) {
    SHOW_LOG(4, fprintf(stdout, "on_riuc4_status port:%d, signal:%s\n", port, RIUC4_SIGNAL_NAME[signal]));
    switch(signal) {
        case RIUC_SIGNAL_SQ:
            gb_sender_report_sq(&riuc_data.gb_sender, riuc_data.node[port].id, port, ustatus->sq);
            break;
        case RIUC_SIGNAL_TX:
            gb_sender_report_tx(&riuc_data.gb_sender, riuc_data.node[port].id, port, ustatus->tx);
            break;
        case RIUC_SIGNAL_RX:
            gb_sender_report_rx(&riuc_data.gb_sender, riuc_data.node[port].id, port, ustatus->rx);
            break;
        default:
            EXIT_IF_TRUE(1, "Unknow signal\n");
    } 
}

void on_adv_info(adv_server_t *adv_server, adv_request_t *request) {
    SHOW_LOG(4, fprintf(stdout,"Received: ID = %s\nSDP addr %s:%d\n", request->adv_info.info_id, request->adv_info.sdp_mip, request->adv_info.sdp_port));
    //Join to mutilcast add
    //Create stream
    //Start stream
    //call gb_sender_report_tx
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

    int i, len, n;
    
    SET_LOG_LEVEL(4);

    gm_cs = argv[5];
    gmc_cs = argv[6];
    n = sprintf(adv_cs, "udp:192.168.2.50:%d", adv_port);
    adv_cs[n] = '\0';
    n = sprintf(gb_cs, "udp:%s:%d",GB_MIP, gb_port);
    gb_cs[n] = '\0';

    SHOW_LOG(5, fprintf(stdout, "%s - %s - %s - %s - %s - %s - %s - %s - %s\n",argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], adv_cs, gb_cs, argv[8]));


    for (i = 0;i < 4; i++) {

        memset(gm_cs_tmp, 0, sizeof(gm_cs_tmp));
        memset(gmc_cs_tmp, 0, sizeof(gmc_cs_tmp));
        memset(adv_cs_tmp, 0, sizeof(adv_cs_tmp));

        ansi_copy_str(gm_cs_tmp, gm_cs);
        ansi_copy_str(adv_cs_tmp, adv_cs);

        n = sprintf(gmc_cs_tmp, "udp:192.168.2.50:399%d", i + 1);
        gmc_cs_tmp[n] = '\0';

       // printf("gmc_cs_tmp = %s\n", gmc_cs_tmp);
        memset(&riuc_data.node[i], 0, sizeof(riuc_data.node[i]));
        riuc_data.node[i].on_adv_info_f = &on_adv_info;

        node_init(&riuc_data.node[i], argv[1], argv[2], argv[3], i, gm_cs_tmp, gmc_cs_tmp, adv_cs_tmp);
    }

    memset(&riuc_data.gb_sender, 0, sizeof(riuc_data.gb_sender));
    n = sprintf(gb_cs, "udp:%s:%d", GB_MIP, GB_PORT);
    gb_cs[n] = '\0';
    gb_sender_create(&riuc_data.gb_sender, gb_cs);

    // Serial RIUC4 interface for radios ...
    memset(riuc_data.serial_file, 0, sizeof(riuc_data.serial_file));
    strncpy(riuc_data.serial_file, argv[8], strlen(argv[8]));
    riuc4_init(&riuc_data.serial, &riuc_data.riuc4, &on_riuc4_status);
    riuc4_start(&riuc_data.serial, riuc_data.serial_file);

    while (1) {
        for (i = 0; i < 4; i++)
            node_register(&riuc_data.node[i]);
        usleep(5*1000*1000);
    }
    return 0;
}
