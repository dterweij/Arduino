//
// Start of home automation system.
//
// - Solar going to be replaced by a LDR to see if it is more stable.
// - Temperature sensor going to be moved to another board and data is send via 433Mhz link
// - LDR Setup is build and in testing on another breadboard (analog to digital converter)
// \ LDR(GL5537),MOSFET(IRFZ44N),NAND IC(CD4093BE),Transistor(BC560C),3 resistors.
//
// Used: Small Solar panel, 4 relay module, RGB Strip, Dallas Temperature sensor
// Used: 2 leds - red and green, pushbutton, 4x MOSFET N-Channel (IRFZ44N)
// Used: A few resistors, capacitors
// Used: A fan
// Used: A 8 Ohm small speaker
// Used: LM358 OPAMP + low pass filter,
// - as driver to the MOSFET that controls FAN Speed to remove high pitch noise
// Used: Tiny RTC DS1307 Clock module
// Used: Using 433Mhz receiver (MX-RM-5V)
//
// Lamp/RGB strip goes on when Solar is below value x, stays on till x hour.
// Everything can be turned off manual by the pushbutton.
//
// I2C communication from Raspbery PI B+ (Raspbian OS) to send commands (No ttl regulator (3.3v/5v) module used!)
// Still as playground, it works well. Going to see if I can make a webpage for controlling.
// - I2C connection is broken. Because the TTL diffence, now that the RTC is in use.
//
// Play: Playing with a airfan that switches on/off by temperature.
// - Works well. Might be permanent to cool the Arduino and MOSFETS when cased.
//
#include "Timer.h"              // Timer library - https://github.com/JChristensen/Timer
#include "pitches.h"            // Speaker notes - https://www.arduino.cc/en/Tutorial/toneMelody
#include <OneWire.h>            // I2C Bus onewire (Standard lib)
#include <DallasTemperature.h>  // Temperature sensor (Standard lib)
#include <Wire.h>               // I2C Communication (Standard lib)
#include "RTClib.h"             // The RTC Module (Standard lib)
#include <RH_ASK.h>             // RF 433Mhz driver - http://www.airspayce.com/mikem/arduino/RadioHead/

#define DEBUG 1 // Set debug mode (Serial Log: 1 = enabled / 2 = disabled)

// ############################################################################
// # RF 433Mhz driver (Speed,rx,tx,ptt)
RH_ASK driver(1000, 9, A1, A2); // Speed 100, RX = pin 9. Using unused pins for the rest

// ############################################################################
// # RTC Clock Module
RTC_DS1307 rtc; // hardware RTC

// ############################################################################
// # Automatic process
int Automatic = 1;

// ############################################################################
// # Speaker
#define SPEAKER 12 // Pin

// ############################################################################
// # I2C Slave ( Communication with/from Raspberry PI )
#define SLAVE_ADDRESS 0x04
int DataIn = 0;
int DataOut = 0;
int state = 0;

