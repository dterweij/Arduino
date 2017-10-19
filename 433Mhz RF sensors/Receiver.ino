//
// Arduino Uno
//
#include <RH_ASK.h>
#include <SPI.h>
#include "RTClib.h"             // The RTC Module (Standard lib)
RTC_Millis rtc; // software RTC

RH_ASK driver(1000, 9, A1, A2);

void setup()
{
  Serial.begin(9600);	// Debugging only
  Serial.println("433Mhz Receiver started.");
  if (!driver.init())
    Serial.println("433Mhz module failed");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop()
{
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen))
  {
    digitalWrite(LED_BUILTIN, HIGH);
    int xi;
    String tempIn;
    String message;

    for (xi = 0; xi < buflen; xi++)
    {
      tempIn += char(buf[xi]);
    }

    int firstCommaIndex = tempIn.indexOf(',');
    int secondCommaIndex = tempIn.indexOf(',', firstCommaIndex + 1);
 
    String cmd = tempIn.substring(0, firstCommaIndex);
    String param1 = tempIn.substring(firstCommaIndex + 1, secondCommaIndex);
    String param2 = tempIn.substring(secondCommaIndex + 1);
   
    if (cmd == "T"); {
      DateTime now = rtc.now();
      print2digits(now.hour());
      Serial.print(':');
      print2digits(now.minute());
      Serial.print(':');
      print2digits(now.second());
      Serial.println(" -->");

      message = "T," + param1 + "," + param2;
      Serial.println(message);
    }
  }
  digitalWrite(LED_BUILTIN, LOW);
}

// add leading zero to single digit
void print2digits(int xnumber) {
  if (xnumber >= 0 && xnumber < 10) {
    Serial.write('0');
  }
  Serial.print(xnumber);
}
