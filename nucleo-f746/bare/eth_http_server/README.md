# eth_http_server - embedded web server on the NUCLEO-F746ZG

A bare-metal web server over the on-board Ethernet (10/100 RMII, LAN8742A
PHY) using **lwIP 2.1.2** in **raw-API / NO_SYS** mode, with a **static IP**.
It serves the bundled single-page site (this board's `../e_server/` copy,
packed by `build_web.py` into `Inc/web_assets.h`). The HTTP server is a
custom raw-TCP app (`src/http_server.c`), ported from the
`disco-f769/bare/eth_http` project - same site, same API, same custom server;
the disco's LCD status page is replaced by USART3 console prints.

## Network configuration (static, no DHCP)

| Setting  | Value            |
| -------- | ---------------- |
| Board IP | **192.168.5.210** |
| Netmask  | 255.255.255.0    |
| Gateway  | 192.168.5.1      |
| MAC      | 02:00:00:12:34:57 (locally administered; differs from the disco board) |

The host PC shares the LAN (its wired NIC is on 192.168.5.0/24). Browse to:

```
http://192.168.5.210/
```

## Pages & API (same contract as the disco eth_http)

* **LED control** - three checkboxes for the Nucleo LEDs (LD1 PB0 green,
  LD2 PB7 blue, LD3 PB14 red); clicks POST to `/api/leds` and re-sync.
* **ADC values** - three canvas plots (VREFINT / die temperature / VBAT) fed
  by the ADC1 internal channels, 1/2/4 s sample interval.
* **Board info** - `arch` + `lan_ip`; `public_ip`/`geo`/`weather` are `null`
  (no HTTP/TLS client - see the disco README for the follow-up design).

| Route                | Description                                     |
| -------------------- | ----------------------------------------------- |
| `GET /`              | the page: gzip, `Content-Encoding: gzip`        |
| `GET /api/leds`      | `{"leds":[0,1,0]}` (real LED GPIO state)        |
| `POST /api/leds`     | body `{"leds":[0,1,0]}` -> applies, `{"leds":[...]}` |
| `GET /api/adc`       | `{"vrefint_mv":..,"temp_c":..,"vbat_v":..,"ts":..}` |
| `GET /api/info`      | `{"arch":"cortex-m7","lan_ip":"192.168.5.210",...}` |
| `GET /public/*`      | raw image bytes from the bundle (`image/avif`)  |

The console (USART3, PD8/PD9, ST-Link VCP @ 115200) prints the boot banner,
the static IP, the negotiated PHY link (speed/duplex) and a status line with
RX/TX frame counters every 5 s (or immediately after each served request):

```
=== eth_http_server on NUCLEO-F746ZG @ 216000000 Hz ===
ETH: static IP 192.168.5.210 - browse to http://192.168.5.210/
ETH PHY: link 2, speed 100M, duplex full
ETH: ip 192.168.5.210, link UP, rx 44, tx 30
```

## Ethernet notes (what differs from the disco port)

* **RMII TXD1 is on PB13 here, not PG14.** The F769I-Discovery wires RMII
  TXD1 to PG14; every NUCLEO-144 board wires it to **PB13**. The disco's
  pin init was copied verbatim at first and produced a bizarre failure
  mode: RX (PA7/PC4/PC5) worked, the board answered every ARP request
  (per its TX counter), but ~60% of the replies never reached the host -
  TXD1 was floating on the wrong port. With PB13 configured the link is
  rock solid (0% ping loss).
* **Clock: the Nucleo's native 216 MHz** (`BOARD_PLL_N=432`, like ST's
  NUCLEO-F746ZG LwIP example). The disco needed 200 MHz; this board does
  not.
* **DMA buffers in plain `.bss` (SRAM1).** The ETH DMA cannot reach DTCM,
  but it does reach SRAM1/SRAM2 - and this board's `.bss` already lives in
  SRAM1 (see `board/stm32f746xx.ld`), so no special section or TX SDRAM
  bounce region is needed (the TX bounce buffer is still used, in SRAM1).
* **D-cache disabled** in `main()` for CPU<->DMA coherence (same as disco).
* **RMII re-select**: the SYSCFG MII/RMII selection is set explicitly before
  `HAL_ETH_Init` and re-selected every 500 ms in the main loop (the ST
  workaround; run unconditionally here since the LAN is quiet).
* **Static IP, no DHCP** (`LWIP_DHCP=0`): `Netif_Config()` assigns
  192.168.5.210/24 directly. `app_ethernet.c` keeps the DHCP state machine
  compiled out.
* **32 RX buffers** (vs 10 on the disco): RX stalls permanently if the pool
  ever runs dry (`RxAllocStatus` latches), so leave burst headroom.
* lwIP + the LAN8742 component are vendored in this board's
  `../../vendor/` (copied from the disco's vendor tree).

## Build & flash

```bash
bash build.sh                 # packs web assets + cmake -G Ninja .. && ninja
ninja flash                   # probe-rs through the on-board ST-Link (SWD)
```

`build.sh` regenerates `Inc/web_assets.h` from this board's
`../../e_server/` copy (`python ../../e_server/build_web.py --out
Inc/web_assets.h`), the NUCLEO-F746ZG-branded page with the board photo
(`public/board_0.avif`).