// ############################################################################
// # Temperature Sensor DS18B20
#define DATA_PIN 2  // Pin
// How many bits to use for temperature values: 9, 10, 11 or 12
#define SENSOR_RESOLUTION 12
// Index of sensors connected to data pin, default: 0
#define SENSOR_INDEX 0
OneWire oneWire(DATA_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorDeviceAddress;
// FAN speed controlled between Min and Max, above max is full speed
int tempMin = 22.5; // The temperature to start the fan
int tempMax = 30; // The temperature when it needs at full speed
String myTemp;
float temperatureInCelsius = 0;

// ############################################################################
// # RGB LED strip (5050)
#define RED_PIN 3     // Pin
#define GREEN_PIN 10  // Pin
#define BLUE_PIN 11   // Pin
#define RGB_BRIGHTNESS 255     // Brightness leds
unsigned int rgbColour[3];
int redLevel = 0;
int greenLevel = 0;
int blueLevel = 0;
float counter = 0;
float pi = 3.14159;

// ############################################################################
// # 4 Relay Module
#define RLA 8       // Pin Relay #1 -- FAN power on/off
#define RLB 4       // Pin Relay #2 -- Power to on/off leds
#define RLC 6       // Pin Relay #3 -- RGB power on/off
#define RLD 13      // Pin Relay #4 -- To Lamp relay (240Volt)


// ############################################################################
// # Solar (Small solar panel, about 1.5 volt orso)
const unsigned int solarPin   = A0;   // Pin
int solarValue        = 0;
int solarState        = 1;
String solarLevel     = "";
const unsigned int solarHigh  = 75;   // Higher is lamp stays off (Also a buffer)
const unsigned int solarLow   = 70;   // Below is lamp on + timer start
int solarCnt          = 1;
int solarTmp          = 0;

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
// # FAN ( Speed is controlled by temperature )
const unsigned int fanPin  = 5;   // Pin
int fanSpeed = 0;
int lastfanSpeed = 0;
int fanOff = tempMin ;            // Turn off fan below X degrees

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
  Serial.println("Start up");
  // Start Serial
  Serial.begin(9600);

  if (!driver.init())
    Serial.println("ERROR: 433Mhz receiver not found");

  if (! rtc.begin()) {
    printlogHeader();
    Serial.println("ERROR: Couldn't find RTC");
  }
  if (! rtc.isrunning()) {
    printlogHeader();
    Serial.println("RTC is NOT running!");
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
  pinMode(fanPin, OUTPUT);
  pinMode(buttonPin, INPUT);


  // Set pin defaults
  digitalWrite(RLA, HIGH);    // Relay off
  digitalWrite(RLB, HIGH);    // Relay off  -- High - RED LED // Low - GREEN LED
  digitalWrite(RLC, HIGH);    // Relay off
  digitalWrite(RLD, HIGH);    // Relay off
  analogWrite(fanPin, 0);     // FAN off

  // Relay test
  printlogHeader();
  Serial.println("Relay Test");
  delay(300);
  digitalWrite(RLA, LOW);
  delay(300);
  digitalWrite(RLB, LOW);
  delay(300);
  digitalWrite(RLC, LOW);
  delay(300);
  digitalWrite(RLD, LOW);
  delay(300);

  digitalWrite(RLA, HIGH);
  delay(300);
  digitalWrite(RLB, HIGH);
  delay(300);
  digitalWrite(RLC, HIGH);
  delay(300);
  digitalWrite(RLD, HIGH);
  delay(300);
  //
  digitalWrite(RLD, LOW); // Power the FAN by setting Relay D
  printlogHeader();
  Serial.println("Fan Test");

  for ( int f = 0; f <= 255; f++) {
    analogWrite(fanPin, f);     // FAN test
    delay(4);
  }
  delay(1500);
  for ( int f = 255; f >= 0; f--) {
    analogWrite(fanPin, f);     // FAN test
    delay(4);
  }

  delay(1500);
  digitalWrite(RLD, HIGH); // Power the FAN by setting Relay D

  analogWrite(fanPin, 0);     // FAN off
  printlogHeader();
  Serial.println("RGB Test");
  digitalWrite(RLC, LOW); // Power on
  setColourRgb(0, 0, 0);      // Turn off RGB OFF

  // Test leds
  setColourRgb(255, 0, 0);    // RGB Red
  delay(1500);
  setColourRgb(0, 255, 0);    // RGB Green
  delay(1500);
  setColourRgb(0, 0, 255);    // RGB Blue
  delay(1500);

  setColourRgb(0, 0, 0);      // Turn off RGB OFF
  digitalWrite(RLC, HIGH); // Power off

  // Temperature Sensor
  sensors.begin();
  sensors.getAddress(sensorDeviceAddress, 0);
  sensors.setResolution(sensorDeviceAddress, SENSOR_RESOLUTION);

  //Timers
  int buttonEvent = t.every(25, checkButton, (void*)2);
  int timerEvent  = t.every(13550, checkTimer, (void*)2);
  int solarEvent  = t.every(3000, checkSolar, (void*)2);
  int rgbEvent    = t.every(2850, checkRGB, (void*)2);
  int lampEvent   = t.every(10000, checkLamp, (void*)2);
  int tempEvent   = t.every(4750, checkTemp, (void*)2);

  // Remove flickering by using highest PWM frequency (PIN usage is important!)
  setPwmFrequency(RED_PIN, 1); // 31 KHz
  setPwmFrequency(GREEN_PIN, 1); // 31 KHz
  setPwmFrequency(BLUE_PIN, 1); // 31 KHz

  // Just set to default, if you change this then timers and milis() are messing up
  setPwmFrequency(fanPin, 64); // 64 = default

  // Inter communication
  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);

  tone(SPEAKER, NOTE_A6, 50);
  delay(55);
  tone(SPEAKER, NOTE_A7, 50);
  printlogHeader();
  Serial.println("################### Started ####################");

}

