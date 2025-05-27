#include <AltSoftSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <ServoTimer2.h>
#include <Wire.h>
#include <stdio.h>

// --- START DFPlayer Mini
AltSoftSerial mySoftwareSerial;  // RX, TX
DFRobotDFPlayerMini myDFPlayer;
// --- END DFPlayer Mini

// --- START Finish micro switch
const int leverPin = 7;
volatile bool leverPinInterrupt = false;
int leverState = LOW;
// --- END Finish micro switch

// --- START Stop start button
const int buttonPin = 6;
bool released = true;
int order[] = {6, 5, 4, 2};
int length = sizeof(order) / sizeof(order[0]);
int index = 0;
unsigned long lastDebounceTime = 0;
bool lastButtonState = LOW;
int buttonState;
volatile bool buttonPinInterrupt = false;
bool reading = LOW;
// --- END Stop start button

// --- START LCD
int LCD_ADDR = -1;
char game_won[] = "Ai castigat! Bravo!";
char game_lost[] = "Offf... Ai pierdut :(";
int text_displayed = 0; 
float lcd_countdown = 0;
int lcd_max_countdown = 4000;
// --- END LCD

unsigned long timer = 0;
float countdown = 0;

// --- START SERVO
#define pinTopServo 13
#define pinBottomServo 12
#define vertical 75
#define minServo 60
#define maxServo 90
#define deltaBottom 0.075
#define deltaTop 0.11
ServoTimer2 TopServo;
ServoTimer2 BottomServo;
int angleX = vertical;
int angleY = vertical;
int startTime;
bool isVertical = true;
// --- END SERVO

// --- START JOYSTICK
const int VRx = A0;
const int VRy = A1;
const int joystickButtonPin = 4;
volatile bool joystickButtonPinInterrupt = false;
bool joystickButtonState = false;
// --- END JOYSTICK

enum State {
  IDLE,
  COUNTING_START,
  PLAYING,
  LOST,
  WIN,
  WINNING_SONG_PLAYING
};
int state = IDLE;


ISR(PCINT2_vect) {
  if ((!reading && PIND & (1 << buttonPin)) || (reading && !(PIND & (1 << buttonPin))))
    buttonPinInterrupt = true;
  else if ((!(PIND & (1 << leverPin)) && !leverState))
    leverPinInterrupt = true;
  else if ((!(PIND & (1 << joystickButtonPin)) && !joystickButtonState) || ((PIND & (1 << joystickButtonPin)) && joystickButtonState))
    joystickButtonPinInterrupt = true;
}

void init_DFPlayer() {
  mySoftwareSerial.begin(9600);

  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("DFPLayer not initialized. Check SD card or connections."));
    while (true);
  }
  Serial.println();
  Serial.println(F("DFPlayer Mini module initialized!"));

  myDFPlayer.setTimeOut(500);
  myDFPlayer.volume(20);
  //  myDFPlayer.EQ(DFPLAYER_EQ_POP);
  //  myDFPlayer.EQ(DFPLAYER_EQ_ROCK);
  //  myDFPlayer.EQ(DFPLAYER_EQ_JAZZ);
  //  myDFPlayer.EQ(DFPLAYER_EQ_BASS);
  myDFPlayer.EQ(0);
}

void handle_DFPlayer() {
    if (state == WIN) {
      myDFPlayer.play(order[length - 1]);
      while (myDFPlayer.readState() == 0) delay(10);
      leverState = LOW;
      state = WINNING_SONG_PLAYING;
    } else if (state == WINNING_SONG_PLAYING || state == LOST) {
      if (myDFPlayer.readState() == 0) state = IDLE;
      countdown = 0;
    } else if (state == COUNTING_START) {
      if (myDFPlayer.readState() == 0) state = PLAYING;
    } else if (state == IDLE && buttonState == HIGH && released) {
      myDFPlayer.play(order[0]);
      while (myDFPlayer.readState() == 0) delay(10);
      released = false;
      state = COUNTING_START;
    } else if (state > IDLE && buttonState == HIGH && released) {
        myDFPlayer.stop();
        released=false;
        state = IDLE;
        countdown = 0;
    } else if (countdown < 0 && state == PLAYING) {
      myDFPlayer.play(order[2]);
      while (myDFPlayer.readState() == 0) delay(10);
      state = LOST;
    }else if (state == PLAYING && myDFPlayer.readState() == 0) {
      myDFPlayer.play(order[1]);
      while (myDFPlayer.readState() == 0) delay(10);
      countdown = 60;
    }
}

