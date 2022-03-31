/********** Libraries ***********/
#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "sys/node-id.h"
// #include "net/queuebuf.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/********** Global variables ***********/
#define UDP_PORT 8765

// period to send a packet to the udp server
#define SEND_INTERVAL (60 * CLOCK_SECOND)

// period to update the policy
#define UPDATE_POLICY_INTERVAL (1 * CLOCK_SECOND)

// UDP communication process
PROCESS(node_udp_process, "UDP communicatio process");
// Q-Learning and scheduling process
PROCESS(scheduler_process, "QL-TSCH Scheduler Process");

AUTOSTART_PROCESSES(&node_udp_process, &scheduler_process);

// data to send to the server
char custom_payload[UDP_PLAYLOAD_SIZE];

// Broadcast slotframe and Unicast slotframe
struct tsch_slotframe *sf_broadcast;
struct tsch_slotframe *sf_unicast;

// array to store the links of the unicast slotframe
struct tsch_link *links_unicast_sf[UNICAST_SLOTFRAME_LENGTH];

// a variable to store the current action number
uint8_t current_action = 0;
// variable to strore Tx link of the scheduler
// struct tsch_link *current_link;
// array to store Q-values of the actions (or timeslots)
float q_values[UNICAST_SLOTFRAME_LENGTH];

/********** Scheduler Setup ***********/
// Function starts Minimal Shceduler
static void init_tsch_schedule(void)
{
  // delete all the slotframes
  tsch_schedule_remove_all_slotframes();

  // create a broadcast slotframe and a unicast slotframe
  sf_broadcast = tsch_schedule_add_slotframe(0, BROADCAST_SLOTFRAME_LENGTH);
  sf_unicast = tsch_schedule_add_slotframe(0, UNICAST_SLOTFRAME_LENGTH);

  // shared/advertising cell at (0, 0) --> create a shared/advertising link in the broadcast slotframe
  tsch_schedule_add_link(sf_broadcast, LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED,
                         LINK_TYPE_ADVERTISING, &tsch_broadcast_address, 0, 0, 1);

  // create one Tx link in the fisrt slot of the unicast slotframe
  links_unicast_sf[0] = tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                               LINK_TYPE_NORMAL, &tsch_broadcast_address, 0, 0, 1);
  current_action = 0;
  // create multiple Rx links in the rest of the unicast slotframe
  for (int i = 1; i < UNICAST_SLOTFRAME_LENGTH; i++)
  {
    links_unicast_sf[i] = tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                 LINK_TYPE_NORMAL, &tsch_broadcast_address, i, 0, 1);
  }
}

// set up new schedule based on the chosen action
void set_up_new_schedule(uint8_t action)
{
  if (action != current_action)
  {
    links_unicast_sf[action] = tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                      LINK_TYPE_NORMAL, &tsch_broadcast_address, action, 0, 1);
    links_unicast_sf[current_action] = tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                              LINK_TYPE_NORMAL, &tsch_broadcast_address, current_action, 0, 1);
    current_action = action;
  }
}

