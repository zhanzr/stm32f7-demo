/**
  * @file    eth_http_server/src/main.c
  * @brief   Minimal HTTP server on the NUCLEO-F746ZG (lwIP raw API,
  *          static IP + LAN8742 PHY). Follows the disco-f769/bare/eth_http
  *          port of the ST LwIP_HTTP_Server_Raw example; the disco's LCD
  *          status page is replaced by USART3 console prints. Runs at the
  *          Nucleo's native 216 MHz (BOARD_PLL_N=432), the clock ST's
  *          NUCLEO-F746ZG LwIP example uses.
  */

#include "board.h"
#include "uart_printf.h"
#include "stm32f7xx_hal.h"
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "app_ethernet.h"
#include "http_server.h"
#include "main.h"

/* Global network interface */
struct netif gnetif;

/* Ethernet handle owned by ethernetif.c */
extern ETH_HandleTypeDef EthHandle;
extern volatile uint32_t eth_rx_cnt;
extern volatile uint32_t eth_tx_cnt;

/* ------------------------------------------------------------------------ */
static void Netif_Config(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

    /* Static IP (no DHCP on this board, LWIP_DHCP=0): 192.168.5.210/24 on
     * the same LAN as the host PC (192.168.5.99). */
    IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);

    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
    netif_set_default(&gnetif);

#if LWIP_NETIF_LINK_CALLBACK
    netif_set_link_callback(&gnetif, ethernet_link_status_updated);
#endif
}

/* ------------------------------------------------------------------------ */
void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&EthHandle);
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();          /* 216 MHz (BOARD_PLL_N=432), caches, MPU, console */

    /* The ETH DMA buffers live in the cached SRAM1 (.bss). Rather than
     * carve out a special non-cacheable region, simply disable the D-cache:
     * this app is network-bound, so the small loss is irrelevant, and
     * CPU<->DMA stay fully coherent with no cache maintenance. */
    SCB_DisableDCache();

    printf("\r\n=== eth_http_server on NUCLEO-F746ZG @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);
    printf("HTTP server: static IP 192.168.5.210 (no DHCP)\r\n");

    lwip_init();
    Netif_Config();
    http_server_init();

    {
        char ip_str[16];
        ip4addr_ntoa_r(netif_ip4_addr(&gnetif), ip_str, sizeof(ip_str));
        printf("ETH: static IP %s - browse to http://%s/\r\n", ip_str, ip_str);
        printf("ETH: MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n",
               gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
               gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);
    }

    /* RMII re-select: see the main-loop comment below. */
    uint32_t rmii_timer = 0;
    uint32_t status_timer = 0;
    uint32_t last_tx = 0;
    char ip_str[16];

    while (1)
    {
        ethernetif_input(&gnetif);        /* poll RX */
        sys_check_timeouts();             /* lwIP timers */
#if LWIP_NETIF_LINK_CALLBACK
        Ethernet_Link_Periodic_Handle(&gnetif);
#endif
        /* RMII re-select, unconditionally every 500 ms while the link is
         * up (the ST workaround the disco port runs on RX stalls; run it
         * flat-out here so the MAC/PHY sampling stays re-synced on a quiet
         * LAN too). */
        if (netif_is_link_up(&gnetif) && HAL_GetTick() - rmii_timer >= 500)
        {
            rmii_timer = HAL_GetTick();
            SYSCFG->PMC &= ~SYSCFG_PMC_MII_RMII_SEL;
            SYSCFG->PMC |= HAL_ETH_RMII_MODE;
            (void)SYSCFG->PMC;
        }

        /* Console status every 5 s (or immediately when the TX count moves,
         * i.e. the server just answered a request). */
        if (HAL_GetTick() - status_timer >= 5000 || eth_tx_cnt != last_tx)
        {
            status_timer = HAL_GetTick();
            last_tx = eth_tx_cnt;
            ip4addr_ntoa_r(netif_ip4_addr(&gnetif), ip_str, sizeof(ip_str));
            printf("ETH: ip %s, link %s, rx %lu, tx %lu\r\n",
                   ip_str, netif_is_link_up(&gnetif) ? "UP" : "DOWN",
                   (unsigned long)eth_rx_cnt, (unsigned long)eth_tx_cnt);
        }
    }
}