// ############################################################################
// # Main loop
void loop() {
  int solar = analogRead(solarPin);
  buttonState = digitalRead(buttonPin);

  // Make solar value more stable
  // Added also a 6,8uF Capacitor as buffer for suddenly voltage changes
  if (solarCnt == 50) {
    solarValue = solarTmp / 50;
    solarTmp = 0;
    solarCnt = 1;
  } else {
    solarTmp = solarTmp + solar;
    solarCnt++;
  }

  // Update timers
  t.update();

  //
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) // Non-blocking
  {
    int xi;

    printlogHeader();
    Serial.print(" # RX: ");
    for (xi = 0; xi < buflen; xi++)
    {
      Serial.write(buf[xi]);
    }
    Serial.println();
  }

}

void checkTemp(void* context)
{
  if (!Automatic) {
    return;
  }

  sensors.requestTemperatures();
  temperatureInCelsius = sensors.getTempCByIndex(SENSOR_INDEX);

  myTemp = dec2strf((float)temperatureInCelsius, 1);

  fanSpeed = map(temperatureInCelsius, tempMin, tempMax, 100, 255); // Calculate FAN Speed
  int fanSpeedP = map(fanSpeed, 100, 255, 0, 100); // Calculate FAN Speed procent

  if (fanSpeed > 255)
    fanSpeed = 255;

  printlogHeader();
  Serial.print(" | Temperature: ");
  Serial.print(temperatureInCelsius, 4);
  Serial.println(" Celsius");

  if (temperatureInCelsius <= -127) {
    // This sensor sometimes, not often, shows -127.0000, so skip the rest of the code till
    // next cycle.
    return;
  }

  if ((temperatureInCelsius >= tempMin) && (temperatureInCelsius <= tempMax)) {
    digitalWrite(RLD, LOW);         // Power the FAN by setting Relay D
    analogWrite(fanPin, fanSpeed);  // Set FAN Speed
    printlogHeader();
    Serial.print(F(" | FAN Speed: "));
    Serial.print(fanSpeedP);
    Serial.println(F("%"));

    lastfanSpeed = fanSpeed;
  }

  if ( temperatureInCelsius > tempMax ) {
    analogWrite(fanPin, 255); // Set FAN at max Speed
    digitalWrite(RLD, LOW);
    printlogHeader();
    Serial.print(F(" | FAN: Speed (max reached): "));
    Serial.println(fanSpeed);
  }

  if (temperatureInCelsius < fanOff) {
    analogWrite(fanPin, 0);
    digitalWrite(RLD, HIGH);
  }

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

  counter = counter + 1;

  redLevel = sin(counter / 100) * 1000;
  greenLevel = sin(counter / 100 + pi * 2 / 3) * 1000;
  blueLevel = sin(counter / 100 + pi * 4 / 3) * 1000;
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
  Serial.print(F(" | Solar: value: "));
  Serial.println(solarValue);

  if (  solarValue < solarLow ) {
    if (solarState) {
      if (!lampState) {
        printlogHeader();
        Serial.println(F(" | Solar -> LAMP ON "));
        tone(SPEAKER, NOTE_D7, 150);
        lampState = HIGH;
      }
    }
  }

  if (  solarValue > solarHigh ) { // high value (light outside)
    solarState = 1; // Reset solar after the night
    if (lampState) {
      printlogHeader();
      Serial.println(F(" | Solar -> LAMP OFF "));
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

    if (  solarValue > solarHigh ) {
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

// I2C callback for received data
void receiveData(int byteCount) {

  while (Wire.available()) {
    DataIn = Wire.read();
    printlogHeader();
    Serial.print(" # Data RX: ");
    Serial.println(DataIn);


    // Just testing for now, sending commands from a Raspberry PI over I2C

    // # 1 -- Switch to Auto or Manual mode
    if (DataIn == 1) {
      if (state == 0) {
        Automatic = 0;
        state = 1;
        DataOut = 100;
        printlogHeader();
        Serial.println(" * Auto mode: OFF");
      } else {
        Automatic = 1;
        state = 0;
        DataOut = 101;
        printlogHeader();
        Serial.println(" * Auto mode: ON");
      }
    }

    // # 2 -- Fan speed
    if (DataIn == 2) {
      DataOut = fanSpeed;
    }

    // # 3 -- Temperature (first part, not the decimals)
    if (DataIn == 3) {
      DataOut = myTemp.toInt();
    }

    // # 4 -- Change RGB color
    if (DataIn == 4 && !Automatic) {
      DataOut = 1;
      counter = counter + 1;

      redLevel = sin(counter / 100) * 1000;
      greenLevel = sin(counter / 100 + pi * 2 / 3) * 1000;
      blueLevel = sin(counter / 100 + pi * 4 / 3) * 1000;
      redLevel = map(redLevel, -1000, 1000, 0, RGB_BRIGHTNESS);
      greenLevel = map(greenLevel, -1000, 1000, 0, RGB_BRIGHTNESS);
      blueLevel = map(blueLevel, -1000, 1000, 0, RGB_BRIGHTNESS);
      setColourRgb(redLevel, greenLevel, blueLevel);

      printlogHeader();
      Serial.print(F(" | RGB (Manual) change colors r,g,b: "));
      Serial.print(redLevel);
      Serial.print(F(","));

      Serial.print(greenLevel);
      Serial.print(F(","));

      Serial.println(blueLevel);

    }

    // # 5 -- All Off
    if (DataIn == 5 && !Automatic) {
      DataOut = 1;
      digitalWrite(RLA, HIGH);    // Relay off
      digitalWrite(RLB, HIGH);    // Relay off  -- High - RED LED // Low - GREEN LED
      digitalWrite(RLC, HIGH);    // Relay off
      digitalWrite(RLD, HIGH);    // Relay off
      analogWrite(fanPin, 0);     // FAN off
      setColourRgb(0, 0, 0);
    }
    // # 255 --
    if (DataIn == 255) {
      DataOut = 1;
    }
  }
}

// I2C callback for sending data
void sendData() {
  Wire.write(DataOut);
  printlogHeader();
  Serial.print(" # Data TX: ");
  Serial.println(DataOut);
  DataOut = 0; // reset
}

// Decimal to String converter
char* dec2strf(float floatNumber, uint8_t dp)
{
  uint8_t ptr = 0;            // Initialise pointer for array
  uint8_t  digits = 1;         // Count the digits to avoid array overflow
  float rounding = 0.5;       // Round up down delta

  if (dp > 7) dp = 7; // Limit the size of decimal portion

  // Adjust the rounding value
  for (uint8_t i = 0; i < dp; ++i) rounding /= 10.0;

  if (floatNumber < -rounding)    // add sign, avoid adding - sign to 0.0!
  {
    str[ptr++] = '-'; // Negative number
    str[ptr] = 0; // Put a null in the array as a precaution
    digits = 0;   // Set digits to 0 to compensate so pointer value can be used later
    floatNumber = -floatNumber; // Make positive
  }

  floatNumber += rounding; // Round up or down

  // For overflow put ... in string and return (Nb. all TFT_ILI9341 library fonts contain . character)
  if (floatNumber >= 2147483647) {
    strcpy(str, "...");
    return str;
  }
  // No chance of number overflow from here on

  // Get integer part
  unsigned long temp = (unsigned long)floatNumber;

  // Put integer part into array
  ltoa(temp, str + ptr, 10);

  // Find out where the null is to get the digit count loaded
  while ((uint8_t)str[ptr] != 0) ptr++; // Move the pointer along
  digits += ptr;                        // Count the digits

  str[ptr++] = '.';   // Add decimal point and increment pointer
  str[ptr] = '0';   // Add a dummy zero
  str[ptr + 1] = 0; // Add a null but don't increment pointer so it can be overwritten

  // Get the decimal portion
  floatNumber = floatNumber - temp;

  // Get decimal digits one by one and put in array
  // Limit digit count so we don't get a false sense of resolution
  uint8_t i = 0;
  while ((i < dp) && (digits < 9)) // while (i < dp) for no limit but array size must be increased
  {
    i++;
    floatNumber *= 10;       // for the next decimal
    temp = floatNumber;      // get the decimal
    ltoa(temp, str + ptr, 10);
    ptr++; digits++;         // Increment pointer and digits count
    floatNumber -= temp;     // Remove that digit
  }

  // Finally we can return a pointer to the string
  return str;
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
