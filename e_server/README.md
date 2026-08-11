# e_server - embedded web demo (frontend + reference C backend)

A minimal single-page web app for the STM32F769I-Discovery, plus a host-side
C backend that mirrors the API the board's embedded server will implement.
The web assets are bundled into C arrays by a build script (inline CSS/JS,
gzip the page, embed the images raw) so they can be served straight from
flash on the MCU.

## Layout

```
e_server/
  web/             raw frontend sources
    index.html     single page, three tabs
    style.css
    app.js
  public/          images served to the page (embedded into web_assets.h)
    board_0_little.jpg
    board_1_little.jpg
    board_2_little.jpg
  build_web.py     bundle: inline CSS/JS -> gzip page -> web_assets.h
  web_assets.h     generated C arrays (page + images + lookup table)
  server.c         reference C backend (host-side)
  Makefile / build.sh
```

## Frontend (three tabs, no external libraries)

1. **LED control** - three checkboxes reflecting the board LEDs. Each click
   POSTs the new state to the backend (no page reload) and the checkboxes
   re-sync to the server's reply. Rapid clicks are coalesced.
2. **ADC values** - three canvas plots (VREFINT / die temperature / VBAT).
   A dropdown picks the sample interval: 1 s (default), 2 s, 4 s; the choice
   is persisted in `localStorage`. Samples are polled only while the tab is
   open; on timeout/error the previous value is kept and the series continues.
3. **Board info** - architecture, LAN IP, public IP, geo location and weather
   (each falls back to "N/A" if unavailable), a manual *Refresh* button, and
   the board photos from `public/`.

## API (shared contract with the embedded server)

| Route                | Description                                     |
| -------------------- | ----------------------------------------------- |
| `GET /`              | the page: gzipped, CSS/JS inlined (`Content-Encoding: gzip`) |
| `GET /api/leds`      | `{"leds":[0,1,0]}`                              |
| `POST /api/leds`     | body `{"leds":[0,1,0]}` -> applied, `{"leds":[...]}` |
| `GET /api/adc`       | `{"vrefint_mv":3292.6,"temp_c":43.6,"vbat_mv":3.32,"ts":...}` |
| `GET /api/info`      | `{"arch","lan_ip","public_ip","geo","weather","ts"}` (nulls = unavailable) |
| `GET /public/*`      | raw image bytes (`image/jpeg`) from `embedded_files[]` |

On the host the reference backend reads real ADC-style values from a
simulation and fetches the public IP / geo / weather over plain HTTP
(api.ipify.org, ip-api.com, wttr.in) - best-effort, so offline they come back
as `null` and the page shows "N/A". On the board those fields would come from
an HTTP client (+ TLS for HTTPS upstreams).

## Build & run (host reference)

Needs Python 3 and a host C compiler (gcc/clang). Windows/MinGW adds
`-lws2_32` automatically.

```bash
python build_web.py      # or: make web_assets.h
make                     # or: bash build.sh
./e_server 8080          # or: make run
```

Then open `http://localhost:8080/`.

## Bundling (build_web.py)

```text
[ index.html ] ──┬── inline <style> + <script> ──► index.html ── gzip ──► index_html_gz[]
[ style.css  ] ──┘                                          (raw)        │
[ app.js     ] ──┘                                                          ▼
[ public/*.jpg ] ─────────────────────────► raw byte arrays + embedded_files[]  web_assets.h
```

* The page is served gzipped (browser decompresses via `Content-Encoding: gzip`).
* JPEG/PNG are already compressed, so the images are embedded **raw** with a
  `path -> {ctype, data, len}` lookup table the server routes against.
* Regenerate after any change to `web/` or `public/`; commit `web_assets.h`
  alongside the sources.

## Porting to the board

The `web_assets.h` arrays drop straight into flash; `server.c`'s routing is
the blueprint for the embedded server (currently `eth_http` in `disco/` serves
an older, hand-built fsdata page). The board already has the ADC channels
(VREFINT / temperature / VBAT) and the three LEDs, so `/api/leds` and
`/api/adc` map 1:1; `/api/info` needs an HTTP client for the external fields.
