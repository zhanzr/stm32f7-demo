/*
 * SSD1306 0.96" 128x64 mono OLED (MD096 module) over SPI1, for the
 * NUCLEO-F746ZG board.
 *
 * Driver API and display patterns follow the f425-start repo's
 * bare/ssd1306_md096_128x64_iic port (MD096_* there, OLED_* here); the
 * hardware wiring and SPI transport follow the nucleo144-f746zg repo's
 * ssd1306_oled096_spi branch (PA5/PA6/PA7 + CS PD14, DC PD15, RES PF12,
 * BL PF13).
 *
 * Compared to the original branch driver this version:
 *   - fixes the Set_Pos low-nibble bug (original ORed 0x01 into the low
 *     nibble, shifting every column by one),
 *   - batches full 128-byte pages into one SPI transmit (8 transfers per
 *     full screen instead of 1024 single-byte ones).
 */

#include <string.h>
#include <stdio.h>

#include "stm32f7xx_hal.h"
#include "board.h"
#include "oled.h"

/* ---- pins: nucleo144-f746zg ssd1306_oled096_spi branch ---- */
#define OLED_CS_PORT GPIOD
#define OLED_CS_PIN GPIO_PIN_14
#define OLED_DC_PORT GPIOD
#define OLED_DC_PIN GPIO_PIN_15
#define OLED_RES_PORT GPIOF
#define OLED_RES_PIN GPIO_PIN_12
#define OLED_BL_PORT GPIOF
#define OLED_BL_PIN GPIO_PIN_13

extern const unsigned char F6x8[][6];
extern const unsigned char F8X16[];

static SPI_HandleTypeDef hspi1;
static SPI_HandleTypeDef *spi = &hspi1;

static uint32_t oled_pow(uint8_t m, uint8_t n);

/* ------------------------------------------------------------------------ */
static void OLED_Res_Set(void)
{
    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_SET);
}

static void OLED_Transmit(const uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(spi, (uint8_t *)buf, len, 1000);
}

/* command/data byte (DC low = command, high = data; CS frames the transfer) */
void OLED_WR_Byte(uint8_t dat, uint8_t cmd)
{
    uint8_t b = dat;

    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN,
                      cmd ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);
    OLED_Transmit(&b, 1);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
}

/* ------------------------------------------------------------------------ */
void OLED_Set_Pos(unsigned char x, unsigned char y)
{
    /* standard SSD1306 page-addressing mode:
     *   page  = 0xB0 + page number (0..7)
     *   col   = 0x10 | high nibble, then low nibble
     * (the original Keil-branch driver used (x&0x0f)|0x01, which shifted
     *  all content one column) */
    OLED_WR_Byte(0xb0 + y, OLED_CMD);
    OLED_WR_Byte(((x >> 4) & 0x0f) | 0x10, OLED_CMD);
    OLED_WR_Byte(x & 0x0f, OLED_CMD);
}

/*
 * Batched write of one full 128-byte page row in a single SPI transmit
 * (DC held high) - about 8 transactions per full screen, versus 1024
 * one-byte transactions of the per-byte path.
 */
void OLED_WritePage(unsigned char page, const unsigned char *data)
{
    OLED_Set_Pos(0, page);
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_RESET);
    OLED_Transmit(data, 128);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
}

/* fill the whole 128x64 window with one RAM byte pattern */
void OLED_Fill(unsigned char data)
{
    unsigned char row[128];
    unsigned char page;

    memset(row, data, sizeof(row));
    for (page = 0; page < 8; page++) {
        OLED_WritePage(page, row);
    }
}

void OLED_Clear(void)
{
    OLED_Fill(0x00);
}

/* whole-display inversion (mono "negative") */
void OLED_Invert(unsigned char on)
{
    OLED_WR_Byte(on ? 0xA7 : 0xA6, OLED_CMD);
}

void OLED_Display_On(void)
{
    OLED_WR_Byte(0X8D, OLED_CMD); /* SET DCDC  */
    OLED_WR_Byte(0X14, OLED_CMD); /* DCDC ON   */
    OLED_WR_Byte(0XAF, OLED_CMD); /* DISPLAY ON */
}

void OLED_Display_Off(void)
{
    OLED_WR_Byte(0X8D, OLED_CMD); /* SET DCDC  */
    OLED_WR_Byte(0X10, OLED_CMD); /* DCDC OFF  */
    OLED_WR_Byte(0XAE, OLED_CMD); /* DISPLAY OFF */
}

/* display one char at the specified position (x: 0-127, y: page 0-7)
 * x:0~127
 * y:0~63
 * size: font 16 (8x16) / 12 (6x8) */
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size)
{
    unsigned char c = 0, i = 0;

    c = chr - ' ';
    if (x > Max_Column - 1) {
        x = 0;
        y = y + 2;
    }
    if (Char_Size == 16) {
        OLED_Set_Pos(x, y);
        for (i = 0; i < 8; i++)
            OLED_WR_Byte(F8X16[c * 16 + i], OLED_DATA);
        OLED_Set_Pos(x, y + 1);
        for (i = 0; i < 8; i++)
            OLED_WR_Byte(F8X16[c * 16 + i + 8], OLED_DATA);
    } else {
        OLED_Set_Pos(x, y);
        for (i = 0; i < 6; i++)
            OLED_WR_Byte(F6x8[c][i], OLED_DATA);
    }
}

