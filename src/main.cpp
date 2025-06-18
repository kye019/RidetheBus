#include "spiAVR.h"
// #include "stdlib.h" 
#include "time.h"
#include "timerISR.h"
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "cardSetup.h"
#include "LCD.h"

#define NUM_TASKS 8

bool isLeft = false;
bool isRight = false;
bool isEnter = false;
bool isStart = false;
unsigned char isWon; //1 = small win, 2 = small lost, 3 = big win, 4 = game over
unsigned char currentstate = 1; //1 = Start, 2 = Bet, 3 = First, 4 = Second, 5 = Third, 6 = Fourth, 7 = Leave, 8 = End
unsigned char currentMoney = 10;
unsigned char InitalBet = 1;
unsigned char currentBet = 1;


typedef struct _task{
	signed 	 char state; 		//Task's current state
	unsigned long period; 		//Task period
	unsigned long elapsedTime; 	//Time elapsed since last task tick
	int (*TickFct)(int); 		//Task tick function
} task;

char rankIndex;
char suit;

const unsigned long Task1_Period = 250;
const unsigned long Task2_Period = 150;
const unsigned long Task3_Period = 150;
const unsigned long Task4_Period = 150;
const unsigned long Task5_Period = 150;
const unsigned long Task6_Period = 250;
const unsigned long Task7_Period = 250;
const unsigned long Task8_Period = 250;
const unsigned long GCD_PERIOD = 1;


task tasks[NUM_TASKS];



enum Task1_States {Task1_Start,Task1_Bet, Task1_First, Task1_Second, Task1_Third, Task1_Fourth, Task1_Leave, Task1_End};
int TickFct_Task1(int state);

enum Task2_States {Task2_Start,Task2_Wait,Task2_Press,Task2_Release};
int TickFct_Task2(int state);

enum Task3_States {Task3_Start,Task3_Wait,Task3_Press,Task3_Release};
int TickFct_Task3(int state);

enum Task4_States {Task4_Start,Task4_Wait,Task4_Press,Task4_Release};
int TickFct_Task4(int state);

enum Task5_States {Task5_Start,Task5_Wait,Task5_Press,Task5_Release};
int TickFct_Task5(int state);

enum Task6_States {Task6_Start,Task6_Idle, Task6_First, Task6_Second, Task6_Third, Task6_Fourth, Task6_Info, Task6_End};
int TickFct_Task6(int state);

enum Task7_States {Task7_Start,Task7_Beep};
int TickFct_Task7(int state);

enum Task8_States {Task8_Start,Task8_LED, Task8_Light};
int TickFct_Task8(int state);

void buzzer_set_freq(uint16_t freq) {
    if (freq == 0) {
        TCCR0A &= ~(1 << COM0B1);       // Disable output compare
        PORTD &= ~(1 << PD5);           // Ensure PD5 is LOW
        return;
    }

    TCCR0A |= (1 << COM0B1);            // Enable output compare

    uint32_t top = (F_CPU / (64UL * freq)) - 1;
    if (top > 255) top = 255;

    OCR0B = (uint8_t)top;               // Set compare value for OC0B
}

void buzzer_init() {
    // Fast PWM, non-inverting mode on OC0A
    TCCR0A = (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);

    TCCR0B = (1 << CS01) | (1 << CS00);
    

    buzzer_set_freq(0);
}

void buzzer_beep(uint16_t freq, uint16_t duration_ms) {
    buzzer_set_freq(freq);

    while (duration_ms--) {
        _delay_ms(1);
    }
    buzzer_set_freq(0); // Stop
}
void buzzer_good() {
    for (int i = 0; i < 5; i++) {
        buzzer_beep(3000, 50);
        _delay_ms(50);
    }
}

