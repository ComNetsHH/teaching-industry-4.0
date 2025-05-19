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

#define SLOTFRAME_SIZE	101
#define NUM_CELLS_PER_LINK 3
#define PACKETS_TO_SEND 100

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
    struct tsch_slotframe *sf = tsch_schedule_add_slotframe(1, SLOTFRAME_SIZE);
    struct tsch_slotframe *sf_min = tsch_schedule_add_slotframe(0, TSCH_SCHEDULE_DEFAULT_LENGTH);

    // minimal cell for time synchronization
    tsch_schedule_add_link(sf_min,
        (LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING),
        LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
        0, 0, 1);

    // dedicated cells for application traffic
    for (int i = 0; i < NUM_CELLS_PER_LINK; i++) {
        uint16_t slOffset = (uint16_t) round(i * (SLOTFRAME_SIZE / (NUM_CELLS_PER_LINK+1)));
        linkaddr_t dest = construct_linkaddr(nodeId-1);
        linkaddr_t self_addr = construct_linkaddr(nodeId);

        // add TX and RX links to forward the packets
        tsch_schedule_add_link(sf, LINK_OPTION_RX, LINK_TYPE_NORMAL, &self_addr, (slOffset + nodeId-1) % SLOTFRAME_SIZE, 1, 0);
        tsch_schedule_add_link(sf, LINK_OPTION_TX, LINK_TYPE_NORMAL, &dest, (slOffset + nodeId-2) % SLOTFRAME_SIZE, 1, 0);
    }
}

double compute_average(uint16_t *array, size_t length) {
    if (length == 0) {
        return 0; // Return 0 if array length is 0 to avoid division by 0
    }

    uint32_t sum = 0;

    // Sum up all the elements
    for (size_t i = 0; i < length; i++) {
        sum += array[i];
    }

    // Calculate the average
    double average = (double) sum / length;

    // Round the average to the nearest integer and return as uint16_t
    return average;
}