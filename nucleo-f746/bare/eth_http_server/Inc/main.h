#ifndef __ETH_HTTP_SERVER_MAIN_H
#define __ETH_HTTP_SERVER_MAIN_H

/* Ethernet MAC address (locally administered; last byte differs from the
 * disco-f769 eth_http board so both can share one LAN). */
#define ETH_MAC_ADDR0  ((uint8_t)0x02)
#define ETH_MAC_ADDR1  ((uint8_t)0x00)
#define ETH_MAC_ADDR2  ((uint8_t)0x00)
#define ETH_MAC_ADDR3  ((uint8_t)0x12)
#define ETH_MAC_ADDR4  ((uint8_t)0x34)
#define ETH_MAC_ADDR5  ((uint8_t)0x57)

/* Static IP (no DHCP on this board): the host PC's NIC is 192.168.5.99, the
 * board sits in the same 192.168.5.0/24 LAN as 192.168.5.210.
 * Browse to:  http://192.168.5.210/  */
#define IP_ADDR0  192U
#define IP_ADDR1  168U
#define IP_ADDR2  5U
#define IP_ADDR3  210U
#define NETMASK_ADDR0  255U
#define NETMASK_ADDR1  255U
#define NETMASK_ADDR2  255U
#define NETMASK_ADDR3  0U
#define GW_ADDR0  192U
#define GW_ADDR1  168U
#define GW_ADDR2  5U
#define GW_ADDR3  1U

#endif /* __ETH_HTTP_SERVER_MAIN_H */
