# eth_http - minimal HTTP server on the STM32F769I-Discovery

A bare-metal web server over the on-board Ethernet (10/100 RMII, LAN8742 PHY)
using **lwIP 2.1.2** in **raw-API / NO_SYS** mode, plus DHCP. Adapted from the
ST `LwIP_HTTP_Server_Raw` example (STM32F769I-EVAL) and reworked for the
Discovery's RMII pins and LAN8742 PHY.

## Pages

* `/` - home page.
* `/led.shtml` - control the three on-board LEDs (**LD1/LD2/LD3**) via
  `GET /leds.cgi?led=1..3` (check = on, uncheck = off). The page is served with
  lwIP SSI tags so the returned checkboxes reflect the **actual LED state**
  (read from the GPIOs, not just what was last submitted).
* `/status.shtml` - the three internal ADC channels (VDDA / die temperature /
  VBAT), auto-refreshed every 2 s via an lwIP SSI tag. Same measurement as the
  `blink_hello` project.

The on-board **LCD** shows the network status - IP address (big font), TX/RX
frame counters (middle font) and the MAC address (small font). The refresh is
coalesced (`src/lcd_display.c`): `main()` polls the counters every loop, but a
burst of traffic just re-arms a 400 ms debounce timer, so the screen is only
repainted once the counters settle (or at the latest after 2 s during a
sustained burst). A repaint itself touches only the line band whose value
changed, never the whole frame.

The embedded filesystem (`src/fsdata_custom.c`) is generated from `html/` by
`make_fsdata.py`:

```bash
python make_fsdata.py     # regenerate src/fsdata_custom.c after editing html/
```

## Ethernet notes

* **RMII + LAN8742**: pins PA1/PA2/PA7, PC1/PC4/PC5, PG11/PG13/PG14 (AF11).
* **Clock: 200 MHz, not 216 MHz.** The on-board Ethernet only works reliably
  at 200 MHz on this board (at 216 MHz neither TX nor RX passes frames). This
  project therefore builds with `BOARD_PLL_N=400` (see the shared board layer),
  the same clock the ST LwIP examples use.
* **RMII watchdog**: the Discovery's RMII interface is marginal, so `main()`
  re-selects the RMII mode (SYSCFG) whenever no frame is received for 500 ms
  while the link is up (the same workaround as the ST example).
* **DMA buffers in the normal `.sdram` pool.** The Ethernet DMA cannot reach
  the core-private DTCM, so the DMA descriptors, RX buffers and the TX bounce
  buffer are plain static arrays in the on-board SDRAM (`.sdram` section,
  same pool as the LCD framebuffer / sdram_test). To keep CPU and DMA coherent
  with no cache maintenance and no special memory region, `main()` disables
  the D-cache - fine for an I/O-bound web server.
* **TX is bounced**: lwIP's pbuf pool lives in the `.bss` section (AXI SRAM,
  DMA-accessible), but `low_level_output` still copies each frame into the
  SDRAM TX bounce buffer before `HAL_ETH_Transmit`, so TX works identically
  whatever memory the pbufs land in.
* RX is polled in `main()` (`ethernetif_input`), no RTOS required.

## Build & flash

```bash
bash build.sh                 # == cmake -G Ninja .. && ninja
ninja flash                   # probe-rs through the on-board ST-Link V2 (SWD)
```

Open the USART1 console (`COMxx` @ 115200 via the ST-Link V2 VCP). When the
link is up and DHCP completes you will see the assigned IP; browse to
`http://<ip>/`.

## DHCP through a Windows network bridge

The firmware is a standard DHCP client, so it works on any normal L2 path
(switch/router). If you connect it through a **Windows Network Bridge**
(Wi-Fi + Ethernet bridged on the PC), the board can get an IP from the router
provided the bridge forwards frames to the board's Ethernet member - Windows
bridges sometimes do not, in which case DHCP times out and the static IP
(`192.168.5.200`) is used. To isolate the board from the bridge, plug the board
straight into the router/switch (or give it a static IP on the bridge subnet
and ping it).
