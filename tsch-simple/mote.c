#include "common.h"
#include "sys/node-id.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL (2 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
uint32_t num_rcv = 0;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  LOG_INFO("Received response| Ticks: '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
  num_rcv++;
}

static void send_packet() {
  // static char str[32];
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

  initialize_tsch_schedule(node_id);
  
  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer,  SEND_INTERVAL);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    send_packet();
    // add some jitter
    uint16_t jitterInMs = (float) rand() / RAND_MAX * 1000;
    etimer_set(&periodic_timer, SEND_INTERVAL + jitterInMs * CLOCK_SECOND / 1000);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
