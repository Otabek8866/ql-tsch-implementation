#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */

/* USB serial takes space, free more space elsewhere */
#define SICSLOWPAN_CONF_FRAG 0
#define UIP_CONF_BUFFER_SIZE 160

/*******************************************************/
/******************* Configure TSCH ********************/
/*******************************************************/

/* Disable the 6TiSCH minimal schedule */
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0

/* IEEE802.15.4 PANID */
#define IEEE802154_CONF_PANID 0x81a5

/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
// you can use this funtion to finish initialization
#define TSCH_CONF_AUTOSTART 0

/* Length of slotframes */
#define BROADCAST_SLOTFRAME_LENGTH 7
#define UNICAST_SLOTFRAME_LENGTH 15

// UDP packet sending interval in seconds
#define PACKET_SENDING_INTERVAL 30

// UDP packet payload size
#define UDP_PLAYLOAD_SIZE 50

// MAX number of re-transmissions
#define TSCH_CONF_MAX_FRAME_RETRIES 3

// hopping sequence
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_2_2

// define a link selector function
#define TSCH_CONF_WITH_LINK_SELECTOR 0
#define TSCH_CALLBACK_PACKET_READY my_callback_packet_ready

// macros to enbale QL-TSCH in tsch libriaries
#define QL_TSCH_ENABLED_CONF 1

// Run algorithm with tsch locking
#define WITH_TSCH_LOCKING 0

// Random Q-values or 0s (at the beginning)
#define RANDOM_Q_VALUES 1

// print Q-values and APT values
#define PRINT_Q_VALUES_AND_APT 1

// Default slotframe length
// #define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH 7

// Packetbuffer size
// #define PACKETBUF_CONF_SIZE 128 // 128 default

// The 6lowpan "headers" length
// #define TSCH_CONF_WITH_SIXTOP 1

/*******************************************************/
#if WITH_SECURITY
/* Enable security */
#define LLSEC802154_CONF_ENABLED 1
#endif /* WITH_SECURITY */

/*******************************************************/
/************* Other system configuration **************/
/*******************************************************/

/* Logging */
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_WARN // alternative _INFO/WARN
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT 1

#endif /* PROJECT_CONF_H_ */
