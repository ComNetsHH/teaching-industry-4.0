#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/sixtop/sixtop.h"
#include "common.h"

#include "sf-simple.h"

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define MAX_NODES 10
#define MAX_DELAYS_PER_NODE 300

// TODO: refactor to a common include header
static struct simple_udp_connection udp_conn;

typedef struct {
    uint16_t delays[MAX_DELAYS_PER_NODE];  // Fixed-size array for delays
    size_t size;  // Current number of delays stored
} delay_array_t;

typedef struct {
    delay_array_t nodes[MAX_NODES];  // Array of delay arrays indexed by node_id
} delay_map_t;

static struct simple_udp_connection udp_conn;
static delay_map_t delay_map;

PROCESS(node_process, "RPL Node");
PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process, &node_process);

/*---------------------------------------------------------------------------*/

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

void print_avg_delays(delay_map_t *map) {
    for (int i = 0; i < MAX_NODES; i++)
    { 
        delay_array_t *delay_array = &map->nodes[i];
        if (delay_array->size == 0)
            continue;
        
        uint16_t *d = delay_array->delays;
        printf("Avg. delay for node #%u: %d ms\n", i, (int) compute_average(d, delay_array->size));
    }
}

static void initialize_delay_map(delay_map_t *map) {
    for (int i = 0; i < MAX_NODES; i++) {
        map->nodes[i].size = 0;  // Initialize the size to 0 for each node
    }
}

static void store_delay(delay_map_t *map, uint16_t node_id, uint16_t delay) {
    if (node_id >= MAX_NODES) {
        printf("Error: node_id %u is out of bounds\n", node_id);
        return;
    }

    delay_array_t *delay_array = &map->nodes[node_id];

    // Check if there's space to add another delay
    if (delay_array->size < MAX_DELAYS_PER_NODE) {
        delay_array->delays[delay_array->size++] = delay;  // Append the delay
    } else {
        printf("Error: No space left to store more delays for node_id %u\n", node_id);
    }
}

static bool enough_pkts_received(delay_map_t *map, uint16_t numRequired) {
    for (int i = 2; i < MAX_NODES; i++) {
        delay_array_t *delay_array = &map->nodes[i];
        LOG_INFO("Pkts received for node %d: %zu\n", i, delay_array->size);
        
        if (delay_array->size < numRequired) {
            return false;
        }

        if (i + 1 < MAX_NODES && map->nodes[i + 1].size == 0) {
            break;
        }
    }

    return true;
}

static void udp_rx_callback(struct simple_udp_connection* c,
    const uip_ipaddr_t* sender_addr,
    uint16_t sender_port,
    const uip_ipaddr_t* receiver_addr,
    uint16_t receiver_port,
    const uint8_t* data,
    uint16_t datalen)
{   
    if (datalen < sizeof(uint16_t) + sizeof(uint64_t)) {      
        printf("Received data is too short to contain node_id and timestamp\n");
        return;
    }

    // Extract the node_id
    uint16_t nodeId;
    memcpy(&nodeId, data, sizeof(uint16_t));

    // and the timestamp
    uint64_t timestamp;
    memcpy(&timestamp, data + sizeof(uint16_t), sizeof(uint64_t));

    uint16_t delayInMilliseconds = (uint16_t)((tsch_get_network_uptime_ticks() - timestamp) * 1000 / CLOCK_SECOND);
    store_delay(&delay_map, nodeId, delayInMilliseconds);

    printf("Received message from node %u with delay: %u ms\n", nodeId, delayInMilliseconds);

    if (enough_pkts_received(&delay_map, 100))
        print_avg_delays(&delay_map);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
    PROCESS_BEGIN();

    simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL, UDP_CLIENT_PORT, udp_rx_callback);

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(node_process, ev, data)
{
    PROCESS_BEGIN();
    
    NETSTACK_ROUTING.root_start();
    NETSTACK_MAC.on();
    initialize_delay_map(&delay_map);
    sixtop_add_sf(&sf_simple_driver);

    if (IS_STAR_TOPOLOGY)
        initialize_tsch_schedule_star(node_id);
    else 
        initialize_tsch_schedule(node_id);       

    PROCESS_END();
}