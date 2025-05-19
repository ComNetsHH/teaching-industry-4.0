#include "common.h"
#include "sys/node-id.h"
#include "math.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define MAX_NODES 10  
#define MAX_DELAYS_PER_NODE 300

typedef struct {
    uint16_t delays[MAX_DELAYS_PER_NODE];
    size_t size; 
} delay_array_t;

typedef struct {
    delay_array_t nodes[MAX_NODES];  // Array of delay arrays indexed by node_id
} delay_map_t;

uint16_t delays[1000];
int pkt_index = 0;
static struct simple_udp_connection udp_conn;
static delay_map_t delay_map;

PROCESS(sink_process, "Sink");
AUTOSTART_PROCESSES(&sink_process);

static void initialize_delay_map(delay_map_t *map) {
    for (int i = 0; i < MAX_NODES; i++) {
        map->nodes[i].size = 0;  // Initialize the size to 0 for each node
    }
}

static void store_delay(delay_map_t *map, uint16_t nodeId, uint16_t delay) {
    if (nodeId >= MAX_NODES) {
        printf("Error: node_id %u is out of bounds\n", nodeId);
        return;
    }

    delay_array_t *delay_array = &map->nodes[nodeId];

    // Check if there's space to add another delay
    if (delay_array->size < MAX_DELAYS_PER_NODE) {
        delay_array->delays[delay_array->size++] = delay;  // Append the delay
        LOG_INFO("Storing delay for node %u, currently %zu\n", nodeId, delay_array->size);

    } else {
        printf("Error: No space left to store more delays for node_id %u\n", nodeId);
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

void print_delays_for_node(delay_map_t *map, uint16_t node_id) {
    if (node_id >= MAX_NODES) {
        printf("Error: node_id %u is out of bounds\n", node_id);
        return;
    }

    delay_array_t *delay_array = &map->nodes[node_id];

    if (delay_array->size == 0) {
        printf("No delays stored for node_id %u\n", node_id);
        return;
    }

    printf("Delays for node_id %u: ", node_id);
    for (size_t i = 0; i < delay_array->size; i++) {
        printf("%u ms ", delay_array->delays[i]);
    }
    printf("\n");
}

void print_avg_delays(delay_map_t *map) {
    for (int i = 0; i < MAX_NODES; i++)
    {
        delay_array_t *delay_array = &map->nodes[i];
        if (delay_array->size == 0)
            continue;
        
        uint16_t *d = delay_array->delays;
        printf("Avg. delay for node #%u: %d ms (%zu packets in total)\n", i, (int) compute_average(d, delay_array->size), delay_array->size);
    }
}

/*---------------------------------------------------------------------------*/
static void udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    // Handle error: data length is too short
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

    uint16_t delayInMilliseconds = (uint16_t) ((tsch_get_network_uptime_ticks() - timestamp) * 1000 / CLOCK_SECOND);
    store_delay(&delay_map, nodeId, delayInMilliseconds);

    // Print or process the node_id and timestamp as needed
    printf("Received message from node %u with delay: %u ms\n", nodeId, delayInMilliseconds);

    if (enough_pkts_received(&delay_map, 100))
        print_avg_delays(&delay_map);
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(sink_process, ev, data)
{
    PROCESS_BEGIN();
    
    initialize_delay_map(&delay_map);
    initialize_tsch_schedule(node_id);
    NETSTACK_ROUTING.root_start();
    simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL, UDP_CLIENT_PORT, udp_rx_callback);

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
