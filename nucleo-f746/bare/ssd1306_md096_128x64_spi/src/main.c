/*
 * SSD1306 0.96" 128x64 mono OLED (MD096) demo over SPI1, NUCLEO-F746ZG.
 *
 * Integrates the nucleo144-f746zg repo's ssd1306_oled096_spi branch:
 * same hardware connection (PA5/PA6/PA7 SPI1 + CS PD14, DC PD15,
 * RES PF12, BL PF13) and SSD1306 page-addressing command flow. Display
 * patterns follow the f425-start repo's bare/ssd1306_md096_128x64_iic
 * demo (see oled.c OLED_Demo).
 *
 * Console: USART3 (PD8/PD9, ST-Link VCP) @ 115200 - prints a banner, the
 * board freq, and the OLED fps after the animated pass.
 */

#include <stdio.h>

#include "board.h"
#include "uart_printf.h"

#include "oled.h"

/* On-board LEDs: LD1 PB0, LD2 PB7, LD3 PB14, all HIGH active (SET = ON) -
 * same wiring as the nucleo-f722 / nucleo-f746 board layer. */
#define LED1_PORT GPIOB
#define LED1_PIN  GPIO_PIN_0
#define LED2_PORT GPIOB
#define LED2_PIN  GPIO_PIN_7
#define LED3_PORT GPIOB
#define LED3_PIN  GPIO_PIN_14

static void LED_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

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

int main(void)
{
    HAL_Init();
    Board_Init();
    LED_Init();

    printf("\r\n=== ssd1306_md096_128x64_spi on STM32F746ZG @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    OLED_Init();

    while (1)
    {
        OLED_Demo();
        LED_ToggleAll();
        printf("demo pass done @ %lu ms\r\n", (unsigned long)HAL_GetTick());
        HAL_Delay(500);
    }

    return 0;
}