/* display a number
 * x,y : start position
 * len : number of digits
 * size: font 16 (8x16) / 12 (6x8)
 * num : value (0~4294967295) */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len,
                  uint8_t size2)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++) {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                OLED_ShowChar(x + (size2 / 2) * t, y, ' ', size2);
                continue;
            } else
                enshow = 1;
        }
        OLED_ShowChar(x + (size2 / 2) * t, y, temp + '0', size2);
    }
}

/* display a string */
void OLED_ShowString(uint8_t x, uint8_t y, const char *chr, uint8_t Char_Size)
{
    unsigned char j = 0;

    while (chr[j] != '\0') {
        OLED_ShowChar(x, y, chr[j], Char_Size);
        x += 8;
        if (x > 120) {
            x = 0;
            y += 2;
        }
        j++;
    }
}

void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1,
                  unsigned char y1, const unsigned char BMP[])
{
    unsigned int j = 0;
    unsigned char x, y;

    for (y = y0; y < y1; y++) {
        OLED_Set_Pos(x0, y);
        for (x = x0; x < x1; x++) {
            OLED_WR_Byte(BMP[j++], OLED_DATA);
        }
    }
}

/* ------------------------------------------------------------------------ */
void OLED_SPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* SPI1 pins: PA5 SCK, PA6 MISO, PA7 MOSI (AF5), as in the source repo */
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* control pins: CS PD14, DC PD15, RES PF12, BL PF13 (output PP, low) */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    GPIO_InitStruct.Pin = OLED_CS_PIN | OLED_DC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(OLED_CS_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = OLED_RES_PIN | OLED_BL_PIN;
    HAL_GPIO_Init(OLED_RES_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(OLED_CS_PORT, OLED_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_DC_PORT, OLED_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_BL_PORT, OLED_BL_PIN, GPIO_PIN_SET);

    /* SPI1 master, mode 0, 8-bit MSB, soft NSS, prescaler 32
     * (APB2 108 MHz / 32 = 3.375 MHz), as in the source repo */
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLED;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLED;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

void OLED_Init(void)
{
    OLED_SPI_Init();

    /* hardware reset pulse (RES PF12) */
    OLED_Res_Set();
    HAL_Delay(50);
    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    OLED_Res_Set();
    HAL_Delay(50);

    OLED_WR_Byte(0xAE, OLED_CMD); /*--turn off oled panel*/
    OLED_WR_Byte(0x00, OLED_CMD); /*---set low column address 00 */
    OLED_WR_Byte(0x12, OLED_CMD); /*---set high column address 12 */
    OLED_WR_Byte(0x40, OLED_CMD); /*--set start line address (0x00~0x3F) */
    OLED_WR_Byte(0x81, OLED_CMD); /*--set contrast control register */
    OLED_WR_Byte(0xCF, OLED_CMD); /* Set SEG Output Current Brightness */
    OLED_WR_Byte(0xA1, OLED_CMD); /*--Set SEG/Column Mapping: 0xa0 normal,
                                    0xa1 mirrored (A1 used) */
    OLED_WR_Byte(0xC8, OLED_CMD); /* Set COM/Row Scan Direction: 0xc0 normal,
                                    0xc8 mirrored */
    OLED_WR_Byte(0xA6, OLED_CMD); /*--set normal display */
    OLED_WR_Byte(0xA8, OLED_CMD); /*--set multiplex ratio(1 to 64) */
    OLED_WR_Byte(0x3f, OLED_CMD); /*--1/64 duty */
    OLED_WR_Byte(0xD3, OLED_CMD); /*-set display offset */
    OLED_WR_Byte(0x00, OLED_CMD); /*-not offset */
    OLED_WR_Byte(0xd5, OLED_CMD); /*--set display clock divide/oscillator */
    OLED_WR_Byte(0x80, OLED_CMD); /*--set divide ratio, 100 Frames/Sec */
    OLED_WR_Byte(0xD9, OLED_CMD); /*--set pre-charge period */
    OLED_WR_Byte(0xF1, OLED_CMD); /* Pre-Charge 15 Clocks, Discharge 1 */
    OLED_WR_Byte(0xDA, OLED_CMD); /*--set com pins hardware configuration */
    OLED_WR_Byte(0x12, OLED_CMD);
    OLED_WR_Byte(0xDB, OLED_CMD); /*--set vcomh */
    OLED_WR_Byte(0x40, OLED_CMD); /* Set VCOM Deselect Level */
    OLED_WR_Byte(0x20, OLED_CMD); /*-Set Page Addressing Mode */
    OLED_WR_Byte(0x02, OLED_CMD);
    OLED_WR_Byte(0x8D, OLED_CMD); /*--set Charge Pump enable/disable */
    OLED_WR_Byte(0x14, OLED_CMD); /*--set(0x10) disable */
    OLED_WR_Byte(0xA4, OLED_CMD); /* Disable Entire Display On */
    OLED_WR_Byte(0xA6, OLED_CMD); /* Disable Inverse Display On */
    OLED_WR_Byte(0xAF, OLED_CMD); /*--turn on oled panel */

    OLED_Clear();
    OLED_Set_Pos(0, 0);
}

static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)
        result *= m;
    return result;
}

