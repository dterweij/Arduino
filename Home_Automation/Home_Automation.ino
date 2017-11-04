//
//
// Start of home automation system. (COM11)
//
//
//
#include "Timer.h"              // Timer library - https://github.com/JChristensen/Timer
#include "pitches.h"            // Speaker notes - https://www.arduino.cc/en/Tutorial/toneMelody
#include "RTClib.h"             // The RTC Module (Standard lib)

// ############################################################################
// # RTC Clock Module
RTC_DS1307 rtc; // hardware RTC

// ############################################################################
// # Automatic process
int Automatic = 1; // todo - make this as button

// ############################################################################
// # Speaker
#define SPEAKER 12 // Pin

// ############################################################################
// # RGB LED strip (5050)
#define RED_PIN 3     // Pin
#define GREEN_PIN 10  // Pin
#define BLUE_PIN 11   // Pin
#define RGB_BRIGHTNESS 255     // Brightness leds -- todo make this variable by analog input with potentiometer
int redLevel = 0;
int greenLevel = 0;
int blueLevel = 0;
float pi = 3.14159;
float rgbcounter = 0;

// ############################################################################
// # 4 Relay Module
#define RLA 8       // Pin Relay #1 -- FAN power on/off
#define RLB 4       // Pin Relay #2 -- Power to on/off leds
#define RLC 6       // Pin Relay #3 -- RGB power on/off
#define RLD 9       // Pin Relay #4 -- To Lamp relay (240Volt)

// ############################################################################
// # LDR
#define solarPin 2  // Pin
int solarValue        = 0;
int solarState        = 1;
int long sVT = 0;


// ############################################################################
// # Button (momentary)
const unsigned int buttonPin  = 7;  // Pin
int buttonState       = LOW;
int lastButtonState   = LOW;

// ############################################################################
// # Main state of lamp trigger
int lampState         = LOW;

// ############################################################################
// # Timer
long interval = 1;     // Turn off lamp/rgb at X hour (Default at 01:00)

// ############################################################################
// # Init timer library
Timer t;

// ############################################################################
// # Define space for dec2strf function
char str[16] = "...";

