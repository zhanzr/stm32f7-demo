/**
  * @file    eth_http/src/main.c
  * @brief   Minimal HTTP server on the STM32F769I-Discovery (lwIP raw API,
  *          DHCP + LAN8742 PHY). Adapted from the ST LwIP_HTTP_Server_Raw
  *          example. Uses the shared board layer (Board_Init) for the
  *          216 MHz clock, caches, UART console and the SDRAM.
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
#include "lcd_display.h"

/* Global network interface */
struct netif gnetif;

/* Ethernet handle owned by ethernetif.c */
extern ETH_HandleTypeDef EthHandle;
extern volatile uint32_t eth_rx_cnt;
extern volatile uint32_t eth_tx_cnt;
#if LWIP_DHCP
extern uint8_t DHCP_state;
#endif

/* ------------------------------------------------------------------------ */
static void Netif_Config(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

#if LWIP_DHCP
    ip_addr_set_zero_ip4(&ipaddr);
    ip_addr_set_zero_ip4(&netmask);
    ip_addr_set_zero_ip4(&gw);
#else
    IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif

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

/* MIPI DSI / LTDC / DMA2D IRQs for the on-board LCD (BSP_LCD). */
extern LTDC_HandleTypeDef hltdc_discovery;
extern DSI_HandleTypeDef hdsi_discovery;
extern DMA2D_HandleTypeDef hdma2d_discovery;
void DSI_IRQHandler(void)  { HAL_DSI_IRQHandler(&hdsi_discovery); }
void LTDC_IRQHandler(void) { HAL_LTDC_IRQHandler(&hltdc_discovery); }
void DMA2D_IRQHandler(void){ HAL_DMA2D_IRQHandler(&hdma2d_discovery); }

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();          /* 200 MHz (BOARD_PLL_N=400), caches, MPU, console */

    /* The ETH DMA buffers live in the write-through-cached SDRAM (`.sdram`).
     * Rather than carve out a special non-cacheable region, simply disable
     * the D-cache: this app is network-bound, so the small loss in SDRAM
     * throughput is irrelevant, and CPU<->DMA stay fully coherent with no
     * cache maintenance and no special memory layout. */
    SCB_DisableDCache();

    printf("\r\n=== eth_http on STM32F769NI @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);
    printf("HTTP server: http://<dhcp-ip>/  (DHCP enabled)\r\n");

    lcd_display_init();

    lwip_init();
    Netif_Config();
    http_server_init();

    /* The Discovery's RMII interface is marginal: the ST example runs a
     * watchdog that re-selects the RMII mode until good frames arrive. Do the
     * same - if no frame is received for 500 ms while the link is up, toggle
     * the SYSCFG MII/RMII selection to re-sync the MAC onto the PHY's 50 MHz
     * REF_CLK. */
    uint32_t rmii_timer = 0;
    uint32_t last_rx = 0;
    char ip_str[16];
    char mac_str[18];

    while (1)
    {
        ethernetif_input(&gnetif);        /* poll RX */
        sys_check_timeouts();             /* lwIP timers */
#if LWIP_NETIF_LINK_CALLBACK
        Ethernet_Link_Periodic_Handle(&gnetif);
#endif
#if LWIP_DHCP
        DHCP_Periodic_Handle(&gnetif);
#endif
        if (netif_is_link_up(&gnetif) && eth_rx_cnt == last_rx &&
            HAL_GetTick() - rmii_timer >= 500)
        {
            rmii_timer = HAL_GetTick();
            SYSCFG->PMC &= ~SYSCFG_PMC_MII_RMII_SEL;
            SYSCFG->PMC |= HAL_ETH_RMII_MODE;
            (void)SYSCFG->PMC;
        }
        last_rx = eth_rx_cnt;

        /* LCD status: lcd_display_poll() coalesces bursts and repaints only
         * the lines that changed, once the counters settle. */
        ip4addr_ntoa_r(netif_ip4_addr(&gnetif), ip_str, sizeof(ip_str));
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
                 gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);
        lcd_display_poll(ip_str, mac_str, eth_rx_cnt, eth_tx_cnt);
    }
}
