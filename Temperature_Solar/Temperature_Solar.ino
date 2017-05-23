#include <OneWire.h>
#include <Ethernet.h>
#include <MySQL_Connection.h> //  https://github.com/copterino/MySQL_Connector_Arduino
#include <MySQL_Cursor.h>
#include <Timer.h>
#include <SevenSegmentTM1637.h>
// ---------------------------------------------------------------------
//
// Sketch may not exceed lower then 825 bytes for local variables, else Arduino uno v3 resets in error
// Current: Sketch - 28254 bytes used
// Global variables - 1213 bytes used
// Local variables - 835 bytes free (10 bytes remaining before it crashes!)
//
// Ethernet shield
//
// Define space for dec2strf function
char str[16] = "...";

// Display section
SevenSegmentTM1637 display(2, 3); // 4bit 7 segment module

// Timer
Timer t;        // timers

// Sensor
OneWire ds(8);  // temp sensor on pin 8

// Ethernet section
const byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 84);
IPAddress myDns(192, 168, 0, 91);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// MySQL Section
IPAddress server_addr(192, 168, 0, 91); // IP of the MySQL *server* here
const char user[] = "temperatuur";            // mysql user
const char password[] = "Poema001";           // mysql password
EthernetClient client;
MySQL_Connection conn((Client *)&client);

const unsigned int ROOD = 5;        // red led
const unsigned int GEEL = 6;        // yellow led
const unsigned int GROEN = 7;       // green led

const unsigned int ld1 = A3;       // led
const unsigned int ld2 = A4;       // led
const unsigned int ld3 = A5;       // led

const unsigned long PERIOD1 = 500;  // half second blink for heartbeat led
const unsigned int solarPin = A0;   // solar at analog pin A0

String celsius;                     // variable for temperature as string
String disptemp;                    // variable for temperature as integer for display
int solarValue;                     // variable for reading A0
String solarLevel;                  // variable for solar in procent
int num_fails;                      // variable for number of failure attempts
#define MAX_FAILED_CONNECTS 5       // maximum number of failed connects to MySQL
int wissel = true;                  // change display check


// ----------------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  Serial.println("");
  Serial.println(F("#### START ####"));

  display.begin();            // initializes the display
  display.setBacklight(100);  // set the brightness to 100 %
  display.clear();
  display.print("INIT");      // display INIT on the display

  pinMode(ROOD, OUTPUT);
  pinMode(GEEL, OUTPUT);
  pinMode(GROEN, OUTPUT);


  pinMode(ld1, OUTPUT);
  pinMode(ld2, OUTPUT);
  pinMode(ld3, OUTPUT);

  digitalWrite(ld1, HIGH);
  digitalWrite(ld2, HIGH);
  digitalWrite(ld3, HIGH);

  t.oscillate(GROEN, 75, LOW, 2);
  t.oscillate(GEEL, 75, LOW, 2);
  t.oscillate(ROOD, 75, LOW, 2);

  digitalWrite(GEEL, LOW);
  digitalWrite(GROEN, LOW);
  digitalWrite(ROOD, HIGH);

  Serial.print(F("Init IP: "));

  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  delay(500);
  display.blink();
  teller();

  Serial.println(Ethernet.localIP());

  // heartbeat
  t.oscillate(GROEN, PERIOD1, HIGH); // blinking green led

  // timers (max = 6)
  int updateEthernetEvent = t.every(30000, updateEthernet, (void*)2); // update ethernet stuff
  int writeMySQLEvent = t.every(60000, writeMySQL, (void*)2);         // write data to mysql
  int readSensorEvent = t.every(2500, readSensor, (void*)2);          // read temperature
  int solarEvent = t.every(3500, readSolar, (void*)2);                // read solar
  int writeDispEvent = t.every(6000, writeDisp, (void*)2);            // write temperature and solar to TM1637 display

  digitalWrite(ROOD, LOW);

  display.print("DONE");
  display.blink();
  display.setBacklight(50);
  display.clear();
  Serial.println(F("Ready"));
}

void loop() {
  t.update();
}

void updateEthernet(void* context)
{
  Serial.println(F("INFO: Update ethernet"));
  Ethernet.maintain();
}

