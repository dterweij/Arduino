extern unsigned long timer0_overflow_count; // ignore overflow errors from millis
// ^^- Needs on top according to some website
//
// Start of home automation system.
//
// Used: Small Solar panel, 4 relay module, RGB Strip, Dallas Temperature sensor
// Used: 2 leds - red and green, pushbutton, 4x MOSFET N-Channel
// Used: A few resistors
// Used: A fan
//
// Lamp/RGB strip goes on when Solar is below value x, stays on for x hours.
// Everything can be turned off manual by the pushbutton.
//
// ToDo: buy RTC clock module.
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
#define DATA_PIN 2
// How many bits to use for temperature values: 9, 10, 11 or 12
#define SENSOR_RESOLUTION 12
// Index of sensors connected to data pin, default: 0
#define SENSOR_INDEX 0
OneWire oneWire(DATA_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorDeviceAddress;
int tempMin = 20; // The temperature to start the fan
int tempMax = 40; // The temperature when it needs at full speed
int myTemp;

// ############################################################################
// # RGB LED strip (5050)
#define RED_PIN 3
#define GREEN_PIN 5
#define BLUE_PIN 6
int lr;
int lg;
int lb;
unsigned int rgbColour[3];

// ############################################################################
// # 4 Relay Module
#define RLA 8       // pin Relay #1
#define RLB 10      // pin Relay #2
#define RLC 11      // pin Relay #3
#define RLD A1      // pin Relay #4 on A1 as digitalpin

// ############################################################################
// # Status leds
const unsigned int ledPin     = 13; // pin - Green led
#define OFFLED 4                    // pin - Red led

// ############################################################################
// # Solar (Small solar panel, about 1.5 volt orso)
const unsigned int solarPin   = A0; // pin
int solarValue        = LOW;
int solarState        = LOW;
String solarLevel     = "";
const unsigned int solarHigh  = 120;  // Higher is lamp stays off
const unsigned int solarLow   = 90;   // Below is lamp on + timer start

// ############################################################################
// # Button (momentary)
const unsigned int buttonPin  = 7;  // pin
int buttonState       = LOW;
int lastButtonState   = LOW;
int buttonOff         = LOW;

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
unsigned long c_mil;
long p_mil;
long m;
int seconds;
int minutes;
int hours;

// ############################################################################
// # FAN ( Speed is controlled by temperature )
const unsigned int fanPin  = 9;   // pin
int fanSpeed = 0;
int lastfanSpeed = 0;

// ############################################################################
// # Init timer library
Timer t;

void setup() {
  // Start Serial
  Serial.begin(9600);

  // Play startup tones
  tone(SPEAKER, NOTE_C6, 150);
  delay(200);
  tone(SPEAKER, NOTE_D6, 150);
  delay(200);
  tone(SPEAKER, NOTE_C7, 150);
  delay(200);
  noTone(SPEAKER);

  // Set pin mode
  pinMode(OFFLED, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(RLA, OUTPUT);
  pinMode(RLB, OUTPUT);
  pinMode(RLC, OUTPUT);
  pinMode(RLD, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  // Set pin defaults
  digitalWrite(ledPin, LOW);  // Green OFF
  digitalWrite(OFFLED, HIGH); // Red ON
  digitalWrite(RLA, HIGH);    // Relay off
  digitalWrite(RLB, HIGH);    // Relay off
  digitalWrite(RLC, HIGH);    // Relay off
  digitalWrite(RLD, HIGH);    // Relay off
  analogWrite(fanPin, 0);     // FAN off
  setColourRgb(0, 0, 0);      // Turn off RGB

  // Temperature Sensor
  sensors.begin();
  sensors.getAddress(sensorDeviceAddress, 0);
  sensors.setResolution(sensorDeviceAddress, SENSOR_RESOLUTION);

  //Timers
  int buttonEvent = t.every(10, checkButton, (void*)2);
  int timerEvent  = t.every(2000, checkTimer, (void*)2);
  int solarEvent  = t.every(20000, checkSolar, (void*)2);
  int rgbEvent    = t.every(30000, checkRGB, (void*)2);
  int lampEvent   = t.every(5000, checkLamp, (void*)2);

  // Random seeder on empty not used pin
  randomSeed(analogRead(2)); // A2

  // Remove FAN noise by using lower PWM frequency
  setPwmFrequency(fanPin, 1024); // 31 Hz

  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);
}

void loop() {
  // Update values
  solarValue = analogRead(solarPin);
  solarLevel = map(solarValue, 0, 1023, 0, 100);
  currentTime = millis();
  buttonState = digitalRead(buttonPin);

  // Randomize RGB colors
  lr = random(0, 255);
  lg = random(0, 255);
  lb = random(0, 255);

  // Update timers
  t.update();

  // test time code - test time code - test time code
  c_mil = millis();
  m += c_mil - p_mil;
  // should work even when millis rolls over
  seconds += m / 1000;
  m = m % 1000;
  minutes += seconds / 60;
  seconds = seconds % 60;
  hours += minutes / 60;
  minutes = minutes % 60;
  p_mil = c_mil;
  // test time code - test time code - test time code
}

void checkLamp(void* context)
{
  // Turn Lamp on or off depending on lampState value
  digitalWrite(ledPin, lampState);
  digitalWrite(RLA, !lampState); // Note the ! to invert it.
}

void checkRGB(void* context)
{
  if (lampState) {
    Serial.println(F(" | RGB ON "));
    digitalWrite(OFFLED, LOW);
    setColourRgb(lr, lg, lb);
  } else {
    setColourRgb(0, 0, 0);
    digitalWrite(OFFLED, HIGH);
  }
}

void checkSolar(void* context)
{
  if (buttonOff == HIGH) {
    Serial.println(F(" | Solar function is OFF by Button set to OFF "));
    return;
  }

  Serial.print(F(" | Solar: "));
  Serial.print(solarValue);
  Serial.print(F(" | Opbrengst: "));
  Serial.println(solarLevel);

  if (  solarValue < solarLow ) {
    Serial.print(F(" | Solar: value is : "));
    Serial.println(solarValue);

    if (!solarState) {
      Serial.print(F(" | Solar: state is : "));
      Serial.println(solarState);

      if (!lampState) {
        Serial.print(F(" | Solar: lampState is : "));
        Serial.println(lampState);

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
  if (hours < 10) {
    Serial.print(F(" | Device Uptime: 0"));
  } else {
    Serial.print(F(" | Device Uptime: "));
  }
  Serial.print(hours);
  if (minutes < 10) {
    Serial.print(F(":0"));
  } else {
    Serial.print(F(":"));
  }
  Serial.print(minutes);
  if (seconds < 10) {
    Serial.print(F(":0"));
  } else {
    Serial.print(F(":"));
  }
  Serial.println(seconds);

  Serial.print(F(" | Timer: Milis: "));
  Serial.print(currentTime);
  Serial.print(F(" | Timer: seconds left: "));
  Serial.print( (interval - ( currentTime - previousTime ) ) / 1000);
  Serial.print(F(" | Timer: Curr-Prev: "));
  Serial.print(currentTime - previousTime);
  Serial.print(F(" | Timer: Interval: "));
  Serial.println(interval);

  sensors.requestTemperatures();
  float temperatureInCelsius = sensors.getTempCByIndex(SENSOR_INDEX);
  float temperatureInFahrenheit = sensors.getTempFByIndex(SENSOR_INDEX);
  myTemp = sensors.getTempCByIndex(SENSOR_INDEX);

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
    fanSpeed = map(temperatureInCelsius, tempMin, tempMax, 67, 255); // Calculate FAN Speed
    analogWrite(fanPin, fanSpeed); // Set FAN Speed
    Serial.print(F(" | FAN: Speed: "));
    Serial.println(fanSpeed);

    if (lastfanSpeed != fanSpeed) {
      // play note when FAN speed changes.
      //   tone(SPEAKER, NOTE_A7, 150);
    }
    lastfanSpeed = fanSpeed;
  }

  if (temperatureInCelsius < tempMin) {
    analogWrite(fanPin, 0);
    digitalWrite(RLD, HIGH);
  }

  if (currentTime - previousTime > interval) {
    Serial.println(F(" | Timer hit the interval | "));

    if (lampState) {
      Serial.println(F(" | Timer -> LAMP OFF "));
      lampState = LOW;
      solarState = HIGH;
    }

    if (buttonOff) {
      Serial.println(F(" | Timer: Solar function auto turned on "));
      buttonOff = LOW;
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
      // tone(SPEAKER, NOTE_C3, 150);
      // Don't allow to turn on light by button while it is light outside
      Serial.println(F(" | Button is ignored by Solar state "));
      return;
    }

    if (buttonState) {
      if (lampState) {
        Serial.println(F(" | Button -> LAMP OFF "));
        digitalWrite(OFFLED, HIGH);
        lampState = LOW;
        buttonOff = HIGH;
      } else {
        Serial.println(F(" | Button -> LAMP ON "));
        digitalWrite(OFFLED, LOW);
        lampState = HIGH;
        buttonOff = LOW;
        resetTimer();
      }
    }
    lastButtonState = buttonState;
  }
}

void resetTimer(void)
{
  Serial.println(F(" | Reset Timer | "));
  currentTime = millis();
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
      number = (int)myTemp;
    }
  }
}

// I2C callback for sending data
void sendData() {
  Wire.write(number);
  Serial.print(" # Data TX: ");
  Serial.println(number);
}