void buzzer_bad() {
    for (int i = 0; i < 5; i++) {
        buzzer_beep(1000, 100);
        _delay_ms(60);
    }
}
void buzzer_victory() {
  buzzer_beep(1047, 150); _delay_ms(30); // C6
  buzzer_beep(1319, 150); _delay_ms(30); // E6
  buzzer_beep(1568, 200); _delay_ms(50); // G6
  buzzer_beep(2093, 150); _delay_ms(30); // C7
  buzzer_beep(2637, 150); _delay_ms(30); // E7
  buzzer_beep(2794, 200); _delay_ms(50); // G7
  buzzer_beep(1568, 150); _delay_ms(30); // G6
  buzzer_beep(2637, 150); _delay_ms(30); // E7
  buzzer_beep(2093, 200); _delay_ms(50); // C7
  buzzer_beep(1319, 150); _delay_ms(30); // E6
  buzzer_beep(1568, 150); _delay_ms(30); // G6
  buzzer_beep(1047, 200); _delay_ms(50); // C6
  buzzer_beep(   0, 200); _delay_ms(50); // Rest
}
void buzzer_sad() {
  buzzer_beep(1047, 150); _delay_ms(30); // C6
  buzzer_beep(1319, 150); _delay_ms(30); // E6
  buzzer_beep(1568, 200); _delay_ms(50); // G6
  buzzer_beep(2093, 150); _delay_ms(30); // C7
  buzzer_beep(2637, 150); _delay_ms(30); // E7
  buzzer_beep(2794, 200); _delay_ms(50); // G7
  buzzer_beep(1568, 150); _delay_ms(30); // G6
  buzzer_beep(2637, 150); _delay_ms(30); // E7
  buzzer_beep(2093, 200); _delay_ms(50); // C7
  buzzer_beep(1319, 150); _delay_ms(30); // E6
  buzzer_beep(1568, 150); _delay_ms(30); // G6
  buzzer_beep(1047, 200); _delay_ms(50); // C6
  buzzer_beep(   0, 200); _delay_ms(50); // Rest
}

void TimerISR() {
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
		if ( tasks[i].elapsedTime == tasks[i].period ) {           // Check if the task is ready to tick
			tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
			tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
		}
		tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
	}
}


void DrawCard(uint8_t x, uint8_t y){
  int white = Color565(255,255,255);
  int dark_red = Color565(98,42,60);

  rankIndex = RandomRankIndex();
  suit = RandomSuit();


  FillRect(x + 1, y + 1, 21, 29, white);

  DrawSuit(x,y,suit);

  DrawCharRank(x + 3,y + 3 , RandomRank(rankIndex), dark_red);
  DrawReverseCharRank(x + 15,y + 21 , RandomRank(rankIndex), dark_red);
}



int main(void) {
    //TODO: initialize all your inputs and ouputs
    srand(0);

    DDRC = 0x00;  
    PORTC = 0xFF;
  
    DDRB   = 0xFF; 
    PORTB  = 0x00; 
  
  
    DDRD   = 0xFF; 
    PORTD  = 0x00;

    // Initialize SPI
    SPI_INIT();

    // Initialize LCD
    ST7735_init();

    lcd_init();
    buzzer_init();

    int i = 0;

    tasks[i].period = Task1_Period;
    tasks[i].state = Task1_Start;
    tasks[i].elapsedTime = Task1_Period;
    tasks[i].TickFct = &TickFct_Task1;
  
    i++;
  
    tasks[i].period = Task2_Period;
    tasks[i].state = Task2_Start;
    tasks[i].elapsedTime = Task2_Period;
    tasks[i].TickFct = &TickFct_Task2;
  
    i++;
  
    tasks[i].period = Task3_Period;
    tasks[i].state = Task3_Start;
    tasks[i].elapsedTime = Task3_Period;
    tasks[i].TickFct = &TickFct_Task3;
  
    i++;
  
    tasks[i].period = Task4_Period;
    tasks[i].state = Task4_Start;
    tasks[i].elapsedTime = Task4_Period;
    tasks[i].TickFct = &TickFct_Task4;

    i++;
  
    tasks[i].period = Task5_Period;
    tasks[i].state = Task5_Start;
    tasks[i].elapsedTime = Task5_Period;
    tasks[i].TickFct = &TickFct_Task5;
  
    i++;
  
    tasks[i].period = Task6_Period;
    tasks[i].state = Task6_Start;
    tasks[i].elapsedTime = Task6_Period;
    tasks[i].TickFct = &TickFct_Task6;

    i++;
  
    tasks[i].period = Task7_Period;
    tasks[i].state = Task7_Start;
    tasks[i].elapsedTime = Task7_Period;
    tasks[i].TickFct = &TickFct_Task7;

    i++;
  
    tasks[i].period = Task8_Period;
    tasks[i].state = Task8_Start;
    tasks[i].elapsedTime = Task8_Period;
    tasks[i].TickFct = &TickFct_Task8;
  
    TimerSet(GCD_PERIOD);
    TimerOn();


    while (1) {}

    return 0;
}

