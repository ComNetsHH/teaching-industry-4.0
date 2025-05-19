#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "mac/tsch/tsch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <random.h>
#include "sys/log.h"
#include <clock.h>

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define IS_STAR_TOPOLOGY 1

static linkaddr_t construct_linkaddr(uint16_t nodeId) {
    linkaddr_t addr;

    for (int i = 0; i < LINKADDR_SIZE; i++) {
        if (i % 2 == 0)
            addr.u8[i] = 0x00;
        else
            addr.u8[i] = (unsigned char)nodeId;
    }
    return addr;
}

static void initialize_tsch_schedule(uint16_t nodeId) {
    struct tsch_slotframe *sf = tsch_schedule_add_slotframe(0, TSCH_SCHEDULE_DEFAULT_LENGTH);

    // minimal cell for time synchronization
    tsch_schedule_add_link(sf,
        (LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING),
        LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
        0, 0, 1);

    // auto cells: self RX and TX to parent and child node (if present)
    linkaddr_t self_addr = construct_linkaddr(nodeId);
    linkaddr_t parent = construct_linkaddr(nodeId-1);
    linkaddr_t child = construct_linkaddr(nodeId+1);

    tsch_schedule_add_link(sf, (LINK_OPTION_RX | LINK_OPTION_SHARED), LINK_TYPE_NORMAL, &self_addr, nodeId, 1, 0);
    tsch_schedule_add_link(sf, (LINK_OPTION_TX | LINK_OPTION_SHARED), LINK_TYPE_NORMAL, &parent, nodeId-1, 1, 0);
    tsch_schedule_add_link(sf, (LINK_OPTION_TX | LINK_OPTION_SHARED), LINK_TYPE_NORMAL, &child, nodeId+1, 1, 0); 
}


static void initialize_tsch_schedule_star(uint16_t nodeId) {
    struct tsch_slotframe *sf = tsch_schedule_add_slotframe(0, TSCH_SCHEDULE_DEFAULT_LENGTH);

    // minimal cell for time synchronization
    tsch_schedule_add_link(sf,
        (LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING),
        LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
        0, 0, 1);

    // auto cells: self RX and TX to parent and child node (if present)
    linkaddr_t self_addr = construct_linkaddr(nodeId);
    linkaddr_t parent = construct_linkaddr(1);

    tsch_schedule_add_link(sf, (LINK_OPTION_RX | LINK_OPTION_SHARED), LINK_TYPE_NORMAL, &self_addr, nodeId, 1, 0);
    tsch_schedule_add_link(sf, (LINK_OPTION_TX | LINK_OPTION_SHARED), LINK_TYPE_NORMAL, &parent, 1, 1, 0);

    // the sink
    if (nodeId == 1) {
        for (int i = 2; i < 10; i++) {
            linkaddr_t child = construct_linkaddr(i);
            tsch_schedule_add_link(sf, (LINK_OPTION_TX | LINK_OPTION_SHARED), LINK_TYPE_NORMAL, &child, (uint16_t) i, 1, 0);
        }
    }
}