void init_Inputs(){
  DDRD &= ~(1 << buttonPin);
  PORTD |= (1 << buttonPin);

  DDRD &= ~(1 << leverPin);
  DDRD &= ~(1 << joystickButtonPin);
  PORTD |= (1 << joystickButtonPin);

  PCICR |= (1 << PCIE2); 
  PCMSK2 |= (1 << PCINT22) | (1 << PCINT23) | (1 << PCINT20); 
  PCIFR |= (1 << PCIF2);   
  sei();
}

void debounce_button() {
  if (buttonPinInterrupt) {
    lastDebounceTime = millis(); 
    reading = !reading;
    buttonPinInterrupt = false;
  }

  if ((millis() - lastDebounceTime) > 50) buttonState = reading;
  if (buttonState == LOW) released = true;
  lastButtonState = reading;
}

void handle_micro_switch() {
  if (leverPinInterrupt) {
    
    leverPinInterrupt = false;
    if (state == PLAYING) leverState = !leverState;
    
    Serial.println(leverState);
    Serial.println(state);
    if (leverState == HIGH && state == PLAYING) {
      state = WIN;
    }
  }
  
}

void lcd_send_half(uint8_t half, uint8_t rs) {
  uint8_t data = (half & 0xF0);         
  data |= (rs ? 0x01 : 0x00);             // RS bit
  data |= 0x08;                           // Backlight ON
  data |= 0x04;                           // Enable high
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(data);
  Wire.endTransmission();
  delayMicroseconds(2);
  
  data &= ~0x04;                          // Enable low
  Wire.beginTransmission(LCD_ADDR);
  Wire.write(data);
  Wire.endTransmission();
  delayMicroseconds(50);
}

void lcd_send_byte(uint8_t byte, uint8_t rs) {
  lcd_send_half(byte & 0xF0, rs);       // High half
  lcd_send_half((byte << 4) & 0xF0, rs); // Low half
}

void lcd_command(uint8_t cmd) {
  lcd_send_byte(cmd, 0);
}

void lcd_data(uint8_t data) {
  lcd_send_byte(data, 1);
}

void lcd_init() {
  delay(50);  // Wait for LCD to power up
  lcd_send_half(0x30, 0); delay(5);
  lcd_send_half(0x30, 0); delay(5);
  lcd_send_half(0x30, 0); delay(5);
  lcd_send_half(0x20, 0); delay(5); // Set to 4-bit mode

  lcd_command(0x28); // 4-bit, 2 line
  lcd_command(0x0C); // Display on
  lcd_command(0x06); // Entry mode
  lcd_command(0x01); // Clear
  delay(2);
}

void lcd_print(const char* str) {
  lcd_command(0x01);
  delay(2);
  while (*str) {
    lcd_data(*str++);
  }
}

void lcd_print_wrap(const char* str1, const char* str2) {
  lcd_command(0x01);
  delay(2);
  int cnt = 0;
  lcd_command(0x80);
  while (cnt < 16 && cnt < strlen(str1)) {
    lcd_data(str1[cnt]);
    cnt++;
  }

  cnt = 0;
  lcd_command(0x80 | 0x40);
  while (cnt < 16 && cnt < strlen(str2)) {
    lcd_data(str2[cnt]);
    cnt++;
  }
}

void lcd_print_alternating(int idx0, int idx1, const char* msg00, const char* msg01, const char*msg10, const char* msg11) {
  if (text_displayed != idx0 && text_displayed != idx1) {
    lcd_countdown = lcd_max_countdown;
    lcd_print_wrap(msg00, msg01);
    text_displayed = idx0;
  }

  if (text_displayed != idx0 && lcd_countdown < 0) {
    lcd_print_wrap(msg00, msg01);
    text_displayed = idx0;
    lcd_countdown = lcd_max_countdown;
  } else if (text_displayed != idx1 && lcd_countdown < 0) {
    lcd_print_wrap(msg10, msg11);
    text_displayed = idx1;
    lcd_countdown = lcd_max_countdown;
    }
}

char *index_to_time(int index, char* time) {
  if (index == 0) strcpy(time, "1:00");
  else if (index > 50) sprintf(time, "0:0%d", 60 - index);
  else sprintf(time, "0:%d", 60 - index);
}