int TickFct_Task1(int state){
  static unsigned char count = 0;
  static int nextstate = Task1_Start;
  static char rank2;
  static char rank3;

    switch(state) {
        case Task1_Start:
            if(isStart){
              int poker_green = Color565(53,101,77);


              FillRect(0, 0, 130, 130, poker_green);
              FillRect(10, 5, 110, 40, Color565(255,255,255));
              FillRect(10, 85, 110, 40, Color565(255,255,255));

              DrawBitmap(15,8,35,10,Bet);
              DrawBitmap(75,8,35,10,Cur);

              DrawNum(21,25,0,InitalBet);
              DrawNum(81,25,0,currentMoney);


              DrawBitmap(10,50,23,31,CardBack);
              DrawBitmap(40,50,23,31,CardBack);
              DrawBitmap(70,50,23,31,CardBack);
              DrawBitmap(100,50,23,31,CardBack);
              state = Task1_Bet;
              _delay_ms(500);
            }
            break;
        case Task1_Bet:
          static bool one = false;
          static bool two = false;         
          static unsigned char xposition = 25;

          if(!one && !two){
            FillRect(10, 85, 110, 40, Color565(255,255,255));
            DrawBitmap(10,50,23,31,CardBack);
            DrawBitmap(40,50,23,31,CardBack);
            DrawBitmap(70,50,23,31,CardBack);
            DrawBitmap(100,50,23,31,CardBack);
            DrawNum(19,95,1,5);
            DrawNum(39,95,1,1);
            DrawNum(59,95,2,1);
            DrawNum(79,95,2,5);

            FillRect(81,25,20,10,Color565(255,255,255));
            DrawNum(81,25,0,currentMoney);
            one = true;
            two = true;
          }
          if(InitalBet > currentMoney && currentMoney != 0){
            InitalBet = currentMoney;
            FillRect(21,25,20,10,Color565(255,255,255));
            DrawNum(21,25,0,InitalBet);
          }

          if(isLeft){
            one = false;
            two = true;
            if (xposition > 25){
              xposition = xposition - 20;
            }
            FillRect(10, 85, 110, 40, Color565(255,255,255));

            FillRect(xposition + 3, 92, 14, 12, Color565(0,0,0));
            FillRect(xposition + 4, 93, 12, 10, Color565(255,255,255));
        
            
            DrawNum(19,95,1,5);
            DrawNum(39,95,1,1);
            DrawNum(59,95,2,1);
            DrawNum(79,95,2,5);

          }
          if(isRight){
            one = true;
            two = false;
            if (xposition < 85){
              xposition = xposition + 20;
            }
            FillRect(10, 85, 110, 40, Color565(255,255,255));

            FillRect(xposition + 3, 92, 14, 12, Color565(0,0,0));
            FillRect(xposition + 4, 93, 12, 10, Color565(255,255,255));

            DrawNum(19,95,1,5);
            DrawNum(39,95,1,1);
            DrawNum(59,95,2,1);
            DrawNum(79,95,2,5);

          }
          if(isEnter && (one ^ two)){

            if(xposition == 25 && InitalBet >= 5){
              InitalBet = InitalBet - 5;
            }
            else if(xposition == 45 && InitalBet != 0){
              InitalBet = InitalBet - 1;
            }
            else if(xposition == 65 && InitalBet < currentMoney){
              InitalBet = InitalBet + 1;
            }
            else if(xposition == 85 && InitalBet <= currentMoney - 5){
              InitalBet = InitalBet + 5;
            }
            FillRect(21,25,20,10,Color565(255,255,255));
            DrawNum(21,25,0,InitalBet);

          }

          if(isStart){
            FillRect(10, 85, 110, 40, Color565(255,255,255));
            FillRect(75, 90, 25, 25, Color565(0,0,0));
            FillRect(35, 90, 25, 25, Color565(255,0,0));

            one = false;
            two = false;

            state = Task1_First;
            _delay_ms(500);
          }

          if(currentMoney == 0){
            isWon = 4;
            state = Task1_End;
          }

            break;

        case Task1_First:
          static bool left = false;
          static bool right = false;

          if(isStart){
            state = Task1_Start;
            _delay_ms(500);
          }

          if(isLeft){
            right = false;
            left = true;
            FillRect(75, 90, 25, 25, Color565(0,0,0));
            FillRect(35, 90, 25, 25, Color565(255,0,0));
            FillRect(50, 105, 10, 10, Color565(255,255,255));
          }
          if(isRight){
            right = true;
            left = false;
            FillRect(75, 90, 25, 25, Color565(0,0,0));
            FillRect(35, 90, 25, 25, Color565(255,0,0));
            FillRect(90, 105, 10, 10, Color565(255,255,255));
          }
          if(isEnter && (left || right)){
            DrawCard(10,50);
            rank2 = rankIndex;

            if((left && (suit == 'H' || suit == 'D')) || (right && (suit == 'C' || suit == 'S'))){

              currentBet = InitalBet;
              isWon = 1;
              nextstate = Task1_Second;
              state = Task1_Leave;
              _delay_ms(500);
            }
            else{
             
              currentMoney = currentMoney - InitalBet;
              isWon = 2;
              FillRect(81,25,20,10,Color565(255,255,255));
              DrawNum(81,25,0,currentMoney);
              state = Task1_Bet;
            }
            right = false;
            left = false;
          }

            break;
        case Task1_Second:
          static bool up = false;
          static bool down = false;

          if(!up && !down){
            DrawBitmap(35,90,25,25,UpArrow);
            DrawBitmap(75,90,25,25,DownArrow);
          }

          if(isStart){
            state = Task1_Start;
            _delay_ms(500);
          }

          if(isLeft){
            down = false;
            up = true;
            DrawBitmap(35,90,25,25,UpArrow);
            DrawBitmap(75,90,25,25,DownArrow);
            FillRect(50, 105, 10, 10, Color565(0,0,0));
          }
          if(isRight){
            down = true;
            up = false;
            DrawBitmap(35,90,25,25,UpArrow);
            DrawBitmap(75,90,25,25,DownArrow);
            FillRect(90, 105, 10, 10, Color565(0,0,0));
          }
          if(isEnter && (up || down)){
            DrawCard(40,50);
            rank3 = rankIndex;

            if((up && (rankIndex > rank2)) || (down && (rankIndex < rank2))){
              currentBet = InitalBet * 2;
              isWon = 1;
              nextstate = Task1_Third;
              state = Task1_Leave;
              _delay_ms(500);
            }
            else{
              currentMoney = currentMoney - InitalBet;
              isWon = 2;
              FillRect(81,25,20,10,Color565(255,255,255));
              DrawNum(81,25,0,currentMoney);
              state = Task1_Bet;
            }
              down = false;
              up = false;
          }
          break;
        case Task1_Third:
          static bool in = false;
          static bool out = false;

          if(!in && !out){
            DrawBitmap(35,90,25,25,Inside);
            DrawBitmap(75,90,25,25,Outside);
          }

          if(isStart){
            state = Task1_Start;
            _delay_ms(500);
          }

          if(isLeft){
            out = false;
            in = true;
              DrawBitmap(35,90,25,25,Inside);
              DrawBitmap(75,90,25,25,Outside);
            FillRect(50, 105, 10, 10, Color565(0,0,0));
          }
          if(isRight){
            out = true;
            in = false;
              DrawBitmap(35,90,25,25,Inside);
              DrawBitmap(75,90,25,25,Outside);
            FillRect(90, 105, 10, 10, Color565(0,0,0));
          }
          if(isEnter && (in || out)){
            DrawCard(70,50);

            int minRank = (rank2 < rank3) ? rank2 : rank3;
            int maxRank = (rank2 > rank3) ? rank2 : rank3;

            if((in && rankIndex >= minRank && rankIndex <= maxRank) || (out && (rankIndex < minRank || rankIndex > maxRank))){

              currentBet = InitalBet * 3;
              isWon = 1;
              nextstate = Task1_Fourth;
              state = Task1_Leave;
              _delay_ms(500);
            }
            else{

              currentMoney = currentMoney - InitalBet;
              isWon = 2;
              FillRect(81,25,20,10,Color565(255,255,255));
              DrawNum(81,25,0,currentMoney);
              state = Task1_Bet;
            }
            out = false;
            in = false;
          }
            break;
        case Task1_Fourth:
          static bool a = false;
          static bool b = false;         
          static unsigned char xpos = 25;

          if(!a && !b){
            DrawSuit(30,90,'H');
            DrawSuit(50,90,'D');
            DrawSuit(70,90,'C');
            DrawSuit(90,90,'S');
          }

          if(isStart){
            state = Task1_Start;
            _delay_ms(500);
          }


          if(isLeft){
            a = false;
            b = true;
            if (xpos > 25){
              xpos = xpos - 20;
            }
            FillRect(10, 85, 110, 40, Color565(255,255,255));

            FillRect(xpos + 5, 100, 13, 12, Color565(0,0,0));
        
            
            DrawSuit(25,90,'H');
            DrawSuit(45,90,'D');
            DrawSuit(65,90,'C');
            DrawSuit(85,90,'S');

          }
          if(isRight){
            a = true;
            b = false;
            if (xpos < 85){
              xpos = xpos + 20;
            }
            FillRect(10, 85, 110, 40, Color565(255,255,255));

            FillRect(xpos + 5, 100, 13, 12, Color565(0,0,0));

            DrawSuit(25,90,'H');
            DrawSuit(45,90,'D');
            DrawSuit(65,90,'C');
            DrawSuit(85,90,'S');

          }
          if(isEnter && (a || b)){
            DrawCard(100,50);

            if(((xpos == 25) && suit == 'H') || ((xpos == 45) && suit == 'D') || ((xpos == 65) && suit == 'C') || ((xpos == 85) && suit == 'S') ){
              currentBet = (InitalBet * 19);
              currentMoney += currentBet;
              isWon = 3;

              FillRect(81,25,20,10,Color565(255,255,255));
              DrawNum(81,25,0,currentMoney);
              state = Task1_Bet;
            }
            else{
              currentMoney = currentMoney - InitalBet;
              isWon = 2;
              FillRect(81,25,20,10,Color565(255,255,255));
              DrawNum(81,25,0,currentMoney);
              state = Task1_Bet;
            }
              a = false;
              b = false;
          }
            break;
        case Task1_Leave:
            FillRect(10, 85, 110, 40, Color565(255,255,255));
            if (isLeft){
              currentMoney = currentMoney + currentBet;
              isWon = 1;

              FillRect(81,25,20,10,Color565(255,255,255));
              DrawNum(81,25,0,currentMoney);
              state = Task1_Bet;
            }
            else if(isRight){
              state = nextstate;
            }
            break;
        case Task1_End:
            state = Task1_Start;
            break;

        default:
            state = Task1_Start;
            break;
    }
//1 = Start, 2 = Bet, 3 = First, 4 = Second, 5 = Third, 6 = Fourth, 7 = Leave, 8 = End
    switch(state) {
        case Task1_Start:
          currentstate = 1;
          if(count % 5 == 0){
            FillRect(0, 0, 130, 130, Color565(0,0,0));
            DrawBitmap(35,55,60,10, Start);  
          }
          count++;

          currentMoney = 10;
          InitalBet = 1;
          break;
        case Task1_Bet:
          currentstate = 2;
          break;

        case Task1_First:
          currentstate = 3;
          break;
        
        case Task1_Second:
          currentstate = 4;
            break;
        case Task1_Third:
          currentstate = 5;
            break;
        case Task1_Fourth:
          currentstate = 6;
            break;
        case Task1_Leave:
          currentstate = 7;
            break;
        case Task1_End:
          currentstate = 8;
            break;
    }

    return state;
}
int TickFct_Task2(int state){

    switch(state) {
        case Task2_Start:
          state = Task2_Wait;
          break;

        case Task2_Wait: 
          if(GetBit(PINC, 0)){
            isLeft = true;
            state = Task2_Press;
          }
          break;
        case Task2_Press:
          if(!GetBit(PINC,0)){
            state = Task2_Release;
          }
          break;
        case Task2_Release:
          state = Task2_Wait;
          break;
        default:
            state = Task2_Start;
            break;
    }

    switch(state) {
        case Task2_Start:
          break;
        case Task2_Wait:
          isLeft = false; 
          break;
        case Task2_Press:
          break;
        case Task2_Release:
          break;
    }

    return state;
}