void readSensor(void* context)
{
  Serial.print(F("INFO: Read Temp Sensor - "));
  t.oscillate(ld3, 50, LOW, 1);
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  byte i;

  ds.reset_search();

  if ( !ds.search(addr)) {
    ds.reset_search();
    t.oscillate(ROOD, 75, LOW, 30);
    Serial.println(F("SEARCH ERROR"));
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    t.oscillate(ROOD, 75, LOW, 30);
    Serial.println(F("CRC ERROR"));
    return;
  }

  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    default:
      digitalWrite(GEEL, HIGH);
      digitalWrite(ROOD, HIGH);
      Serial.println(F("NO SENSOR"));
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // reset chip

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  int16_t raw = (data[1] << 8) | data[0];
  raw = raw << 3;
  raw = (raw & 0xFFF0) + 12 - data[6];
  celsius = (float)raw / 16.0;
  disptemp = dec2strf((float)raw / 16.0, 1);
  disptemp.replace(".", "");
  Serial.println(celsius);
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

void writeDisp(void* context)
{
  t.oscillate(ld2, 50, LOW, 1);
  display.clear();
  display.setColonOn(0);

  if (wissel) {

    Serial.println(F("INFO: Update display C"));
    display.setColonOn(1);
    byte rawData = B01100011;
    display.print(disptemp);
    display.printRaw(rawData, 3);
    wissel = false;

  } else {

    Serial.println(F("INFO: Update display P"));
    int digitoneS = map(solarValue, 0, 587, 0, 100) / 10;
    int digittwoS = map(solarValue, 0, 587, 0, 100) % 10;
    int8_t datadisplay[] = {0x0, 0x0, 0x0, 0x0};
    datadisplay[0] = display.encode(digitoneS);
    datadisplay[1] = display.encode(digittwoS);
    datadisplay[3] = display.encode('P'); // P

    display.printRaw(datadisplay);
    wissel = true;

  }

}


void writeMySQL(void* context)
{
  Serial.println(F("INFO:      : Write Temp to MySQL"));

  Serial.print(F("# Temp     : "));
  Serial.println(celsius);
  Serial.print(F("# Solar    : "));
  Serial.println(solarValue);
  Serial.print(F("# Opbrengst: "));
  Serial.println(solarLevel);

  String SQL;
  int tmp_str_len;
  if (conn.connected()) {
    t.oscillate(GEEL, 75, LOW, 3);

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    SQL = "UPDATE temperatuur.current SET value='" +  celsius + "', logtime=UNIX_TIMESTAMP(now()) WHERE log_id='3'";

    tmp_str_len = SQL.length() + 1;
    char mquery[tmp_str_len];
    SQL.toCharArray(mquery, tmp_str_len);
    cur_mem->execute(mquery);

    SQL = "INSERT INTO temperatuur.solar (value, date) VALUES('" +  solarLevel + "', UNIX_TIMESTAMP(now())) ";
    tmp_str_len = SQL.length() + 1;
    char mmquery[tmp_str_len];
    SQL.toCharArray(mmquery, tmp_str_len);
    cur_mem->execute(mmquery);

    delete cur_mem;

    num_fails = 0;                 // reset failures
  } else {
    t.oscillate(ROOD, 250, LOW, 50); // error
    if (conn.connect(server_addr, 3306, user, password)) {
      delay(500);
    } else {
      t.oscillate(ROOD, 250, LOW, 50); // error
      num_fails++;
      if (num_fails == MAX_FAILED_CONNECTS) {
        Serial.println(F("ERROR: MySQL Failed!"));
        teller();
        soft_reset();
      }
    }
  } //
  t.oscillate(ROOD, 75, LOW, 3);
}

void readSolar(void* context)
{
  t.oscillate(ld1, 50, LOW, 1);
  Serial.print(F("INFO: Read Solar - "));
  solarValue = analogRead(solarPin);
  solarLevel = map(solarValue, 0, 587, 0, 100);
  Serial.println(solarLevel);
}

void soft_reset() {
  Serial.println("");
  Serial.println(F("INFO: RESET!"));
  display.print("BOOT");
  teller();
  asm volatile("jmp 0");
}

void teller() {
  display.clear();
  for ( int c = 20; c >= 0; c--) {
    display.print(c);
    delay(1000);
    display.clear();
  }
}
