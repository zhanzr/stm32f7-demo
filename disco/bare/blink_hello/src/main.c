#include <stdio.h>

#include "board.h"
#include "uart_printf.h"

/* On-board LEDs: LD1 PJ13, LD2 PJ5, LD3 PA12, all HIGH active (SET = ON).
 * See the STM32F769I-Discovery user manual / schematic (UM2033, MB1166). */
#define LED1_PORT GPIOJ
#define LED1_PIN  GPIO_PIN_13
#define LED2_PORT GPIOJ
#define LED2_PIN  GPIO_PIN_5
#define LED3_PORT GPIOA
#define LED3_PIN  GPIO_PIN_12

/* ------------------------------------------------------------------------ */
/* ADC internal channels (see RM0410: ADC1_IN17 = VREFINT, ADC1_IN18 =
 * temperature sensor / VBAT):
 *  - VREFINT : internal bandgap reference (~1.2 V nominal)
 *  - Temp    : die temperature sensor (needs the TSVREFE enable)
 *  - VBAT    : battery pin, fed through an internal /4 divider
 * Factory calibration (stm32f769xx.h, acquired at Vref+ = 3.3 V):
 *  - VREFINT_CAL @ 0x1FF0F44A
 *  - TS_CAL1 @ 0x1FF0F44C  (30  掳C)
 *  - TS_CAL2 @ 0x1FF0F44E  (110 掳C)
 */
#define ADC_CAL_VREF_MV   3300U
#define ADC_MAX_VALUE     4095U

static ADC_HandleTypeDef hadc1;

static void ADC_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV4;  /* PCLK2 108/4 = 27 MHz */
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion   = 0;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T1_CC1;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* Configure one internal channel on rank 1 and take a single conversion.
 * VBAT and temperature share ADC1_IN18 (mutually exclusive mux), so the
 * channels are configured one at a time. */
static uint32_t ADC_ReadChannel(uint32_t channel, uint32_t sampling_time)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t value = 0;

    sConfig.Channel      = channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = sampling_time;
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    if (channel == ADC_CHANNEL_VBAT)
    {
        /* VBAT enable (VBATE) needs a settle time before the first sample. */
        HAL_Delay(1);
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        value = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return value;
}

static void ADC_Print(void)
{
    uint32_t vrefint_raw = ADC_ReadChannel(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_480CYCLES);
    uint32_t ts_raw      = ADC_ReadChannel(ADC_CHANNEL_TEMPSENSOR, ADC_SAMPLETIME_480CYCLES);
    uint32_t vbat_raw    = ADC_ReadChannel(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_480CYCLES);

    uint16_t vrefint_cal = *((uint16_t *)0x1FF0F44AU);   /* VREFINT_CAL */
    uint16_t ts_cal1     = *((uint16_t *)0x1FF0F44CU);   /* TS_CAL1, 30  掳C */
    uint16_t ts_cal2     = *((uint16_t *)0x1FF0F44EU);   /* TS_CAL2, 110 掳C */

    /* Actual VDDA inferred from the VREFINT reading (factory cal @ 3300 mV). */
    float vdda_mv = (float)ADC_CAL_VREF_MV * (float)vrefint_cal / (float)vrefint_raw;

    /* Normalize the temp-sensor code to the 3300 mV calibration reference,
     * then two-point interpolate between TS_CAL1/TS_CAL2. */
    float ts_norm = (float)ts_raw * (float)vrefint_cal / (float)vrefint_raw;
    float temp_c  = 30.0f + (ts_norm - (float)ts_cal1)
                    * (110.0f - 30.0f) / ((float)ts_cal2 - (float)ts_cal1);

    /* VBAT is divided by 4 inside the chip, so scale the channel reading by 4. */
    float vbat_mv = 4.0f * (float)vbat_raw * vdda_mv / (float)ADC_MAX_VALUE;

    printf("ADC: VREFINT raw=%lu cal=%u -> VDDA=%.0f mV | Temp=%.1f C | VBAT=%.2f V @ %lu Hz\r\n",
           (unsigned long)vrefint_raw, (unsigned)vrefint_cal, vdda_mv,
           temp_c, vbat_mv / 1000.0f, (unsigned long)SystemCoreClock);
}

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

int main(void)
{
    HAL_Init();
    Board_Init();
    LED_Init();
    ADC_Init();

    printf("\r\n=== blink_hello on STM32F769NI @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    while (1)
    {
        LED_ToggleAll();
        ADC_Print();
        HAL_Delay(1000);
    }

    return 0;
}
