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

// period to to check Tx status
#define TIME_10MS (CLOCK_SECOND * 0.01)

// number to update APT values 0 < num < 1 (APT values will decay to 0s)
#define APT_UPDATE_NUMBER 0.9

// Tx slot status checking time interval in 10ms
#define TX_SLOT_STATUS_CHECK 1

// UDP communication process
PROCESS(node_udp_process, "UDP communicatio process");
// Q-Learning and scheduling process
PROCESS(scheduler_process, "QL-TSCH Scheduler Process");
// APT updating process
PROCESS(apt_update_process, "APT Updating Process");

AUTOSTART_PROCESSES(&node_udp_process, &scheduler_process, &apt_update_process);

// data to send to the server
unsigned char custom_payload[UDP_PLAYLOAD_SIZE];

// Broadcast slotframe and Unicast slotframe
struct tsch_slotframe *sf_broadcast;
struct tsch_slotframe *sf_unicast;

// array to store the links of the unicast slotframe
// struct tsch_link *links_unicast_sf[UNICAST_SLOTFRAME_LENGTH];

// a variable to store the current action number
uint8_t current_action = 0;

// array to store Q-values of the actions (or timeslots)
float q_values[UNICAST_SLOTFRAME_LENGTH];

// reward values
int reward_succes = 0;
int reward_failure = -1;

// cycles since the beginning of the first slotframe
uint16_t cycles_since_start = 0;

// epsilon-greedy probability
float epsilon_fixed = 0.5;

// Q-learning parameters
float learning_rate = 0.1;
float discount_factor = 0.95;

// Tx status and timeslot number
uint8_t *Tx_status;

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
  tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                LINK_TYPE_NORMAL, &tsch_broadcast_address, 0, 0, 0);

  // create multiple Rx links in the rest of the unicast slotframe
  for (int i = 1; i < UNICAST_SLOTFRAME_LENGTH; i++)
  {
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                 LINK_TYPE_NORMAL, &tsch_broadcast_address, i, 0, 0);
  }
}

// set up new schedule based on the chosen action
void set_up_new_schedule(uint8_t action)
{ 
  if (action != current_action)
  {
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_TX | LINK_OPTION_SHARED,
                                                      LINK_TYPE_NORMAL, &tsch_broadcast_address, action, 0, 1);
    tsch_schedule_add_link(sf_unicast, LINK_OPTION_RX | LINK_OPTION_SHARED,
                                                              LINK_TYPE_NORMAL, &tsch_broadcast_address, current_action, 0, 1);
    current_action = action;
  }
}

// function to populate the payload
void create_payload()
{
  for (uint16_t i = 4; i < UDP_PLAYLOAD_SIZE; i++)
  {
    custom_payload[i] = i + 'a';
  }
  custom_payload[2] = 0xFF;
  custom_payload[3] = 0xFF;
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
  float epsilon_new = (10000.0 / cycles_since_start);
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

  char *ch = packetbuf_dataptr();
  uint8_t f0 = ch[0] & 0xFF, f1 = ch[1] & 0xFF, f2 = ch[2] & 0xFF, f3 = ch[3] & 0xFF;

  if (f0 == 126 && f1 == 247 && f2 == 0 && f3 == 225)
  {
    timeslot = current_action;
    slotframe = 1;
  }
  // LOG_INFO("Packet header length: %u\n", packetbuf_hdrlen());
  // LOG_INFO("Packet data length: %u\n", packetbuf_datalen());

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

  uint16_t packet_num;
  packet_num = received_data[1] & 0xFF;
  packet_num = (packet_num << 8) + (received_data[0] & 0xFF);

  LOG_INFO("Received_from %d packet_number: %d\n", sender_addr->u8[15], packet_num);
  // LOG_INFO_6ADDR(sender_addr);
}