// ############################################################################
// # Setup
void setup() {
  printlogHeader();
  Serial.println(F("Start up"));
  // Start Serial
  Serial.begin(9600);

  if (! rtc.begin()) {
    printlogHeader();
    Serial.println(F("ERROR: Couldn't find RTC"));
  }
  
  // enable once for manual setting, needed to change the time for DST
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  if (! rtc.isrunning()) {
    printlogHeader();
    Serial.println(F("RTC is NOT running!"));
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  // Play startup tones
  tone(SPEAKER, NOTE_C6, 150);
  delay(200);
  tone(SPEAKER, NOTE_D6, 150);
  delay(200);
  tone(SPEAKER, NOTE_C7, 150);
  delay(200);
  noTone(SPEAKER);

  // Set pin mode
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(RLA, OUTPUT);
  pinMode(RLB, OUTPUT);
  pinMode(RLC, OUTPUT);
  pinMode(RLD, OUTPUT);
  pinMode(solarPin, INPUT);
  pinMode(buttonPin, INPUT);

  // Set pin defaults
  digitalWrite(RLA, HIGH);    // Relay off  -- NOT IN USE
  digitalWrite(RLB, HIGH);    // Relay off  -- High - RED LED // Low - GREEN LED
  digitalWrite(RLC, HIGH);    // Relay off  -- RGB Power
  digitalWrite(RLD, HIGH);    // Relay off  -- Lamp Power

  // Relay test
  printlogHeader();
  Serial.println(F("Relay Test"));
  delay(200);
  digitalWrite(RLA, LOW);
  delay(200);
  digitalWrite(RLB, LOW);
  delay(200);
  digitalWrite(RLC, LOW);
  delay(200);
  digitalWrite(RLD, LOW);
  delay(200);

  digitalWrite(RLA, HIGH);
  delay(200);
  digitalWrite(RLB, HIGH);
  delay(200);
  digitalWrite(RLC, HIGH);
  delay(200);
  digitalWrite(RLD, HIGH);
  delay(200);

  printlogHeader();
  Serial.println(F("RGB Test"));
  digitalWrite(RLC, LOW); // Power on
  setColourRgb(0, 0, 0);      // Turn off RGB OFF

  // Test leds
  setColourRgb(255, 0, 0);    // RGB Red
  delay(500);
  setColourRgb(0, 255, 0);    // RGB Green
  delay(500);
  setColourRgb(0, 0, 255);    // RGB Blue
  delay(500);

  setColourRgb(0, 0, 0);      // Turn off RGB OFF
  digitalWrite(RLC, HIGH); // Power off

  //Timers
  int buttonEvent = t.every(25, checkButton, (void*)2);
  delay(10);
  int timerEvent  = t.every(13550, checkTimer, (void*)2);
  delay(10);
  int solarEvent  = t.every(3000, checkSolar, (void*)2);
  delay(10);
  int rgbEvent    = t.every(850, checkRGB, (void*)2);
  delay(10);
  int lampEvent   = t.every(10000, checkLamp, (void*)2);

  // Remove flickering by using highest PWM frequency (PIN usage is important!)
  setPwmFrequency(RED_PIN, 1); // 31 KHz
  setPwmFrequency(GREEN_PIN, 1); // 31 KHz
  setPwmFrequency(BLUE_PIN, 1); // 31 KHz

  tone(SPEAKER, NOTE_A6, 50);
  delay(55);
  tone(SPEAKER, NOTE_A7, 50);
  printlogHeader();
  Serial.println(F("################### Started ####################"));

}

// ############################################################################
// # Main loop
void loop() {
  int solarValueTmp = digitalRead(solarPin);
  buttonState = digitalRead(buttonPin);

solarValue = solarValueTmp;

// test
  if (solarValueTmp == false) {
    if (sVT > 5000) {
    
    }
    sVT++;
  } else {
     sVT = 0;
 }

  // Update timers
  t.update();
}

void checkLamp(void* context)
{
  // Turn Lamp on or off depending on lampState value

  if (Automatic) {
    // Led red/green state
    digitalWrite(RLB, !lampState); // Note the ! to invert it.
    //   delay(60);
    // Turn on/off lamp on relay A
    // This relay activates another relay (24volt rail) that switches a 240volt lamp.
    digitalWrite(RLA, !lampState); // Note the ! to invert it.
    //   delay(60);
    // RGB Power
    digitalWrite(RLC, !lampState); // Note the ! to invert it.
    //   delay(60);
  }

}

void checkRGB(void* context)
{
  if (!Automatic) {
    return;
  }
  rgbcounter++;
  redLevel = sin(rgbcounter / 100) * 1000;
  greenLevel = sin(rgbcounter / 100 + pi * 2 / 3) * 1000;
  blueLevel = sin(rgbcounter / 100 + pi * 4 / 3) * 1000;
  redLevel = map(redLevel, -1000, 1000, 0, RGB_BRIGHTNESS);
  greenLevel = map(greenLevel, -1000, 1000, 0, RGB_BRIGHTNESS);
  blueLevel = map(blueLevel, -1000, 1000, 0, RGB_BRIGHTNESS);
  setColourRgb(redLevel, greenLevel, blueLevel);
}

void checkSolar(void* context)
{
  if (!Automatic) {
    return;
  }

  printlogHeader();
  if (  !solarValue ) {
    Serial.println(F(" | LDR: DARK"));
  } else {
    Serial.println(F(" | LDR: LIGHT"));
  }

  if (  !solarValue ) { // LOW = DARK
    if (solarState) {
      if (!lampState) {
        printlogHeader();
        Serial.println(F(" | LCD -> LAMP ON "));
        tone(SPEAKER, NOTE_D7, 150);
        lampState = HIGH;
      }
    }
  }

  if (  solarValue ) { // HIGH = LIGHT
    solarState = 1; // Reset solar after the night
    
    if (lampState) {
      printlogHeader();
      Serial.println(F(" | LCD -> LAMP OFF "));
      lampState = LOW;
      tone(SPEAKER, NOTE_D5, 150);
    }
  }
}

void checkTimer(void* context)
{

  if (!lampState) {
    // when off then no timer needed.
    return;
  }
  DateTime now = rtc.now();
  int uur = now.hour();

  if (uur >= interval && uur <= 6 && Automatic) {
    printlogHeader();
    Serial.println(F(" | Time to turn the lamp off | "));

    if (lampState) {
      printlogHeader();
      Serial.println(F(" | Timer -> LAMP OFF "));
      lampState = LOW;
      solarState = 0;
      tone(SPEAKER, NOTE_G6, 50);
      delay(55);
      tone(SPEAKER, NOTE_G7, 50);
    }
  } else {
    solarState = 1; // reset solar
    sVT = 0; // reset counter see loop()
  }
}

void checkButton(void* context)
{

  if (!Automatic) {
    return;
  }

  if (buttonState != lastButtonState) {
    printlogHeader();
    Serial.print(F(" | Buttonstate: "));
    Serial.println(buttonState);

    if (  solarValue  ) {
      tone(SPEAKER, NOTE_C3, 150);
      // Don't allow to turn on light by button while it is light outside
      printlogHeader();
      Serial.println(F(" | Button is ignored by Solar state "));
      return;
    }

    if (buttonState) {
      if (lampState) {
        printlogHeader();
        Serial.println(F(" | Button -> LAMP OFF "));
        lampState = LOW;
      } else {
        printlogHeader();
        Serial.println(F(" | Button -> LAMP ON "));
        lampState = HIGH;
      }
    }
    lastButtonState = buttonState;
  }
}

// Set RGB Colors
void setColourRgb(unsigned int red, unsigned int green, unsigned int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
  printlogHeader();
  Serial.print(F(" | RGB change colors r,g,b: "));
  Serial.print(red);
  Serial.print(F(","));

  Serial.print(green);
  Serial.print(F(","));

  Serial.println(blue);

}

// PWM Frequency changer
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if (pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch (divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if (pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if (pin == 3 || pin == 11) {
    switch (divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x07; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}


// add leading zero to single digit
void print2digits(int xnumber) {
  if (xnumber >= 0 && xnumber < 10) {
    Serial.write('0');
  }
  Serial.print(xnumber);
}


// Log header (time)
void printlogHeader() {
  DateTime now = rtc.now();
  print2digits(now.hour());
  Serial.print(':');
  print2digits(now.minute());
  Serial.print(':');
  print2digits(now.second());
  Serial.print(" ");
}

// #### END ####
