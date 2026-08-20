/**
  * @file    eth_http/src/lcd_display.c
  * @brief   Network status on the on-board MIPI DSI LCD: IP (big font),
  *          TX/RX counters (middle font), MAC address (small font).
  *
  * The refresh is coalesced: a burst of RX/TX just re-arms a debounce timer,
  * and the screen is repainted once the counters settle (or after a maximum
  * hold time during a very long burst). A repaint itself touches only the
  * line band whose value changed, never the whole 800x480 frame.
  */

#include <stdio.h>
#include <string.h>
#include "stm32f7xx_hal.h"
#include "stm32f769i_discovery_lcd.h"

#define FB_W   800
#define FB_H   480
static uint32_t framebuffer[FB_W * FB_H] __attribute__((section(".sdram")));

/* Repaint policy: wait for the counters to be stable for DEBOUNCE_MS, but
 * never let the screen freeze longer than MAX_HOLD_MS during a sustained
 * burst. */
#define LCD_DEBOUNCE_MS  400
#define LCD_MAX_HOLD_MS  2000

/* Line geometry. Each clear band is wider than the longest possible string,
 * so a shrinking value cannot leave stale pixels behind. */
#define IP_X   24
#define IP_Y   60
#define IP_W   340
#define IP_H   24                 /* Font24 */
#define MAC_X  24
#define MAC_Y  448
#define MAC_W  240
#define MAC_H  12                 /* Font12 */
#define CTR_X  24
#define CTR_W  240
#define CTR_H  20                 /* Font20 */
#define RX_Y   220
#define TX_Y   270

typedef struct
{
    char    ip[16];
    char    mac[18];
    uint32_t rx;
    uint32_t tx;
} lcd_state_t;

static lcd_state_t pending;       /* latest snapshot, waiting for the settle */
static uint32_t     pend_change_tick;  /* last time a value changed */
static uint32_t     pend_start_tick;   /* when this "episode" started */
static int          pend_valid;

static lcd_state_t shown;         /* what is on the screen */
static int          painted;

static void clear_line(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_FillRect(x, y, w, h);
}

/* Repaint only the line bands whose value changed vs. what is shown. */
static void lcd_repaint(const char *ip, const char *mac, uint32_t rx, uint32_t tx)
{
    char s[80];

    if (!painted || strcmp(shown.ip, ip) != 0)
    {
        clear_line(IP_X, IP_Y, IP_W, IP_H);
        BSP_LCD_SetFont(&Font24);
        BSP_LCD_SetTextColor(LCD_COLOR_YELLOW);
        snprintf(s, sizeof(s), "IP  %s", ip);
        BSP_LCD_DisplayStringAt(IP_X, IP_Y, (uint8_t *)s, LEFT_MODE);
    }
    if (!painted || shown.rx != rx)
    {
        clear_line(CTR_X, RX_Y, CTR_W, CTR_H);
        BSP_LCD_SetFont(&Font20);
        BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
        snprintf(s, sizeof(s), "RX  %lu", (unsigned long)rx);
        BSP_LCD_DisplayStringAt(CTR_X, RX_Y, (uint8_t *)s, LEFT_MODE);
    }
    if (!painted || shown.tx != tx)
    {
        clear_line(CTR_X, TX_Y, CTR_W, CTR_H);
        BSP_LCD_SetFont(&Font20);
        BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
        snprintf(s, sizeof(s), "TX  %lu", (unsigned long)tx);
        BSP_LCD_DisplayStringAt(CTR_X, TX_Y, (uint8_t *)s, LEFT_MODE);
    }
    if (!painted || strcmp(shown.mac, mac) != 0)
    {
        clear_line(MAC_X, MAC_Y, MAC_W, MAC_H);
        BSP_LCD_SetFont(&Font12);
        BSP_LCD_SetTextColor(LCD_COLOR_GRAY);
        snprintf(s, sizeof(s), "MAC %s", mac);
        BSP_LCD_DisplayStringAt(MAC_X, MAC_Y, (uint8_t *)s, LEFT_MODE);
    }

    snprintf(shown.ip, sizeof(shown.ip), "%s", ip);
    snprintf(shown.mac, sizeof(shown.mac), "%s", mac);
    shown.rx = rx;
    shown.tx = tx;
    painted = 1;
}

void lcd_display_init(void)
{
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, (uint32_t)framebuffer);
    BSP_LCD_DisplayOn();
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    memset(&pending, 0, sizeof(pending));
    memset(&shown, 0, sizeof(shown));
    pend_change_tick = pend_start_tick = 0;
    pend_valid = painted = 0;
}

/* Poll the network state (call from the main loop). The screen is only
 * repainted once the values stop changing for a while, or at the latest after
 * LCD_MAX_HOLD_MS during a burst. */
void lcd_display_poll(const char *ip, const char *mac, uint32_t rx, uint32_t tx)
{
    uint32_t now = HAL_GetTick();

    /* A new value vs. the last sample starts/re-arms the settle window. */
    if (!pend_valid ||
        strcmp(pending.ip, ip) != 0 || strcmp(pending.mac, mac) != 0 ||
        pending.rx != rx || pending.tx != tx)
    {
        if (!pend_valid)
        {
            pend_start_tick = now;
        }
        snprintf(pending.ip, sizeof(pending.ip), "%s", ip);
        snprintf(pending.mac, sizeof(pending.mac), "%s", mac);
        pending.rx = rx;
        pending.tx = tx;
        pend_change_tick = now;
        pend_valid = 1;
    }

    /* Repaint when the burst settled, or when the hold time expired. */
    if (pend_valid &&
        ((now - pend_change_tick) >= LCD_DEBOUNCE_MS ||
         (now - pend_start_tick) >= LCD_MAX_HOLD_MS))
    {
        lcd_repaint(pending.ip, pending.mac, pending.rx, pending.tx);
        pend_valid = 0;
    }
}
