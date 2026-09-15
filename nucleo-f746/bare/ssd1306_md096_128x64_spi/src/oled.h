#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

/* SSD1306 0.96" 128x64 mono OLED (MD096) on SPI1 - pin map follows the
 * nucleo144-f746zg repo's ssd1306_oled096_spi branch:
 *   PA5 SPI1_SCK, PA6 SPI1_MISO, PA7 SPI1_MOSI (AF5)
 *   PD14 CS (soft), PD15 DC (cmd/data), PF12 RES, PF13 BL (driven high)
 */

#define OLED_CMD 0  /* command byte  */
#define OLED_DATA 1 /* data byte     */

#define Max_Column 128
#define Max_Row 64

/* transport init (SPI1 + control GPIOs) and driver */
void OLED_SPI_Init(void);
void OLED_Init(void);

void OLED_WR_Byte(uint8_t dat, uint8_t cmd);

/* whole-window primitives (batched SPI page writes) */
void OLED_Fill(unsigned char data);
void OLED_Clear(void);
void OLED_WritePage(unsigned char page, const unsigned char *data);
void OLED_Invert(unsigned char on);
void OLED_Display_On(void);
void OLED_Display_Off(void);

/* text (F8X16 8x16 or F6x8 6x8) */
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *chr, uint8_t Char_Size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len,
                  uint8_t size2);

void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1,
                  unsigned char y1, const unsigned char BMP[]);

/* full-screen mono test pattern sweep (banner -> info -> patterns -> fps) */
void OLED_Demo(void);

#endif /* __OLED_H */
