/********** Libraries ***********/
#include "contiki.h"
#include "net/ipv6/simple-udp.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "sys/node-id.h"

#include "net/mac/tsch/tsch-slot-operation.h"
#include "net/mac/tsch/tsch-queue.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/********** Global variables ***********/
#define UDP_PORT 8765

// period to send a packet to the udp server
#define SEND_INTERVAL (PACKET_SENDING_INTERVAL * CLOCK_SECOND)

// period to update the policy
#if UPDATE_POLICY_INTERVAL_CONF == 0
#define UPDATE_POLICY_INTERVAL (CLOCK_SECOND * (EXTRA_TIME + UNICAST_SLOTFRAME_LENGTH + BROADCAST_SLOTFRAME_LENGTH) / 100)
#else
#define UPDATE_POLICY_INTERVAL UPDATE_POLICY_INTERVAL_CONF
#endif

// UDP communication process
PROCESS(node_udp_process, "UDP communicatio process");
// Q-Learning and scheduling process
PROCESS(scheduler_process, "QL-TSCH Scheduler Process");

AUTOSTART_PROCESSES(&node_udp_process, &scheduler_process);

// data to send to the server
unsigned char custom_payload[UDP_PLAYLOAD_SIZE];

// Broadcast slotframe and Unicast slotframe
struct tsch_slotframe *sf_broadcast;
struct tsch_slotframe *sf_unicast;

// array to store the links of the unicast slotframe
struct tsch_link *links_unicast_sf[UNICAST_SLOTFRAME_LENGTH];

// a variable to store the current action number
uint8_t current_action = 0;

// array to store Q-values of the actions (or timeslots)
float q_values[UNICAST_SLOTFRAME_LENGTH];

// reward values
int reward_succes = 1;
int reward_failure = 0;

// cycles since the beginning of the first slotframe
uint16_t cycles_since_start = 0;
uint8_t schedule_setup = 0;

// epsilon-greedy probability
float epsilon_fixed = 0.5;

// Q-learning parameters
float learning_rate = 0.1;
float discount_factor = 0.95;

// Set up the initial schedule
static void init_tsch_schedule(void)
{
  // delete all the slotframes
  tsch_schedule_remove_all_slotframes();

  // create a broadcast slotframe and a unicast slotframe
  sf_broadcast = tsch_schedule_add_slotframe(0, BROADCAST_SLOTFRAME_LENGTH);
  sf_unicast = tsch_schedule_add_slotframe(1, UNICAST_SLOTFRAME_LENGTH);

  // shared/advertising cell at (0, 0) --> create a shared/advertising link in the broadcast slotframe
  tsch_schedule_add_link(sf_broadcast, LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED,
                         LINK_TYPE_ADVERTISING, &tsch_broadcast_address, 0, 0, 0);

  // create one Tx link in the fisrt slot of the unicast slotframe (if this is a simple node, otherwise it will be Rx link)
  links_unicast_sf[0] = tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                LINK_TYPE_NORMAL, &tsch_broadcast_address, 0, 0, 0);

  // create multiple Rx links in the rest of the unicast slotframe
  for (int i = 1; i < UNICAST_SLOTFRAME_LENGTH; i++)
  {
    links_unicast_sf[i] = tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                 LINK_TYPE_NORMAL, &tsch_broadcast_address, i, 0, 0);
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

// function to populate the payload
void create_payload()
{
  for (uint16_t i = 0; i < UDP_PLAYLOAD_SIZE; i++)
  {
    custom_payload[i] = i % 26 + 'a';
  }
}

// initialize q-values randomly or set all to 0
void initialize_q_values(uint8_t val)
{
  if (val){
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      q_values[i] = (float) random_rand()/RANDOM_RAND_MAX;
    }
  } else {
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      q_values[i] = 0;
    }
  }
}

