extern unsigned long timer0_overflow_count; // ignore overflow errors from millis
// ^^- Needs on top according to some website
//
// Start of home automation system.
//
// Used: Small Solar panel, 4 relay module, RGB Strip, Dallas Temperature sensor
// Used: 2 leds - red and green, pushbutton, 4x MOSFET N-Channel
// Used: A few resistors, capacitors
// Used: A fan
// Used: A 8 Ohm small speaker
//
// Lamp/RGB strip goes on when Solar is below value x, stays on for x hours.
// Everything can be turned off manual by the pushbutton.
//
// ToDo: buy RTC clock module ( RTC clock module on its way :-) )
//
// Play: Playing with a airfan that switches on/off by temperature.
//
#include "Timer.h"              // Timer library - https://github.com/JChristensen/Timer
#include "pitches.h"            // Speaker notes - https://www.arduino.cc/en/Tutorial/toneMelody
#include <OneWire.h>            // I2C Bus wire (Standard lib)
#include <DallasTemperature.h>  // Temperature sensor (Standard lib)
#include <Wire.h>               // I2C Communication (Standard lib)


// ############################################################################
// # Speaker
#define SPEAKER 12 // Pin

// ############################################################################
// # I2C Slave ( Communication with/from Raspberry PI )
#define SLAVE_ADDRESS 0x04
int number = 0;
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
int tempMin = 23; // The temperature to start the fan
int tempMax = 30; // The temperature when it needs at full speed
String myTemp;
float temperatureInCelsius = 0;

// ############################################################################
// # RGB LED strip (5050)
#define RED_PIN 3     // Pin
#define GREEN_PIN 10  // Pin
#define BLUE_PIN 11   // Pin
unsigned int rgbColour[3];
int redLevel = 0;
int greenLevel = 0;
int blueLevel = 0;
float counter = 0;
float pi = 3.14159;

// ############################################################################
// # 4 Relay Module
#define RLA 8       // Pin Relay #1
#define RLB 4       // Pin Relay #2 -- on/off leds
#define RLC 6       // Pin Relay #3
#define RLD A1      // Pin Relay #4 on A1 as digitalpin


// ############################################################################
// # Solar (Small solar panel, about 1.5 volt orso)
const unsigned int solarPin   = A0; // Pin
int solarValue        = 0;
int solarState        = 0;
String solarLevel     = "";
const unsigned int solarHigh  = 100;  // Higher is lamp stays off
const unsigned int solarLow   = 90;   // Below is lamp on + timer start
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
long previousTime     = LOW;
unsigned long currentTime;
long interval = (60000 * 60) * 3;     // Turn off lamp/rgb after 3 hours

// ############################################################################
// # test time code
long p_mil;
long m;
int seconds;
int minutes;
int hours;

// ############################################################################
// # FAN ( Speed is controlled by temperature )
const unsigned int fanPin  = 5;   // Pin
int fanSpeed = 0;
int lastfanSpeed = 0;

// ############################################################################
// # Init timer library
Timer t;

// ############################################################################
// # Define space for dec2strf function
char str[16] = "...";

