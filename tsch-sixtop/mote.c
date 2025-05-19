#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/sixtop/sixtop.h"
#include "net/routing/routing.h"
#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>
#include "sf-simple.h"
#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"
#include "sys/log.h"
#include "sys/rtimer.h"
#include "common.h"

#define LOG_MODULE                     "App"
#define LOG_LEVEL                      LOG_LEVEL_INFO
#define SEND_INTERVAL                  (3 * CLOCK_SECOND)
#define NUM_CELLS_TO_ADD               2

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
PROCESS(node_process, "RPL Node");
AUTOSTART_PROCESSES(&udp_client_process, &node_process);
/*---------------------------------------------------------------------------*/
static bool cells_added = false;
static struct simple_udp_connection udp_conn;

static void send_packet() {
  uip_ipaddr_t dest_ipaddr;
  static uint32_t tx_count;

  if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
    /* Send to DAG root */
    // int queue = tsch_queue_global_packet_count();
    // LOG_INFO("Queue: %d packets\n", queue);
    LOG_INFO("Sending request #%"PRIu32" to ", tx_count);
    LOG_INFO_6ADDR(&dest_ipaddr);
    LOG_INFO_("\n");
    // snprintf(str, sizeof(str), "%d:%" PRIu64, node_id, tsch_get_network_uptime_ticks());
    // printf("\n");
    
    uint8_t payload[sizeof(node_id) + sizeof(uint64_t)];
    uint64_t timestamp = tsch_get_network_uptime_ticks();

    // Copy the node_id into the first part of the payload
    memcpy(payload, &node_id, sizeof(node_id));

    // Copy the timestamp into the subsequent part of the payload
    memcpy(payload + sizeof(node_id), &timestamp, sizeof(timestamp));

    // Send the payload using your UDP send function
    simple_udp_sendto(&udp_conn, payload, sizeof(payload), &dest_ipaddr);
    tx_count++;
  } else
    LOG_INFO("Not reachable yet\n");
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
    static struct etimer periodic_timer;

    PROCESS_BEGIN();
    simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);

    etimer_set(&periodic_timer, SEND_INTERVAL);
    while (1)
    {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

        if (!cells_added)
            LOG_INFO("Cells are not added yet\n");
        else
            send_packet();

        // add some jitter
        uint16_t jitterInMs = (float) rand() / RAND_MAX * 1000;
        etimer_set(&periodic_timer, SEND_INTERVAL + jitterInMs * CLOCK_SECOND / 1000);
    }

    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
    static struct etimer et;
    struct tsch_neighbor *n;

    PROCESS_BEGIN();

    NETSTACK_MAC.on();
    sixtop_add_sf(&sf_simple_driver);
    initialize_tsch_schedule(node_id);

    // wait until a node joins the DODAG
    while (!NETSTACK_ROUTING.node_is_reachable()) {
        etimer_set(&et, 5 * CLOCK_SECOND);
        PROCESS_YIELD_UNTIL(etimer_expired(&et));
        LOG_INFO("Node process: not reachable\n");
    }

    // bit of warmup to associate with the network
    etimer_set(&et, 30 * CLOCK_SECOND);
    PROCESS_YIELD_UNTIL(etimer_expired(&et));

    LOG_INFO("Adding %d cells\n", NUM_CELLS_TO_ADD);
    n = tsch_queue_get_time_source();
    sf_simple_add_links(tsch_queue_get_nbr_address(n), NUM_CELLS_TO_ADD);

    // To ensure cells are added
    etimer_set(&et, 5 * CLOCK_SECOND);
    PROCESS_YIELD_UNTIL(etimer_expired(&et));
    
    LOG_INFO("Cells should be added by now, starting packet transmissions\n");
    cells_added = true;
    
    PROCESS_END();
}