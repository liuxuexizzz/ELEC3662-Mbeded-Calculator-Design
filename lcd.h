#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/*
 * ELEC/XJEL3662 Mini-Project LCD interface (HD44780 compatible) - 4-bit mode
 * Appendix C pin assignment:
 *   - PORTB -> LCD DB7..DB4   (PB7..PB4)
 *   - PA2   -> EN
 *   - PA3   -> RS
 *   - R/W   -> GND (write-only)
 */

void LCD_Init(void);
void LCD_Command(uint8_t cmd);
void LCD_WriteChar(char c);
void LCD_WriteString(const char *s);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);  // 16x2: row 0-1, col 0-15

#endif
