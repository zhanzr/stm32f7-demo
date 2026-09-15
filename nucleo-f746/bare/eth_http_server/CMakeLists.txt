# eth_http_server: minimal HTTP server (lwIP raw API + LAN8742 PHY, static
# IP 192.168.5.210) on the NUCLEO-F746ZG. Follows the disco-f769 eth_http
# port of the ST LwIP_HTTP_Server_Raw example: same RMII pins + LAN8742A PHY,
# ETH DMA buffers in .bss (SRAM1 - DMA-reachable, unlike DTCM), console
# status instead of the disco's LCD page. Runs at the Nucleo's native
# 216 MHz (ST's NUCLEO-F746ZG LwIP example clock; the disco-f769 needed 200 MHz).
#
#   bash build.sh     # == pack web assets + cmake -G Ninja .. && ninja
#   ninja flash       # program the board (probe-rs, ST-Link / SWD)

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc or armclang")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(eth_http_server C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f746_board.cmake)

set(VENDOR     ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor)
set(LWIP       ${VENDOR}/LwIP)
set(COMPONENTS ${VENDOR}/Drivers/BSP/Components)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/ethernetif.c
    src/app_ethernet.c
    src/http_server.c

    # lwIP (NO_SYS raw API; the HTTP server is a custom raw-TCP app,
    # src/http_server.c, serving the bundled web_assets.h site)
    ${LWIP}/core/def.c
    ${LWIP}/core/dns.c
    ${LWIP}/core/inet_chksum.c
    ${LWIP}/core/init.c
    ${LWIP}/core/ip.c
    ${LWIP}/core/ipv4/ip4.c
    ${LWIP}/core/ipv4/ip4_addr.c
    ${LWIP}/core/ipv4/ip4_frag.c
    ${LWIP}/core/mem.c
    ${LWIP}/core/memp.c
    ${LWIP}/core/netif.c
    ${LWIP}/core/pbuf.c
    ${LWIP}/core/raw.c
    ${LWIP}/core/stats.c
    ${LWIP}/core/sys.c
    ${LWIP}/core/tcp.c
    ${LWIP}/core/tcp_in.c
    ${LWIP}/core/tcp_out.c
    ${LWIP}/core/timeouts.c
    ${LWIP}/core/udp.c
    ${LWIP}/netif/ethernet.c
    ${LWIP}/core/ipv4/etharp.c
    ${LWIP}/core/ipv4/dhcp.c
    ${LWIP}/core/ipv4/icmp.c

    # LAN8742 PHY + HAL Ethernet + internal-channel ADC
    # (F7_HAL points at the repo-root vendored drivers/, set by the board include)
    ${COMPONENTS}/lan8742/lan8742.c
    ${F7_HAL}/Src/stm32f7xx_hal_eth.c
    ${F7_HAL}/Src/stm32f7xx_hal_adc.c
    ${F7_HAL}/Src/stm32f7xx_hal_adc_ex.c
    ${F7_HAL}/Src/stm32f7xx_hal_dma.c
)

stm32f746_apply_board(${PROJECT_NAME}.elf "-O2")

# The Nucleo-144 RMII runs on its native 216 MHz clock (ST NUCLEO-F746ZG
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE BOARD_PLL_N=432)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LWIP}/include
    ${LWIP}/include/netif
    ${LWIP}/system
    ${COMPONENTS}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