/********** UDP Communication Process - Start ***********/
PROCESS_THREAD(node_udp_process, ev, data)
{
  static struct simple_udp_connection udp_conn;
  static struct etimer periodic_timer;

  static uint16_t seqnum;
  uip_ipaddr_t dst;

  PROCESS_BEGIN();

  // creating the payload
  create_payload();
  // initialize q-values
  initialize_q_values(RANDOM_Q_VALUES);
  // set all the APT values to 0s at the beginning
  set_apt_values();
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

  // if this is a simple node, start sending upd packets
  LOG_INFO("Started UDP communication\n");

  // start the timer for periodic udp packet sending
  etimer_set(&periodic_timer, SEND_INTERVAL);
  
  /* Main UDP comm Loop */
  while (1)
  {
#if PRINT_Q_VALUES_AND_APT
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
#endif /* PRINT_Q_VALUES_AND_APT */

    // reset all the backoff windows for all the neighbours
    custom_reset_all_backoff_exponents();
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if (node_id != 1){
      if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dst))
      {
        /* Send the packet number to the root and extra data */
        custom_payload[0] = seqnum & 0xFF;
        custom_payload[1] = (seqnum >> 8) & 0xFF;
        LOG_INFO("Sent_to %d packet_number: %d\n", dst.u8[15], seqnum);
        // LOG_INFO_6ADDR(&dst);
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
  Tx_status = get_Tx_slot_status();

  PROCESS_BEGIN();
  
  // wait untill the initial setupt finishes
  LOG_INFO("Starting Q-table Update Process - Setup finished\n");
  
  // set the timer for one whole frame cycle 
  etimer_set(&policy_update_timer, TIME_10MS * TX_SLOT_STATUS_CHECK);

  /* Main Scheduler Loop */
  while (1)
  { 
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));

#if WITH_TSCH_LOCKING
    // lock time-slotting before starting the first schedule
    while(1) if (tsch_get_lock() == 1) break;
    // tsch_get_lock();
#endif /* WITH_TSCH_LOCKING */

    /**********  Q-value update calculations - Start **********/
    
    // updating the q-table based on the last action results
    if (Tx_status[0]) {
      etimer_set(&policy_update_timer, (TIME_10MS * (UNICAST_SLOTFRAME_LENGTH - 1 - Tx_status[1])));
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&policy_update_timer));
      if (Tx_status[0] == 1){
        update_q_table(current_action, reward_succes);
      } else {
        update_q_table(current_action, reward_failure);
      }
      Tx_status[0] = 0;
    //} //------------ change is here -------------------
    
    // choosing exploration/exploatation and updating the schedule
      uint8_t action;
      if (policy_check() == 1){ /* Exploration */
        action = get_slot_with_apt_table_min_value();
      } else { /* Explotation */
        action = max_q_value_index();
      }
      // set up a new schedule based on the chosen action
      set_up_new_schedule(action);
    } //------------ and here -------------------

#if WITH_TSCH_LOCKING
    // start the slot operations again and set the timer
    tsch_release_lock();
#endif /* WITH_TSCH_LOCKING */

    // set up a new schedule after releasing the TSCH lock
    // set_up_new_schedule(action);
    
    // set the timer again -> duration 10 ms
    etimer_set(&policy_update_timer, TIME_10MS * TX_SLOT_STATUS_CHECK);
    cycles_since_start++;

    /**********  Q-value update calculations - End **********/
  }
  PROCESS_END();
}
/********** QL-TSCH Scheduler Process - End ***********/

/********** APT Update Process - Start ***********/
PROCESS_THREAD(apt_update_process, ev, data)
{
  // timer to update APT
  static struct etimer apt_update_timer;
  
  PROCESS_BEGIN();
  
  // wait untill the initial setupt finishes
  LOG_INFO("Starting APT Update Process - Setup finished\n");
  
  // set the timer for one whole frame cycle 
  etimer_set(&apt_update_timer, (TIME_10MS * UNICAST_SLOTFRAME_LENGTH));

  /* Main APT Update function loop */
  while (1)
  { 
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&apt_update_timer));
    update_apt_table(APT_UPDATE_NUMBER);
    etimer_set(&apt_update_timer, (TIME_10MS * UNICAST_SLOTFRAME_LENGTH));
  }
  PROCESS_END();
}
/********** APT Update Process - end ***********/