int TickFct_Task3(int state){

    switch(state) {
        case Task3_Start:
          state = Task3_Wait;
          break;

        case Task3_Wait: 
          if(GetBit(PINC, 3)){
            isStart = true;
            state = Task3_Press;
          }
          break;
        case Task3_Press:
          if(!GetBit(PINC,3)){
            state = Task3_Release;
          }
          break;
        case Task3_Release:
          state = Task3_Wait;
          break;
        default:
            state = Task3_Start;
            break;
    }

    switch(state) {
        case Task3_Start:
          break;
        case Task3_Wait:
          break;
        case Task3_Press: 
          break;
        case Task3_Release:
          isStart = false;
          break;
    }

    return state;
}

int TickFct_Task4(int state){

    switch(state) {
        case Task4_Start:
          state = Task4_Wait;
          break;

        case Task4_Wait: 
          if(GetBit(PINC, 1)){
            isRight = true;
            state = Task4_Press;
          }
          break;
        case Task4_Press:
          if(!GetBit(PINC,1)){
            state = Task4_Release;
          }
          break;
        case Task4_Release:
          state = Task4_Wait;
          break;
        default:
            state = Task4_Start;
            break;
    }

    switch(state) {
        case Task4_Start:
          break;
        case Task4_Wait: 
          isRight = false;
          break;
        case Task4_Press:
          break;
        case Task4_Release:
          break;
    }

    return state;
}