// ############################################################################
// # Setup
void setup() {
  // Start Serial
  Serial.begin(115200);

  // Play startup tones
  tone(SPEAKER, NOTE_C6, 150);
  delay(200);
  tone(SPEAKER, NOTE_D6, 150);
  delay(200);
  tone(SPEAKER, NOTE_C7, 150);
  delay(200);
  noTone(SPEAKER);

  // Set pin mode
  pinMode(13, OUTPUT);
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
  digitalWrite(13, LOW);  // Internal led off
  digitalWrite(RLA, HIGH);    // Relay off
  digitalWrite(RLB, HIGH);    // Relay off  -- High - RED LED // Low - GREEN LED
  digitalWrite(RLC, HIGH);    // Relay off
  digitalWrite(RLD, HIGH);    // Relay off
  analogWrite(fanPin, 0);     // FAN off
  setColourRgb(0, 0, 0);      // Turn off RGB

  // Temperature Sensor
  sensors.begin();
  sensors.getAddress(sensorDeviceAddress, 0);
  sensors.setResolution(sensorDeviceAddress, SENSOR_RESOLUTION);

  //Timers
  int buttonEvent = t.every(25, checkButton, (void*)2);
  int timerEvent  = t.every(10000, checkTimer, (void*)2);
  int solarEvent  = t.every(3000, checkSolar, (void*)2);
  int rgbEvent    = t.every(50, checkRGB, (void*)2);
  int lampEvent   = t.every(2000, checkLamp, (void*)2);
  int tempEvent   = t.every(750, checkTemp, (void*)2);

  // Remove flickering by using highest PWM frequency (PIN usage is important!)
  setPwmFrequency(RED_PIN, 1); // 31 KHz
  setPwmFrequency(GREEN_PIN, 1); // 31 KHz
  setPwmFrequency(BLUE_PIN, 1); // 31 KHz

  // Just set to default, if you change this then timers and milis() are messing up
  // I used for now a capacitor to remove the pitched tone
  setPwmFrequency(fanPin, 64); // 64 = default

  // Inter communication
  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);

  Serial.println("################### Started ####################");
}

// ############################################################################
// # Main loop
void loop() {
  // Update values
  currentTime = millis();
  int solar = analogRead(solarPin);
  buttonState = digitalRead(buttonPin);

  // Make solar value more stable
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

  // test time code - test time code - test time code
  m += currentTime - p_mil;
  // should work even when millis rolls over
  seconds += m / 1000;
  m = m % 1000;
  minutes += seconds / 60;
  seconds = seconds % 60;
  hours += minutes / 60;
  minutes = minutes % 60;
  p_mil = currentTime;
  // test time code - test time code - test time code
}

void checkTemp(void* context)
{

  sensors.requestTemperatures();
  temperatureInCelsius = sensors.getTempCByIndex(SENSOR_INDEX);

  myTemp = dec2strf((float)temperatureInCelsius, 1);

  fanSpeed = map(temperatureInCelsius, tempMin, tempMax, 108, 255); // Calculate FAN Speed

  if (fanSpeed > 255)
    fanSpeed = 255;

  Serial.print(" | Temperature: ");
  Serial.print(temperatureInCelsius, 4);
  Serial.println(" Celsius");

  if (temperatureInCelsius <= -127) {
    // This sensor sometimes, not often, shows -127.0000, so skip the rest of the code till
    // next cycle.
    return;
  }

  if ((temperatureInCelsius >= tempMin) && (temperatureInCelsius <= tempMax)) {
    digitalWrite(RLD, LOW); // Power the FAN by setting Relay D
    analogWrite(fanPin, fanSpeed); // Set FAN Speed
    Serial.print(F(" | FAN: Speed: "));
    Serial.println(fanSpeed);
    lastfanSpeed = fanSpeed;
  }

  if ( temperatureInCelsius > tempMax ) {
    analogWrite(fanPin, 255); // Set FAN at max Speed
    digitalWrite(RLD, LOW);
    Serial.print(F(" | FAN: Speed (max reached): "));
    Serial.println(fanSpeed);
  }

  if (temperatureInCelsius < tempMin) {
    analogWrite(fanPin, 0);
    digitalWrite(RLD, HIGH);
  }

}

void checkLamp(void* context)
{
  // Turn Lamp on or off depending on lampState value

  // Led red/green state
  digitalWrite(RLB, !lampState); // Note the ! to invert it.
  // Turn on/off lamp on relay A
  digitalWrite(RLA, !lampState); // Note the ! to invert it.
}