void handle_lcd() {
  if (state == IDLE) {
    lcd_print_alternating(0, 1, "Pune bila pe", "START", "Apoi apasa buto-", "nul de start.");
  } else if (state == COUNTING_START) {
    if (text_displayed < 2 || text_displayed > 5) {
      lcd_countdown = 3000;
      text_displayed = 2;
      lcd_print("3...");
    } else if (lcd_countdown <= 2000 && lcd_countdown > 1000 && text_displayed != 3) {
      lcd_print("2...");
      text_displayed = 3;
    }
    else if (lcd_countdown <= 1000 && lcd_countdown > 0 && text_displayed != 4) {
      lcd_print("1...");
      text_displayed = 4;
    } else if (lcd_countdown <= 0 && text_displayed != 5) {
      lcd_print("START!");
      text_displayed = 5;
    }
  } else if (state == PLAYING) {
    char time[16];
    if (text_displayed < 6) {
      ++text_displayed;
      index_to_time(text_displayed - 6, time);
      lcd_print_wrap("Timp ramas:", time);
    } else if (countdown <= 60 - text_displayed + 5) {
      ++text_displayed;
      index_to_time(text_displayed - 6, time);
      lcd_print_wrap("Timp ramas:", time);
    }
  } else if (state == LOST) {
    if (text_displayed != 66) {
      lcd_print_wrap("Off...",  "Ai pierdut :(");
      text_displayed = 66;
    }
  } else if (state == WIN || state == WINNING_SONG_PLAYING) {
    if (text_displayed != 67) {
      lcd_print_wrap("Bravo!", "Ai castigat!");
      text_displayed = 67;
    }
  }
}

inline uint16_t degToUs(int deg)
{
  return map(deg, 0, 180, 544, 2400);
}

bool isZero(int val) { return val < 515 && val > 500;}

void handle_joystick_and_servos(unsigned long elapsed) {
  if (joystickButtonPinInterrupt) {
    joystickButtonState = !joystickButtonState;
    joystickButtonPinInterrupt = false;
  }
  if (state == PLAYING) {
    if (joystickButtonState) {
      if (!isVertical) {
        BottomServo.write(degToUs(vertical));
        TopServo.write(degToUs(vertical));
        isVertical == true;
      }
    } else {
      int xVal = analogRead(VRx);  // 0 to 1023
      int yVal = analogRead(VRy);  // 0 to 1023

      if (!isZero(xVal)) {
        float speed = (xVal - 1023 / 2.0) / (1023/2.0);
        angleX -= deltaBottom * speed * elapsed;
        angleX = max(angleX, minServo);
        angleX = min(angleX, maxServo);
        BottomServo.write(degToUs(angleX));
      }

      if (!isZero(yVal)) {
        float speed = (yVal - 1023 / 2.0) / (1023/2.0);
        angleY += deltaTop * speed * elapsed;
        angleY = max(angleY, minServo);
        angleY = min(angleY, maxServo);
        TopServo.write(degToUs(angleY));
      }

      isVertical = false;
    }
  } else if (!isVertical){
    isVertical = true;
    TopServo.write(degToUs(vertical));
    BottomServo.write(degToUs(vertical));
  }  

  delay(50);
}

void init_servo() {
  if (!TopServo.attach(pinTopServo) || !BottomServo.attach(pinBottomServo)) {
    Serial.println(F("Servo attach failed - too many servos?"));
    while (true) {}
  }

  TopServo.write(degToUs(vertical));
  BottomServo.write(degToUs(vertical));
}


void setup() {
  Serial.begin(115200);

  Wire.begin();
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      LCD_ADDR = address;
      break;
    }
  }
  if (LCD_ADDR != -1) {
    lcd_init();
  }
  init_DFPlayer();
  init_Inputs();
  lcd_print_wrap("Pune bila pe", "START");
  lcd_countdown = lcd_max_countdown;
  delay(10);
  init_servo();
  timer = millis();
}

void loop() {
  unsigned long elapsed = millis() - timer;
  if (countdown > 0) countdown -= elapsed / 1000.0;
  timer = millis();

  if (lcd_countdown >= 0) lcd_countdown -= elapsed;

  debounce_button();
  delay(10);
  handle_micro_switch();
  delay(10);
  handle_DFPlayer();
  delay(10);
  handle_lcd();
  delay(10);
  handle_joystick_and_servos(elapsed);
}
