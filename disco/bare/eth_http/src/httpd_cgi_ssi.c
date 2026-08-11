/**
  * @file    eth_http/src/httpd_cgi_ssi.c
  * @brief   Webserver SSI and CGI handlers (lwIP raw httpd) for the
  *          STM32F769I-Discovery. The SSI tag inserts the internal ADC
  *          channels (die temperature / VREFINT / VBAT) and the CGI controls
  *          the three on-board LEDs (LD1/LD2/LD3).
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "lwip/debug.h"
#include "lwip/tcp.h"
#include "lwip/apps/httpd.h"
#include "http_cgi_ssi.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* On-board LEDs (LD1 PJ13, LD2 PJ5, LD3 PA12 - all high active). */
#define LED_PORT_LD1  GPIOJ
#define LED_PIN_LD1   GPIO_PIN_13
#define LED_PORT_LD2  GPIOJ
#define LED_PIN_LD2   GPIO_PIN_5
#define LED_PORT_LD3  GPIOA
#define LED_PIN_LD3   GPIO_PIN_12

static uint8_t leds_ready;

static void LED_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Pin   = LED_PIN_LD1;                  /* LD1 PJ13 */
  HAL_GPIO_Init(LED_PORT_LD1, &gpio);
  gpio.Pin   = LED_PIN_LD2;                  /* LD2 PJ5 */
  HAL_GPIO_Init(LED_PORT_LD2, &gpio);
  gpio.Pin   = LED_PIN_LD3;                  /* LD3 PA12 */
  HAL_GPIO_Init(LED_PORT_LD3, &gpio);
}

static void led_all_off(void)
{
  HAL_GPIO_WritePin(LED_PORT_LD1, LED_PIN_LD1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_PORT_LD2, LED_PIN_LD2, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_PORT_LD3, LED_PIN_LD3, GPIO_PIN_RESET);
}

static void led_on(uint32_t n)
{
  switch (n)
  {
    case 1: HAL_GPIO_WritePin(LED_PORT_LD1, LED_PIN_LD1, GPIO_PIN_SET); break;
    case 2: HAL_GPIO_WritePin(LED_PORT_LD2, LED_PIN_LD2, GPIO_PIN_SET); break;
    case 3: HAL_GPIO_WritePin(LED_PORT_LD3, LED_PIN_LD3, GPIO_PIN_SET); break;
    default: break;
  }
}

/* --------------------------------------------------------------------------
 * Internal ADC channels (die temperature / VREFINT / VBAT), the same
 * measurement as the blink_hello project.
 * ------------------------------------------------------------------------*/
#define ADC_CAL_VREF_MV  3300U
#define ADC_MAX_VALUE    4095U

static ADC_HandleTypeDef hadc1;
static uint8_t adc_ready;

static void ADC_Init(void)
{
  __HAL_RCC_ADC1_CLK_ENABLE();

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV4;
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
  HAL_ADC_Init(&hadc1);
}

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
    HAL_Delay(1);   /* VBATE needs a settle time before the first sample */
  }

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
  {
    value = HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);

  return value;
}

/* SSI tags: "t" reports the three internal ADC channels, "led1..3" report the
 * current state of the three on-board LEDs (injects ` checked` when on). */
static const char *TAGS[] = { "t", "led1", "led2", "led3" };

static u16_t Status_Handler(int iIndex, char *pcInsert, int iInsertLen)
{
  if (iIndex == 0)
  {
    char str[96];
    uint32_t vrefint_raw, ts_raw, vbat_raw;
    uint16_t vrefint_cal, ts_cal1, ts_cal2;
    float vdda_mv, temp_c, vbat_mv;

    if (!adc_ready)
    {
      ADC_Init();
      adc_ready = 1;
    }

    vrefint_raw = ADC_ReadChannel(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_480CYCLES);
    ts_raw      = ADC_ReadChannel(ADC_CHANNEL_TEMPSENSOR, ADC_SAMPLETIME_480CYCLES);
    vbat_raw    = ADC_ReadChannel(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_480CYCLES);

    vrefint_cal = *((uint16_t *)0x1FF0F44AU);   /* VREFINT_CAL */
    ts_cal1     = *((uint16_t *)0x1FF0F44CU);   /* TS_CAL1, 30 degC */
    ts_cal2     = *((uint16_t *)0x1FF0F44EU);   /* TS_CAL2, 110 degC */

    vdda_mv = (float)ADC_CAL_VREF_MV * (float)vrefint_cal / (float)vrefint_raw;
    temp_c  = 30.0f + ((float)ts_raw * (float)vrefint_cal / (float)vrefint_raw - (float)ts_cal1)
              * (110.0f - 30.0f) / ((float)ts_cal2 - (float)ts_cal1);
    vbat_mv = 4.0f * (float)vbat_raw * vdda_mv / (float)ADC_MAX_VALUE;

    snprintf(str, sizeof(str),
             "VDDA: %.0f mV | Temp: %.1f C | VBAT: %.2f V\r\n",
             vdda_mv, temp_c, vbat_mv / 1000.0f);

    if ((int)strlen(str) <= iInsertLen)
    {
      memcpy(pcInsert, str, strlen(str));
      return (u16_t)strlen(str);
    }
  }
  else if (iIndex >= 1 && iIndex <= 3)
  {
    /* LED checkbox state: inject ` checked` when the LED is actually on. */
    const char chk[] = " checked";
    GPIO_TypeDef *port;
    uint16_t pin;

    switch (iIndex)
    {
      case 1: port = LED_PORT_LD1; pin = LED_PIN_LD1; break;
      case 2: port = LED_PORT_LD2; pin = LED_PIN_LD2; break;
      default: port = LED_PORT_LD3; pin = LED_PIN_LD3; break;
    }

    if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET &&
        (int)sizeof(chk) - 1 <= iInsertLen)
    {
      memcpy(pcInsert, chk, sizeof(chk) - 1);
      return (u16_t)(sizeof(chk) - 1);
    }
  }
  return 0;
}

/* CGI handler for LED control: GET /leds.cgi?led=1..3 (check = on). */
static const char *LEDS_CGI_Handler(int iIndex, int iNumParams,
                                    char *pcParam[], char *pcValue[]);

static const tCGI LEDS_CGI = {"/leds.cgi", LEDS_CGI_Handler};
static tCGI CGI_TAB[1];

static const char *LEDS_CGI_Handler(int iIndex, int iNumParams,
                                    char *pcParam[], char *pcValue[])
{
  uint32_t i;

  if (iIndex == 0)
  {
    if (!leds_ready)
    {
      LED_Init();
      leds_ready = 1;
    }
    led_all_off();

    for (i = 0; i < iNumParams; i++)
    {
      if (strcmp(pcParam[i], "led") == 0)
      {
        led_on((uint32_t)strtoul(pcValue[i], NULL, 10));
      }
    }
  }
  return "/led.shtml";
}

/**
  * @brief  Http webserver init: start httpd + register SSI/CGI handlers.
  */
void http_server_init(void)
{
  /* Bring the LEDs up front so the SSI handler can read their real state
   * (all off to begin with). */
  LED_Init();
  led_all_off();
  leds_ready = 1;

  httpd_init();
  http_set_ssi_handler(Status_Handler, TAGS, 4);
  CGI_TAB[0] = LEDS_CGI;
  http_set_cgi_handlers(CGI_TAB, 1);
}