// choose exploration/explotation ==> 1/0 (gradient-greedy function)
uint8_t policy_check()
{ 
  float num = (float) random_rand()/RANDOM_RAND_MAX;
  float epsilon_new = (10000.0 / (float)cycles_since_start);
  if (epsilon_new > epsilon_fixed) epsilon_new = epsilon_fixed;
  
  if (num < epsilon_new)
      return 1;
  else
      return 0;  
}

// function to find the highest Q-value and return its index
uint8_t max_q_value_index()
{
  uint8_t max = random_rand()%UNICAST_SLOTFRAME_LENGTH;
  for (uint8_t i = 1; i < UNICAST_SLOTFRAME_LENGTH; i++)
  {
    if (q_values[i] > q_values[max])
    {
      max = i;
    }
  }
  return max;
}

// Update q-value table function
void update_q_table(uint8_t action, int reward)
{ 
  uint8_t max = max_q_value_index();
  float expected_max_q_value = q_values[max] + reward_succes;
  q_values[action] = (1 - learning_rate) * q_values[action] + 
                      learning_rate * (reward + discount_factor * expected_max_q_value -
                      q_values[action]);
}

// link selector function
int my_callback_packet_ready(void)
{
#if TSCH_CONF_WITH_LINK_SELECTOR
  uint8_t slotframe = 0;
  uint8_t channel_offset = 0;
  uint8_t timeslot = 0;

  if (packetbuf_hdrlen() == UDP_HEADER_LEN)
  {
    timeslot = current_action;
    slotframe = 1;
    LOG_INFO("Current Packet is a UDP packet\n");
  }
  LOG_INFO("Packet header length: %u\n", packetbuf_hdrlen());
#if TSCH_WITH_LINK_SELECTOR
  packetbuf_set_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME, slotframe);
  packetbuf_set_attr(PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
  packetbuf_set_attr(PACKETBUF_ATTR_TSCH_CHANNEL_OFFSET, channel_offset);
#endif /* TSCH_WITH_LINK_SELECTOR */
#endif /* TSCH_CONF_WITH_LINK_SELECTOR */
  return 1;
}

// function to receive udp packets
static void rx_packet(struct simple_udp_connection *c, const uip_ipaddr_t *sender_addr,
                      uint16_t sender_port, const uip_ipaddr_t *receiver_addr,
                      uint16_t receiver_port, const uint8_t *data, uint16_t datalen)
{
  char received_data[UDP_PLAYLOAD_SIZE];
  memcpy(received_data, data, datalen);

  uint16_t packet_num = 0;
  packet_num = received_data[1] && 0xFF;
  packet_num = (packet_num << 8) + (received_data[0] && 0xFF);

  LOG_INFO("Received_from node: %d packet_num: %u\n", sender_addr->u8[15], packet_num);
  // LOG_INFO_6ADDR(sender_addr);
  // LOG_INFO_("node: %d\n", sender_addr->u8[15]);
  // LOG_INFO_("  data: %s\n", data);
}

