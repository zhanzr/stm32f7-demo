#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <stdint.h>

void lcd_display_init(void);
void lcd_display_poll(const char *ip, const char *mac, uint32_t rx, uint32_t tx);

#endif