int TickFct_Task5(int state){

    switch(state) {
        case Task5_Start:
          state = Task5_Wait;
          break;

        case Task5_Wait: 
          if(GetBit(PINC, 2)){
            isEnter = true;
            state = Task5_Press;
          }
          break;
        case Task5_Press:
          if(!GetBit(PINC,2)){
            state = Task5_Release;
          }
          break;
        case Task5_Release:
          state = Task5_Wait;
          break;
        default:
            state = Task5_Start;
            break;
    }

    switch(state) {
        case Task5_Start:
          break;
        case Task5_Wait: 
          isEnter = false;
          break;
        case Task5_Press:
          break;
        case Task5_Release:
          break;
    }

    return state;
}

// Note to self: come back to finish the currentstate system
int TickFct_Task6(int state){
    static unsigned char count = 43;
    static char num = 10;
    static int prestate = Task6_Start;

    switch(state) {
        case Task6_Start:
            lcd_clear();
            lcd_write_str("Press to Start! ");
            lcd_goto_xy(1,0);
            lcd_write_str("                ");
            if(currentstate == 2){
              state = Task6_Info;
              _delay_ms(500);
            }
            break;
        case Task6_Idle:
            if(num == 0){
              lcd_clear();
              lcd_write_str("Speed Up!!!     ");
              lcd_goto_xy(1,0);
              lcd_write_str("                ");
            }
            else{
              lcd_clear();
              lcd_write_str("Leave?          ");
              lcd_goto_xy(1,0);
              lcd_write_str("Time: ");
              lcd_write_uint3(num);
              lcd_goto_xy(0,0);
            }
            num = count/4;
            if(currentstate == 1){
              state = Task6_Start;
            }
            if(currentstate != 7 && currentstate != 2){
              state = prestate;
              _delay_ms(500);
            }         
            else if(currentstate == 2){
              lcd_clear();
              lcd_write_str("You Won!        ");
              lcd_goto_xy(1,0);
              lcd_write_str("Winnings: +");
              lcd_write_uint3(currentBet);
              lcd_goto_xy(0,0);
              _delay_ms(3000);
              state = Task6_Info;
            } 
            break;

        case Task6_First:
            lcd_clear();
            lcd_write_str("Red or Black?   ");
            lcd_goto_xy(1,0);
            lcd_write_str("                ");
            if(currentstate == 1){
              state = Task6_Start;
            }
            if(currentstate == 7){
              prestate = Task6_Second;
              count = 43;
              state = Task6_Idle;
            }
            if(currentstate == 2){
              state = Task6_End;
            }
            break;
        case Task6_Second:
            lcd_clear();
            lcd_write_str("Higher or Lower?");
            lcd_goto_xy(1,0);
            lcd_write_str("                ");
            if(currentstate == 1){
              state = Task6_Start;
            }
            if(currentstate == 7){
              prestate = Task6_Third;
              count = 43;
              state = Task6_Idle;
            }
            if(currentstate == 2){
              state = Task6_End;
            }
          break;
        case Task6_Third:
            lcd_clear();
            lcd_write_str("In or Out?      ");
            lcd_goto_xy(1,0);
            lcd_write_str("                ");
            if(currentstate == 1){
              state = Task6_Start;
            }
            if(currentstate == 7){
              prestate = Task6_Fourth;
              count = 43;
              state = Task6_Idle;
            }
            if(currentstate == 2){
              state = Task6_End;
            }
            break;
        case Task6_Fourth:
            lcd_clear();
            lcd_write_str("What Suit?      ");
            lcd_goto_xy(1,0);
            lcd_write_str("                ");
            if(currentstate == 1){
              state = Task6_Start;
            }
            if(currentBet == (InitalBet * 19)){
              lcd_clear();
              lcd_write_str("You Won!        ");
              lcd_goto_xy(1,0);
              lcd_write_str("Winnings: +");
              lcd_write_uint3(currentBet);
              lcd_goto_xy(0,0);
              _delay_ms(3000);
              state = Task6_Info;
            }
            else if(currentstate == 2){
              state = Task6_End;
            }
            break;
        case Task6_Info:
            lcd_clear();
            lcd_write_str("What's your Bet?");
            lcd_goto_xy(1,0);
            lcd_write_str("                ");
            if(currentstate == 3){
              state = Task6_First;
            }
            break;
        case Task6_End:
            lcd_clear();
            lcd_write_str("You Lost :(     ");
            lcd_goto_xy(1,0);
            lcd_write_str("Losses: -");
            lcd_write_uint3(InitalBet);
            lcd_goto_xy(0,0);
            _delay_ms(3000);
            state = Task6_Info;
            if(currentMoney == 0){
              lcd_clear();
              lcd_write_str("Game Over!!!    ");
              lcd_goto_xy(1,0);
              lcd_write_str("                ");
              _delay_ms(3000);  
              state = Task6_Start;           
            }
            break;

        default:
            state = Task6_Start;
            break;
    }

    switch(state) {
        case Task6_Start:
          break;
        case Task6_Idle:
          if(count != 0){
            count--;
          }
          break;

        case Task6_First:
          break;
        
        case Task6_Second:
            break;
        case Task6_Third:
            break;
        case Task6_Fourth:
            break;
        case Task6_Info:
            break;
        case Task6_End:
            break;
    }

    return state;
}