/********** UDP Communication Process - Start ***********/
PROCESS_THREAD(node_udp_process, ev, data)
{
  static struct simple_udp_connection udp_conn;
  static struct etimer periodic_timer;

  static uint16_t seqnum;
  uip_ipaddr_t dst;

  PROCESS_BEGIN();

  // set values of APT table to 0s
  reset_apt_table();
  // creating the payload
  create_payload();
  // initialize q-values
  initialize_q_values(0);
  // set up the initial schedule
  init_tsch_schedule();
  
  //------------------------
  // NETSTACK_RADIO.off()
  // NETSTACK_MAC.off()
  //------------------------

  /* Initialization; `rx_packet` is the function for packet reception */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, rx_packet);

  if (node_id == 1)
  { /* node_id is 1, then start as root*/
    NETSTACK_ROUTING.root_start();
  }
  // Initialize the mac layer
  NETSTACK_MAC.on();

  // start QL-TSCH protocol
  schedule_setup = 1;

  // if this is a simple node, start sending upd packets
  LOG_INFO("Started UDP communication\n");

  // start the timer for periodic udp packet sending
  etimer_set(&periodic_timer, SEND_INTERVAL);
  
  /* Main UDP comm Loop */
  while (1)
  {
    // print the Q-values
    LOG_INFO("Q-Values:");
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      LOG_INFO_(" %u-> %f", i, q_values[i]);
    }
    LOG_INFO_("\n");

    // print APT table values
    LOG_INFO("APT-Values:");
    uint8_t *table = get_apt_table();
    for (uint8_t i = 0; i < UNICAST_SLOTFRAME_LENGTH; i++){
      LOG_INFO_(" (%u->%u)", i, table[i]);
    }
    LOG_INFO_("\n");
    LOG_INFO("Total frame cycles: %u\n", cycles_since_start);

    // reset all the backoff windows for all the neighbours
    // custom_reset_all_backoff_exponents();
    // reset APT-table values
    reset_apt_table();

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if (node_id != 1){
      if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
      {
        /* Send the packet number to the root and extra data */
        custom_payload[0] = seqnum && 0xFF;
        custom_payload[1] = (seqnum >> 8) && 0xFF;
        LOG_INFO("Send_to ");
        LOG_INFO_6ADDR(&dst);
        LOG_INFO_(" packet_number: %" PRIu32 "\n", seqnum);
        simple_udp_sendto(&udp_conn, &custom_payload, UDP_PLAYLOAD_SIZE, &dst);
        seqnum++;
      }
    }
    etimer_set(&periodic_timer, SEND_INTERVAL);
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
  
  // wait untill the initial setupt finishes
  while(1) if(schedule_setup) break;
  LOG_INFO("Finished Setting up cycles: %u\n", cycles_since_start);
  
  // set the timer for one whole frame cycle 
  etimer_set(&policy_update_timer, UPDATE_POLICY_INTERVAL);

  /* Main Scheduler Loop */
  while (1)
  { 
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));

#if WITH_TSCH_LOCKING
    // lock time-slotting before starting the first schedule
    while(1) if (tsch_get_lock() == 1) break;
    // tsch_get_lock();
    // LOG_INFO("TSCH got locked -> WARNING !!!\n");
#endif /* WITH_TSCH_LOCKING */

    /**********  Q-value update calculations - Start **********/
    
    // updating the q-table based on the last action results
    uint8_t transmission_status = get_and_reset_Tx_slot_status();
    if (transmission_status){
      if (transmission_status == 1){
        update_q_table(current_action, reward_succes);
      } else {
        update_q_table(current_action, reward_failure);
      }
      // LOG_INFO("Updating the Q-table\n");
    }
    // LOG_INFO("Transmission status: %u\n", transmission_status);
    
    // choosing exploration/exploatation and updating the schedule
    uint8_t action;
    if (policy_check() == 1){ /* Exploration */
      action = get_slot_with_apt_table_min_value();
      // LOG_INFO("Exploartion is selected. Action is %u\n", action);
    } else { /* Explotation */
      action = max_q_value_index();
      // LOG_INFO("Explotation is selected. Action is %u\n", action);
    }

#if WITH_TSCH_LOCKING
    // start the slot operations again and set the timer
    tsch_release_lock();
    // LOG_INFO("TSCH lock released -> WARNING !!!\n");
#endif /* WITH_TSCH_LOCKING */

    // set up a new schedule after releasing the TSCH lock
    set_up_new_schedule(action);

    // set the timer again -> duration = (unicast + broadcast) slotframe cycle
    while (1) if (!tsch_is_locked()) break;
    etimer_set(&policy_update_timer, UPDATE_POLICY_INTERVAL);

    current_action = action;
    cycles_since_start++;

    /**********  Q-value update calculations - End **********/

// #if WITH_TSCH_LOCKING
//     // start the slot operations again and set the timer
//     tsch_release_lock();
// #endif /* WITH_TSCH_LOCKING */

  }
  PROCESS_END();
}
/********** RL-TSCH Scheduler Process - End ***********/