// function to receive udp packets
static void rx_packet(struct simple_udp_connection *c,
                      const uip_ipaddr_t *sender_addr,
                      uint16_t sender_port,
                      const uip_ipaddr_t *receiver_addr,
                      uint16_t receiver_port,
                      const uint8_t *data,
                      uint16_t datalen)
{
  char received_data[UDP_PLAYLOAD_SIZE];
  memcpy(received_data, data, datalen);
  LOG_INFO("Received from ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("  data: %s\n", data);
}

// function to populate the payload
void create_payload()
{
  for (int i = 0; i < UDP_PLAYLOAD_SIZE; i++)
  {
    custom_payload[i] = i % 26 + 'a';
  }
}

// function to empty the queue and/or print the statistics
// uint8_t empty_schedule_records(uint8_t tx_rx)
// {
//   queue_packet_status *queue;
//   if (tx_rx == 0)
//   {
//     queue = func_custom_queue_tx();
//     LOG_INFO(" Transmission Operations in %d seconds\n", Q_TABLE_INTERVAL);
//   }
//   else
//   {
//     queue = func_custom_queue_rx();
//     LOG_INFO(" Receiving Operations in %d seconds\n", Q_TABLE_INTERVAL);
//   }
// #if PRINT_TRANSMISSION_RECORDS
//   for (int i = 0; i < queue->size; i++)
//   {
//     LOG_INFO("seqnum:%u trans_count:%u timeslot:%u channel_off:%u\n",
//              queue->packets[i].packet_seqno,
//              queue->packets[i].transmission_count,
//              queue->packets[i].time_slot,
//              queue->packets[i].channel_offset);
//   }
// #endif
//   return emptyQueue(queue);
// }

/********** UDP Communication Process - Start ***********/
PROCESS_THREAD(node_udp_process, ev, data)
{
  static struct simple_udp_connection udp_conn;
  static struct etimer periodic_timer;

  static uint32_t seqnum;
  uip_ipaddr_t dst;

  PROCESS_BEGIN();

  // creating the payload and starting the scheduler
  create_payload();
  init_tsch_schedule();
  LOG_INFO("Payload %s\n", custom_payload);
  // NETSTACK_RADIO.off()
  // NETSTACK_MAC.off()

  /* Initialization; `rx_packet` is the function for packet reception */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, rx_packet);

  if (node_id == 1)
  { /* node_id is 1, then start as root*/
    NETSTACK_ROUTING.root_start();
  }
  // Initialize the mac layer
  NETSTACK_MAC.on();

  LOG_INFO("Finished setting up.......\n");

  // if this is a simple node, start sending upd packets
  if (node_id != 1)
  {
    LOG_INFO("Started UDP communication\n");
    // start the timer for periodic udp packet sending
    etimer_set(&periodic_timer, SEND_INTERVAL);
    /* Main UDP comm Loop */
    while (1)
    {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
      if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
      {
        /* Send custom payload to the network root node and increase the packet count number*/
        seqnum++;
        LOG_INFO("Send to ");
        LOG_INFO_6ADDR(&dst);
        LOG_INFO_(", application packet number %" PRIu32 "\n", seqnum);
        simple_udp_sendto(&udp_conn, &custom_payload, sizeof(custom_payload), &dst);
      }
      etimer_set(&periodic_timer, SEND_INTERVAL);
    }
  }
  PROCESS_END();
}
/********** UDP Communication Process - End ***********/

/********** QL-TSCH Scheduler Process - Start ***********/
PROCESS_THREAD(scheduler_process, ev, data)
{
  // timer to update the policy
  static struct etimer policy_update_timer;

  PROCESS_BEGIN();

  // queue length pointer
  uint8_t *queue_length;
  uint8_t buffer_len_before = 0;
  uint8_t buffer_len_after = 0;

  // lock time-slotting before starting the first schedule
  // while(1) {
  //     if (tsch_get_lock()) break;
  // }

  /* Main Scheduler Loop */
  while (1)
  {
    // getting the action with the highest q-value and setting upda the schedule
    uint8_t action = get_highest_q_val();
    set_up_new_schedule(action);

    // record the buffer size and release the tsch lock
    buffer_len_before = getCustomBuffLen();
    // if (tsch_is_locked()) tsch_release_lock();
    udp_com_stop = 0;

    // set the timer to update Q-table
    etimer_set(&policy_update_timer, Q_TABLE_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));

    buffer_len_after = getCustomBuffLen();
    queue_length = getCurrentQueueLen();
    // didn't work, solve this
    int buffer_size_global_count = tsch_queue_global_packet_count();
    LOG_INFO("Current Packet Buffer Size: %u\n", *queue_length);
    LOG_INFO("Second way Buffer Size: %d\n", buffer_size_global_count);
    LOG_INFO("Buffer Size 3rd Way: before-%u after-%u\n", buffer_len_before, buffer_len_after);
    LOG_INFO("Chosen Action: %u\n", action);

    // stopping the slot operations
    // while(1) {
    //   if (tsch_get_lock()) break;
    // }
    udp_com_stop = 1;
    // calculating the number of trans/receptions
    uint8_t n_tx_count = empty_schedule_records(0);
    uint8_t n_rx_count = empty_schedule_records(1);

    // calculate the reward and update the q-table
    float new_reward = reward(n_tx_count, n_rx_count, buffer_len_before, buffer_len_after);
    update_q_table(action, new_reward);
  }
  PROCESS_END();
}
/********** RL-TSCH Scheduler Process - End ***********/
