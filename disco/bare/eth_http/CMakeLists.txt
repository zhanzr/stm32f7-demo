# eth_http: minimal HTTP server (lwIP raw API + LAN8742 PHY + DHCP) on the
# STM32F769I-Discovery. Based on the ST LwIP_HTTP_Server_Raw example (EVAL),
# adapted for the Discovery's RMII pins + LAN8742 PHY, with the ETH DMA buffers
# in the AXI SRAM (stm32f769ni_eth.ld) because the ETH DMA cannot reach DTCM.
#
#   bash build.sh     # == cmake -G Ninja .. && ninja
#   ninja flash       # program the board (probe-rs, ST-Link V2 / SWD)

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc or armclang")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(eth_http C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32f769_board.cmake)

set(VENDOR     ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor)
set(LWIP       ${VENDOR}/LwIP)
set(COMPONENTS ${VENDOR}/Drivers/BSP/Components)
set(BSP_DIR    ${VENDOR}/Drivers/BSP/STM32F769I-Discovery)
set(FONTS      ${VENDOR}/Utilities/Fonts)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/ethernetif.c
    src/app_ethernet.c
    src/httpd_cgi_ssi.c
    src/lcd_display.c

    # on-board LCD: MIPI DSI + LTDC + DMA2D, SDRAM framebuffer, fonts
    ${BSP_DIR}/stm32f769i_discovery.c
    ${BSP_DIR}/stm32f769i_discovery_lcd.c
    ${BSP_DIR}/stm32f769i_discovery_sdram.c
    ${COMPONENTS}/otm8009a/otm8009a.c
    ${COMPONENTS}/nt35510/nt35510.c

    # lwIP (NO_SYS raw API + httpd)
    ${LWIP}/apps/http/httpd.c
    ${LWIP}/apps/http/fs.c
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
    ${COMPONENTS}/lan8742/lan8742.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_eth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_adc_ex.c

    # HAL modules for the DSI/LTDC/DMA2D LCD + SDRAM framebuffer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dsi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_ltdc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma2d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_sdram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_fmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c
)

stm32f769_apply_board(${PROJECT_NAME}.elf "-O2")

# The board's RMII Ethernet only works reliably at 200 MHz, not 216 MHz.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE BOARD_PLL_N=400)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LWIP}/include
    ${LWIP}/include/netif
    ${LWIP}/apps/http
    ${LWIP}/system
    ${COMPONENTS}
    ${BSP_DIR}
    ${COMPONENTS}/otm8009a
    ${COMPONENTS}/nt35510
    ${COMPONENTS}/adv7533
    ${FONTS}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
