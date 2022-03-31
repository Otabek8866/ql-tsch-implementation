#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Set to enable TSCH security */
#ifndef WITH_SECURITY
#define WITH_SECURITY 0
#endif /* WITH_SECURITY */

/* USB serial takes space, free more space elsewhere */
// #define SICSLOWPAN_CONF_FRAG 0
// #define UIP_CONF_BUFFER_SIZE 160

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

/* 6TiSCH minimal schedule length */
#define BROADCAST_SLOTFRAME_LENGTH 7
#define UNICAST_SLOTFRAME_LENGTH 5

// Payload size
#define PACKETBUF_CONF_SIZE 125

// UDP packet payload size
#define UDP_PLAYLOAD_SIZE 50

// to list all the packets in the queue and get the total number
// #define QUEUEBUF_CONF_DEBUG 1
// #define QUEUEBUF_CONF_STATS 1

// MAX number of re-transmissions
#define TSCH_CONF_MAX_FRAME_RETRIES 3

// hopping sequence
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_2_2

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
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO // alternative _INFO/WARN
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_WARN
#define TSCH_LOG_CONF_PER_SLOT 1

#endif /* PROJECT_CONF_H_ */