/*
 * Full-screen mono test pattern set, following the
 * f425-start bare/ssd1306_md096_128x64_iic demo (itself modeled after the
 * st7735_md144_128x128 demo of the f4-demo STM32 repo): info page ->
 * inverted page -> full-window pattern sweep -> animated FPS pass.
 *
 *   banner -> info page -> inverted info page -> all-on -> all-off ->
 *   8-px horizontal stripes -> 4-px horizontal stripes -> 1-px checkerboard
 *   -> 16-px vertical bars -> border rectangle -> page-gradient ->
 *   animated bar sweep with console FPS report
 */
void OLED_Demo(void)
{
    unsigned char row[128];
    unsigned char page, x;
    uint32_t t0, frames;
    char line[24];

    /* --- banner + info pages (8x16 font, 16 px/row, 4 rows of 16 chars) --- */
    OLED_Clear();
    OLED_ShowString(0, 0, "  SSD1306 MD096", 16);
    OLED_ShowString(0, 2, "  128x64 mono", 16);
    OLED_ShowString(0, 4, "  STM32F746ZG", 16);
    OLED_ShowString(0, 6, "  216MHz SPI1", 16);
    HAL_Delay(1000);

    /* --- same info page, whole-display inverted --- */
    OLED_Invert(1);
    HAL_Delay(1000);
    OLED_Invert(0);
    HAL_Delay(500);

    /* --- all pixels on / off --- */
    OLED_Fill(0xFF);
    HAL_Delay(1000);
    OLED_Fill(0x00);
    HAL_Delay(1000);

    /* --- 8-px horizontal stripes --- */
    for (page = 0; page < 8; page++) {
        memset(row, (page & 1) ? 0x00 : 0xFF, sizeof(row));
        OLED_WritePage(page, row);
    }
    HAL_Delay(1000);

    /* --- 4-px horizontal stripes --- */
    for (page = 0; page < 8; page++) {
        memset(row, (page & 1) ? 0xF0 : 0x0F, sizeof(row));
        OLED_WritePage(page, row);
    }
    HAL_Delay(1000);

    /* --- 1-px checkerboard --- */
    for (x = 0; x < 128; x++) {
        row[x] = (x & 1) ? 0xAA : 0x55;
    }
    for (page = 0; page < 8; page++) {
        OLED_WritePage(page, row);
    }
    HAL_Delay(1000);

    /* --- 16-px vertical bars --- */
    for (x = 0; x < 128; x++) {
        row[x] = ((x / 16) & 1) ? 0x00 : 0xFF;
    }
    for (page = 0; page < 8; page++) {
        OLED_WritePage(page, row);
    }
    HAL_Delay(1000);

    /* --- border rectangle (1-px frame) --- */
    for (page = 0; page < 8; page++) {
        memset(row, 0x00, sizeof(row));
        if (page == 0 || page == 7) {
            memset(row, 0xFF, sizeof(row));
        } else {
            row[0] = 0xFF;
            row[127] = 0xFF;
        }
        OLED_WritePage(page, row);
    }
    HAL_Delay(1000);

    /* --- page gradient (fake grey ramp on mono via row bits) --- */
    {
        static const unsigned char ramp[8] = {0x01, 0x07, 0x1F, 0x7F,
                                              0xFF, 0x7F, 0x1F, 0x07};
        for (page = 0; page < 8; page++) {
            memset(row, ramp[page], sizeof(row));
            OLED_WritePage(page, row);
        }
    }
    HAL_Delay(1000);

    /* --- animated bar sweep with FPS on the console --- */
    OLED_Clear();
    t0 = HAL_GetTick();
    for (frames = 0; frames < 40; frames++) {
        unsigned char start = (frames * 8) % 128;

        memset(row, 0x00, sizeof(row));
        memset(&row[start], 0xFF, 16 <= 128 - start ? 16 : 128 - start);
        for (page = 0; page < 8; page++) {
            OLED_WritePage(page, row);
        }
    }
    sprintf(line, "fps %lu",
            (unsigned long)(frames * 1000 / (HAL_GetTick() - t0)));
    OLED_ShowString(0, 3, line, 16);
    printf("oled fps: %s\r\n", line);
    HAL_Delay(1000);
}
