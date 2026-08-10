/**
  * @file    main.c
  * @brief   disco_boot - minimal bootloader in internal flash for the disco
  *          (STM32F769I-Discovery).
  *
  * Boots from internal flash, brings up the 216 MHz clock tree and console,
  * initializes the MX25L51245G via the QUADSPI (vendor BSP), then performs a
  * *basic* bootability check on a stage-2 firmware at the QSPI memory-mapped
  * base 0x90000000:
  *   - the initial stack pointer (word 0) must land in SRAM, and
  *   - the reset vector (word 1) must be a thumb pointer inside the QSPI
  *     window.
  * If the check passes it enters QUADSPI memory-mapped mode and jumps. If it
  * fails it loops: toggles the three LEDs and prints the check result every
  * second, so a missing/bad firmware is visible on both the LEDs and the
  * console without a debugger.
  */

#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32f769i_discovery_qspi.h"

#define APP_BASE  0x90000000UL
#define APP_LIMIT 0x90800000UL     /* 8 MB QSPI window */

/* On-board LEDs (LD1 PJ13, LD2 PJ5, LD3 PA12), high active. */
#define LED1_PORT GPIOJ
#define LED1_PIN  GPIO_PIN_13
#define LED2_PORT GPIOJ
#define LED2_PIN  GPIO_PIN_5
#define LED3_PORT GPIOA
#define LED3_PIN  GPIO_PIN_12

typedef void (*pfnVoid)(void);

/* ------------------------------------------------------------------------ */
static void LED_Init(void)
{
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LED1_PIN;
    HAL_GPIO_Init(LED1_PORT, &gpio);
    gpio.Pin = LED2_PIN;
    HAL_GPIO_Init(LED2_PORT, &gpio);
    gpio.Pin = LED3_PIN;
    HAL_GPIO_Init(LED3_PORT, &gpio);

    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
}

static void LED_ToggleAll(void)
{
    HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
    HAL_GPIO_TogglePin(LED2_PORT, LED2_PIN);
    HAL_GPIO_TogglePin(LED3_PORT, LED3_PIN);
}

/* MPU: the shared board layer denies 0x90000000 (region 0 subregion 4). Make
 * the QSPI memory-mapped space cacheable + executable (the stage-2 app runs
 * from there), and the QUADSPI peripheral at 0xA0001000 a device region. */
static void mpu_qspi_config(void)
{
    HAL_MPU_Disable();
    SCB_InvalidateDCache();
    SCB_InvalidateICache();

    MPU_Region_InitTypeDef m = {0};

    /* QSPI memory-mapped space: 256 MB @ 0x90000000, Normal write-back. */
    m.Enable           = MPU_REGION_ENABLE;
    m.Number           = MPU_REGION_NUMBER0;
    m.BaseAddress      = 0x90000000u;
    m.Size             = MPU_REGION_SIZE_256MB;
    m.SubRegionDisable = 0;
    m.TypeExtField     = MPU_TEX_LEVEL1;
    m.AccessPermission = MPU_REGION_FULL_ACCESS;
    m.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    m.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    m.IsCacheable      = MPU_ACCESS_CACHEABLE;
    m.IsBufferable     = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&m);

    /* QUADSPI peripheral (0xA0001000) as device memory. */
    m.Number           = MPU_REGION_NUMBER2;
    m.BaseAddress      = 0xA0000000u;
    m.Size             = MPU_REGION_SIZE_8KB;
    m.TypeExtField     = MPU_TEX_LEVEL0;
    m.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    m.IsShareable      = MPU_ACCESS_SHAREABLE;
    m.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    m.IsBufferable     = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&m);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    SCB_InvalidateDCache();
    SCB_InvalidateICache();
}

/* ------------------------------------------------------------------------ */
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Basic bootability check on the firmware at the QSPI base. */
static int app_bootable(uint32_t *out_sp, uint32_t *out_rv)
{
    uint8_t hdr[8];
    *out_sp = 0;
    *out_rv = 0;

    if (BSP_QSPI_Read(hdr, 0u, 8u) != QSPI_OK)
    {
        return 0;
    }

    uint32_t sp = rd32(&hdr[0]);
    uint32_t rv = rd32(&hdr[4]);
    *out_sp = sp;
    *out_rv = rv;

    /* Initial SP in SRAM (0x20000000..0x20080000), reset vector a thumb
     * pointer in the QSPI window. */
    return (sp >= 0x20000000u && sp < 0x20080000u) &&
           (rv & 1u) && (rv & ~1u) >= APP_BASE && (rv & ~1u) < APP_LIMIT;
}

/* ------------------------------------------------------------------------ */
static void jump_to_app(void)
{
    uint32_t sp    = *(volatile uint32_t *)(APP_BASE);
    uint32_t reset = *(volatile uint32_t *)(APP_BASE + 4u);

    __disable_irq();
    SCB->VTOR = APP_BASE;               /* relocate vector table to QSPI */
    __set_MSP(sp);
    ((pfnVoid)reset)();                 /* app reset handler (thumb)     */
    while (1) { }                       /* never reached                 */
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    SCB_EnableICache();
    SCB_EnableDCache();
    SystemClock_Config();
    UART_Init();
    LED_Init();
    mpu_qspi_config();

    printf("\r\n=== disco_boot bootloader @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    int rc = BSP_QSPI_Init();
    printf("QSPI init rc=%d (MX25L51245G)\r\n", rc);

    uint32_t sp = 0, rv = 0;
    int ok = (rc == QSPI_OK) && app_bootable(&sp, &rv);

    printf("QSPI firmware check: %s (SP=0x%08lx, Reset=0x%08lx)\r\n",
           ok ? "PASS - booting" : "FAIL", (unsigned long)sp, (unsigned long)rv);

    if (ok)
    {
        rc = BSP_QSPI_EnableMemoryMappedMode();
        if (rc != QSPI_OK)
        {
            printf("memmap start FAIL rc=%d\r\n", rc);
            ok = 0;
        }
    }

    if (ok)
    {
        printf("jumping to 0x%08lx ...\r\n", (unsigned long)APP_BASE);
        jump_to_app();
        return 0;
    }

    /* No bootable firmware: loop, toggle the LEDs, print the result. */
    printf("no bootable firmware on QSPI flash - waiting (LEDs blink)\r\n");
    while (1)
    {
        LED_ToggleAll();
        printf("boot FAIL: SP=0x%08lx Reset=0x%08lx (need SP in SRAM + thumb "
               "reset in 0x%08lx..0x%08lx)\r\n",
               (unsigned long)sp, (unsigned long)rv,
               (unsigned long)APP_BASE, (unsigned long)APP_LIMIT);
        printf("  NOTE: it is only a basic check. The bootloader itself is a "
               "basic demo. If necessary, modify the bootloader or the "
               "application's linker script.\r\n");
        HAL_Delay(3000);
    }
}
