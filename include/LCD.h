#ifndef LCD_H_
#define LCD_H_

#include <avr/io.h>
#include <util/delay.h>
#include "stdio.h"

// LCD-to-74HC595 mapping
#define LCD_D4  0x02  // Q1
#define LCD_D5  0x04  // Q2
#define LCD_D6  0x08  // Q3
#define LCD_D7  0x10  // Q4
#define LCD_RS  0x20  // Q5
#define LCD_EN  0x40  // Q6

// LCD command set
#define LCD_CMD_CLEAR_DISPLAY         0x01
#define LCD_CMD_CURSOR_HOME           0x02
#define LCD_CMD_DISPLAY_OFF           0x08
#define LCD_CMD_DISPLAY_NO_CURSOR     0x0C
#define LCD_CMD_DISPLAY_CURSOR_NO_BLINK 0x0E
#define LCD_CMD_DISPLAY_CURSOR_BLINK  0x0F
#define LCD_CMD_4BIT_2ROW_5X7         0x28
#define LCD_CMD_8BIT_2ROW_5X7         0x38

// function to shift out a byte via 74HC595
void shiftOutByte(uint8_t val) {
    for (int8_t i = 7; i >= 0; i--) {
        if (val & (1 << i))
            PORTD |= 0x04; // DS high
        else
            PORTD &= ~0x04; // DS low

        PORTD |= 0x08;  // SH_CP high
        PORTD &= ~0x08; // SH_CP low
    }
    PORTD |= 0x10;  // ST_CP high (latch)
    _delay_us(1);
    PORTD &= ~0x10; // ST_CP low
}

// Send 4-bit nibble to LCD through shift register
void shiftOutNibble(uint8_t nibble, uint8_t control) {
    uint8_t dataOut = 0;

    if (nibble & 0x01) dataOut |= LCD_D4;
    if (nibble & 0x02) dataOut |= LCD_D5;
    if (nibble & 0x04) dataOut |= LCD_D6;
    if (nibble & 0x08) dataOut |= LCD_D7;

    dataOut |= control;

    // EN = 1
    shiftOutByte(dataOut | LCD_EN);
    _delay_us(1); // >=450ns, safe
    // EN = 0
    shiftOutByte(dataOut & ~LCD_EN);
    _delay_us(50); // allow LCD to process
}

void lcd_send_command(uint8_t command) {
    shiftOutNibble(command >> 4, 0);     // RS = 0
    shiftOutNibble(command & 0x0F, 0);   // RS = 0
}

void lcd_write_character(char character) {
    shiftOutNibble(character >> 4, LCD_RS);    // RS = 1
    shiftOutNibble(character & 0x0F, LCD_RS);  // RS = 1
}

void lcd_write_str(const char* str) {
    while (*str) {
        lcd_write_character(*str++);
    }
}

void lcd_write_uint3(uint16_t val) {
    if (val > 999) val = 999;

    char buffer[4]; // 3 digits + null terminator
    sprintf(buffer, "%03d", val);  // Always 3 digits, e.g., "007"
    lcd_write_str(buffer);
}

void lcd_clear() {
    lcd_send_command(LCD_CMD_CLEAR_DISPLAY);
    _delay_ms(2);
}

void lcd_goto_xy(uint8_t line, uint8_t pos) {
    lcd_send_command((0x80 | (line << 6)) + pos);
    _delay_us(50);
}

void lcd_init() {
    _delay_ms(50);  // LCD power-on delay

    // LCD 4-bit init sequence (per datasheet recommendation)
    lcd_send_command(0x28);  // 4-bit mode, 2 lines, 5x7 font
    _delay_ms(5);
    lcd_send_command(LCD_CMD_DISPLAY_CURSOR_BLINK);
    _delay_ms(5);
    lcd_send_command(0x80);  // Set DDRAM address to 0
    _delay_ms(10);
    lcd_clear();
}

#endif /* LCD_H_ */