int TickFct_Task7(int state) {
    switch(state) {
      case Task7_Start:
        if(currentstate == 2){
          state = Task7_Beep;
        }
        break;
      case Task7_Beep:
        if(isWon == 1){
          buzzer_good();
          isWon = 11;
        }
        else if(isWon == 2){
          buzzer_bad();
          isWon = 22;
        }
        else if(isWon == 3){
          buzzer_victory();
          isWon = 33;
        }
        else if(isWon == 4){
          buzzer_sad();
          isWon = 44;
          state = Task7_Start;
        }
        break;
      default:
        state = Task7_Start;
        break;
    }

    switch(state) {
      case Task7_Start:
        break;
      case Task7_Beep:
        break;
    }

    return state;
}

int TickFct_Task8(int state) {
    static unsigned char counter;
    static unsigned char curr = 0;
    switch(state) {
    case Task8_Start:
            PORTB &= 0xE9; // Turn off all LEDs
            counter = 0;
            if (currentstate == 2) {
                state = Task8_LED;
            }
            break;

        case Task8_LED:
            counter = 0;
            if (isWon == 11) {
                curr = 1;
                isWon = 0;
            }
            else if (isWon == 22) {
                curr = 2;
                isWon = 0;
            }
            else if (isWon == 33) {
                curr = 3;
                isWon = 0;
            }
            else if (isWon == 44) {
                curr = 4;
                isWon = 0;
            }
            if (curr != 0) {
                state = Task8_Light;
            }
            break;

        case Task8_Light:
            if (curr == 1 && counter < 8) {
                PORTB |= (1 << PB2); // Turn on Green (pin 10) steadily
            }
            else if (curr == 2 && counter < 8) {
                    PORTB |= (1 << PB4); 
            }
            else if (curr == 3 && counter < 20) {
                    PORTB |= (1 << PB1); 
            }
            else if (curr == 4 && counter < 20) {
                    PORTB |= (1 << PB4); 
            }
            else {
                curr = 0;
                PORTB &= 0xE9; // Turn off all LEDs
                state = Task8_LED;
            }
            break;

        default:
            state = Task8_Start;
            break;
    }

    switch(state) {
        case Task8_Start:
            break;
        case Task8_LED:
            break;
        case Task8_Light:
            counter++;
            break;
    }

    return state;
}