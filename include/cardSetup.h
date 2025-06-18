#include <avr/pgmspace.h>
#include "spiAVR.h"
#include "stdlib.h" 
#include "bitmap.h"


const char suits[] = {'H', 'D', 'S', 'C'};
const char ranks[] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K'};


char RandomSuit() {
    return suits[rand() % 4];
}


char RandomRank(int index) {
    return ranks[index];
}

int RandomRankIndex() {
    return (rand() % 13);
}

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
  return (b ?  (x | (0x01 << k))  :  (x & ~(0x01 << k)) );
             //   Set bit to 1           Set bit to 0
}

unsigned char GetBit(unsigned char x, unsigned char k) {
  return ((x & (0x01 << k)) != 0);
}

void HardwareReset() {
    PORTD = SetBit(PORTD,7,0);  // RESET = LOW
    _delay_ms(200);
    PORTD = SetBit(PORTD,7,1);   // RESET = HIGH
    _delay_ms(200);
}
void Send_Command(int cmd) {
    PORTB = SetBit(PORTB,0,0);   // A0 = 0 (command)
    PORTD = SetBit(PORTD,6,0);   // CS = 0
    SPI_SEND(cmd);
    PORTD = SetBit(PORTD,6,1);     // CS = 1
}

void Send_Data(int data) {
    PORTB = SetBit(PORTB,0,1);    // A0 = 1 (data)
    PORTD = SetBit(PORTD,6,0);   // CS = 0
    SPI_SEND(data);
    PORTD = SetBit(PORTD,6,1);     // CS = 1
}
void ST7735_init() {
    HardwareReset();

    Send_Command(0x01); // SWRESET
    _delay_ms(150);

    Send_Command(0x11); // SLPOUT
    _delay_ms(200);

    Send_Command(0x3A); // COLMOD
    Send_Data(0x05);    // 18-bit color (can also use 0x05 for 16-bit)
    _delay_ms(10);

    Send_Command(0x29); // DISPON
    _delay_ms(200);
}
int Color565(int r, int g, int b) {
    return ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
}

void SetAddrWindow(int x0, int y0, int x1, int y1) {
    Send_Command(0x2A); // CASET
    Send_Data(0x00); Send_Data(x0);
    Send_Data(0x00); Send_Data(x1);

    Send_Command(0x2B); // RASET
    Send_Data(0x00); Send_Data(y0);
    Send_Data(0x00); Send_Data(y1);

    Send_Command(0x2C); // RAMWR
}

void FillRect(int x, int y, int w, int h, int color) {
    SetAddrWindow(x, y, x + w - 1, y + h - 1);
    for (int i = 0; i < w * h; i++) {
        Send_Data(color >> 8);
        Send_Data(color & 0xFF);
    }
}
void DrawBitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint16_t* bitmap) {
    SetAddrWindow(x, y, x + width - 1, y + height - 1);
    for (uint16_t i = 0; i < width * height; i++) {
        uint16_t color = pgm_read_word(&bitmap[i]);
        Send_Data(color >> 8);
        Send_Data(color & 0xFF);
    }
}

void DrawNumRank(uint8_t x, uint8_t y, uint8_t rank, uint16_t color) {
    int index = rank;

    if (index == -1) return;

    for (uint8_t col = 0; col < 6; col++) {
        uint8_t line = BigNummaps[index][col];
        for (uint8_t row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                SetAddrWindow(x + col, y + row, x + col, y + row);
                Send_Data(color >> 8);
                Send_Data(color & 0xFF);
            }
        }
    }
}

void DrawNum(uint8_t x, uint8_t y,int np, int num){
    if(np != 0 && num < 10){
        if(np == 1){
            DrawNumRank(x + 8,y,10,Color565(0,0,0));
        }
        else if(np == 2){
            DrawNumRank(x + 8,y,11,Color565(0,0,0));
        }
        DrawNumRank(x + 14,y,num,Color565(0,0,0));
    }
    else{
        if(num < 10){
            DrawNumRank(x + 14,y,num,Color565(0,0,0));
            DrawNumRank(x + 7,y,0,Color565(0,0,0));
            DrawNumRank(x,y,0,Color565(0,0,0));
        }
        else if (num < 100){
            DrawNumRank(x + 14,y,(num % 10),Color565(0,0,0));
            DrawNumRank(x + 7,y,((num - (num % 10))/10),Color565(0,0,0));
            DrawNumRank(x,y,0,Color565(0,0,0));
        }
        else if (num < 1000){
            DrawNumRank(x + 14,y,(num % 10),Color565(0,0,0));
            DrawNumRank(x + 7,y,(((num % 100) - (num % 10))/10),Color565(0,0,0));  
            DrawNumRank(x,y,((num - (num % 100))/100),Color565(0,0,0));      
        }
        else{
            DrawBitmap(x + 3,y,13,6,infinity);
        }
    }
}

void DrawCharRank(uint8_t x, uint8_t y, char rank, uint16_t color) {
    int index = -1;
    for (uint8_t i = 0; i < 13; i++) {
        if (ranks[i] == rank) {
            index = i;
            break;
        }
    }

    if (index == -1) return;

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = Nummaps[index][col];
        for (uint8_t row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                SetAddrWindow(x + col, y + row, x + col, y + row);
                Send_Data(color >> 8);
                Send_Data(color & 0xFF);
            }
        }
    }
}

void DrawReverseCharRank(uint8_t x, uint8_t y, char rank, uint16_t color) {
    int index = -1;
    for (uint8_t i = 0; i < 13; i++) {
        if (ranks[i] == rank) {
            index = i;
            break;
        }
    }

    if (index == -1) return; 

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = NumReversemaps[index][col];
        for (uint8_t row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                SetAddrWindow(x + col, y + row, x + col, y + row);
                Send_Data(color >> 8);
                Send_Data(color & 0xFF);
            }
        }
    }
}

void DrawSuit(uint8_t x, uint8_t y, char suit){
  if(suit == 'H'){
    DrawBitmap(x + 6,y + 11,11,10,heartmap);
  }
  else if(suit == 'D'){
    DrawBitmap(x + 6,y + 11,11,10,diamondmap);
  }
  else if(suit == 'S'){
    DrawBitmap(x + 6,y + 11,11,10,spademap);
  }
  else if(suit == 'C'){
    DrawBitmap(x + 6,y + 11,11,10,clubmap);
  }

}