void checkRGB(void* context)
{
  if (lampState) {
    Serial.println(F(" | RGB ON: Change color"));

    counter = counter + 1;

    redLevel = sin(counter / 100) * 1000;
    greenLevel = sin(counter / 100 + pi * 2 / 3) * 1000;
    blueLevel = sin(counter / 100 + pi * 4 / 3) * 1000;
    redLevel = map(redLevel, -1000, 1000, 0, 100);
    greenLevel = map(greenLevel, -1000, 1000, 0, 100);
    blueLevel = map(blueLevel, -1000, 1000, 0, 100);
    setColourRgb(redLevel, greenLevel, blueLevel);
    
    Serial.print(F(" | RGB colors r,g,b: "));
    Serial.print(redLevel);
    Serial.print(F(","));
    
    Serial.print(greenLevel);
    Serial.print(F(","));

    Serial.println(blueLevel);
    
  } else {
    counter = 0;
    setColourRgb(0, 0, 0);
  }
}

void checkSolar(void* context)
{

  Serial.print(F(" | Solar: value: "));
  Serial.println(solarValue);

  if (  solarValue < solarLow ) {
    if (!solarState) {
      if (!lampState) {
        Serial.println(F(" | Solar -> LAMP ON "));
        tone(SPEAKER, NOTE_D7, 150);
        lampState = HIGH;
        resetTimer();
      }
    }
  }

  if (  solarValue > solarHigh ) { // high value (light outside)
    solarState = LOW ;
    if (lampState) {
      Serial.println(F(" | Solar -> LAMP OFF "));
      lampState = LOW;
      tone(SPEAKER, NOTE_D5, 150);
    }
  }
}

void checkTimer(void* context)
{

  Serial.print(F(" | Timer: Milis: "));
  Serial.print(currentTime);
  Serial.print(F(" | Timer: seconds left: "));
  Serial.print( (interval - ( currentTime - previousTime ) ) / 1000);
  Serial.print(F(" | Timer: Curr-Prev: "));
  Serial.print(currentTime - previousTime);
  Serial.print(F(" | Timer: Interval: "));
  Serial.println(interval);

  if (currentTime - previousTime > interval) {
    Serial.println(F(" | Timer hit the interval | "));

    if (lampState) {
      Serial.println(F(" | Timer -> LAMP OFF "));
      lampState = LOW;
      solarState = HIGH;
      tone(SPEAKER, NOTE_G6, 50);
      delay(55);
      tone(SPEAKER, NOTE_G7, 50);
    }

    previousTime = currentTime;
  }
}

void checkButton(void* context)
{
  if (buttonState != lastButtonState) {

    Serial.print(F(" | Buttonstate: "));
    Serial.println(buttonState);

    if (  solarValue > solarHigh ) {
      tone(SPEAKER, NOTE_C3, 150);
      // Don't allow to turn on light by button while it is light outside
      Serial.println(F(" | Button is ignored by Solar state "));
      return;
    }

    if (buttonState) {
      if (lampState) {
        Serial.println(F(" | Button -> LAMP OFF "));
        lampState = LOW;
        solarState = HIGH;
      } else {
        Serial.println(F(" | Button -> LAMP ON "));
        lampState = HIGH;
        resetTimer();
      }
    }
    lastButtonState = buttonState;
  }
}

void resetTimer(void)
{
  Serial.println(F(" | Reset Timer | "));
  previousTime = currentTime; // reset timer
}

void setColourRgb(unsigned int red, unsigned int green, unsigned int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
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
    number = Wire.read();
    Serial.print(" # Data RX: ");
    Serial.println(number);


    // Just testing for now, sending commands from a Raspberry PI over I2C
    if (number == 1) {
      if (state == 0) {
        digitalWrite(RLA, LOW);
        digitalWrite(RLB, LOW);
        digitalWrite(RLC, LOW);
        digitalWrite(RLD, LOW);
        state = 1;
      } else {
        digitalWrite(RLA, HIGH);
        digitalWrite(RLB, HIGH);
        digitalWrite(RLC, HIGH);
        digitalWrite(RLD, HIGH);
        state = 0;
      }
    }
    if (number == 2) {
      number = fanSpeed;
    }
    if (number == 3) {
      number = myTemp.toInt();
    }
  }
}

// I2C callback for sending data
void sendData() {
  Wire.write(number);
  Serial.print(" # Data TX: ");
  Serial.println(number);
}

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
