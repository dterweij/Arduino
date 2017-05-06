extern unsigned long timer0_overflow_count; // ignore overflow errors from millis
// ^^- Needs on top according to some website
//
// Start of home automation system.
//
// Used: Small Solar panel, 4 relay module, RGB Strip, Dallas Temperature sensor
// Used: 2 leds - red and green, pushbutton, 3x MOSFET N-Channel
// Used: A few resistors
//
// Lamp/RGB strip goes on when Solar is below value x, stays on for x hours.
// Everything can be turned off manual by the pushbutton.
//
// ToDo: buy RTC clock module.
//
// Play: Playing with a airfan that switches on/off by temperature.
// 
#include "Timer.h"    // timer library
#include "pitches.h"  // speaker sound notes. speaker on pin 12
#include <OneWire.h>  // I2C Bus wire lib
#include <DallasTemperature.h> // Temperature sensor lib

// TempSensor input pin
#define DATA_PIN 2
// How many bits to use for temperature values: 9, 10, 11 or 12
#define SENSOR_RESOLUTION 12
// Index of sensors connected to data pin, default: 0
#define SENSOR_INDEX 0

OneWire oneWire(DATA_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorDeviceAddress;

// RGB
#define RED_PIN 3
#define GREEN_PIN 5
#define BLUE_PIN 6
//   # of milliseconds to pause between each pass of the loop
#define LOOP_DELAY 5

#define OFFLED 4    // pin
#define RLA 8      // pin Relay #1
#define RLB 10      // pin Relay #2
#define RLC 11      // pin Relay #3
#define RLD 19      // pin Relay #4 on A5 as digitalpin
const unsigned int buttonPin  = 7;  // pin
const unsigned int ledPin     = 13; // pin
const unsigned int solarPin   = A0; // pin
const unsigned int MotorPin  = 9;  // pin

int buttonState       = LOW;
int lastButtonState   = LOW;
int lampState         = LOW;
long previousTime     = LOW;
int solarValue        = LOW;
int solarState        = LOW;
String solarLevel     = "";
int buttonOff         = LOW;

const unsigned int solarHigh  = 100;  // higher is lamp stays off -- 100
const unsigned int solarLow   = 60;   // below is lamp on + timer start -- 50
unsigned long currentTime;
long interval = (60000 * 60) * 4;     // Turn off after 4 hours
unsigned int rgbColour[3];

//test
unsigned long c_mil;
long p_mil;
long m;
int seconds;
int minutes;
int hours;


// Init timer library
Timer t;

void setup() {
  tone(12, NOTE_C6, 150);
  delay(200);
  tone(12, NOTE_D6, 150);
  delay(200);
  tone(12, NOTE_C7, 150);
  delay(200);
  noTone(12);

  Serial.begin(9600);

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
  pinMode(MotorPin, OUTPUT);
  analogWrite(MotorPin, 55);
  pinMode(buttonPin, INPUT);

  // Set pin defaults
  digitalWrite(ledPin, LOW);
  digitalWrite(OFFLED, HIGH);
  digitalWrite(RLA, HIGH);
  digitalWrite(RLB, HIGH);
  digitalWrite(RLC, HIGH);
  digitalWrite(RLD, HIGH);

  setColourRgb(0, 0, 0);

  sensors.begin();
  sensors.getAddress(sensorDeviceAddress, 0);
  sensors.setResolution(sensorDeviceAddress, SENSOR_RESOLUTION);

  //Timers
  int buttonEvent = t.every(10, checkButton, (void*)2);
  int timerEvent  = t.every(2000, checkTimer, (void*)2);
  int solarEvent  = t.every(20000, checkSolar, (void*)2);
  int rgbEvent    = t.every(1800, checkRGB, (void*)2);
  int lampEvent   = t.every(10, checkLamp, (void*)2);
}

void loop() {
  // update values
  solarValue = analogRead(solarPin);
  solarLevel = map(solarValue, 0, 1023, 0, 100);
  currentTime = millis();
  buttonState = digitalRead(buttonPin);

  // update timers
  t.update();

  // test

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
}

void checkLamp(void* context)
{
  // Turn Lamp on or off depending on lampState value
  digitalWrite(ledPin, lampState);
  digitalWrite(RLA, !lampState); //use negative hope it works

}

void checkRGB(void* context)
{
  if (lampState) {
    Serial.println(F(" | RGB ON "));
    digitalWrite(OFFLED, LOW);

    analogWrite(RED_PIN, random(30));
    analogWrite(GREEN_PIN, random(30));
    analogWrite(BLUE_PIN, random(30));

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
        tone(12, NOTE_D7, 150);

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
      tone(12, NOTE_D5, 150);
    }
  }
}

void checkTimer(void* context)
{

  Serial.print(F(" | Device is running "));
  Serial.print(hours);
  Serial.print(F(":"));
  Serial.print(minutes);
  Serial.print(F(":"));
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
  // Measurement may take up to 750ms

  float temperatureInCelsius = sensors.getTempCByIndex(SENSOR_INDEX);
  float temperatureInFahrenheit = sensors.getTempFByIndex(SENSOR_INDEX);

  Serial.print(" | Temperature: ");
  Serial.print(temperatureInCelsius, 4);
  Serial.println(" Celsius");

  if (temperatureInCelsius > 25) {
    digitalWrite(RLD, LOW);
  } else {

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
      // tone(12, NOTE_C3, 150);
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
