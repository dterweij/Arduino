#include <OneWire.h>
#include <Ethernet.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <Timer.h>
#include <TM1637Display.h>
// ---------------------------------------------------------------------

// Display section
TM1637Display display(2, 3); // 4bit 7 segment module
const uint8_t SEG_DONE[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
  SEG_C | SEG_E | SEG_G,
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G
};                                                        // d 0 n E
const uint8_t SEG_READY[] = {SEG_D, SEG_D, SEG_D, SEG_D}; // _ _ _ _

Timer t;        // timers
OneWire ds(8);  // temp sensor on pin 8

// Ethernet section
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 84);
IPAddress myDns(192, 168, 0, 91);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// MySQL Section
IPAddress server_addr(192, 168, 0, 91); // IP of the MySQL *server* here
char user[] = "temperature";            // mysql user
char password[] = "justafakepassword";           // mysql password
EthernetClient client;
MySQL_Connection conn((Client *)&client);

const unsigned int ROOD = 5;        // red led
const unsigned int GEEL = 6;        // yellow led
const unsigned int GROEN = 7;       // green led
const unsigned long PERIOD1 = 500;  // half second blink for heartbeat led
const unsigned int solarPin = A0;   // solar at analog pin A0

String celsius;                     // variable for temperature as string
int disptemp;                       // variable for temperature as integer for display
int solarValue;                     // variable for reading A0
int disptemp;                       // variable for temperature as integer
String solarLevel;                  // variable for solar in procent
int num_fails;                      // variable for number of failure attempts
#define MAX_FAILED_CONNECTS 5       // maximum number of failed connects to MySQL
int wissel = true;                  // change display check

// ----------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println(F("#### START ####"));

  pinMode(ROOD, OUTPUT);
  pinMode(GEEL, OUTPUT);
  pinMode(GROEN, OUTPUT);

  t.oscillate(GROEN, 75, LOW, 2);
  t.oscillate(GEEL, 75, LOW, 2);
  t.oscillate(ROOD, 75, LOW, 2);

  digitalWrite(GEEL, LOW);
  digitalWrite(GROEN, LOW);
  digitalWrite(ROOD, HIGH);

  display.setBrightness(2);

  Serial.print(F("Init IP: "));

  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  teller();

  Serial.println(Ethernet.localIP());

  // heartbeat
  t.oscillate(GROEN, PERIOD1, HIGH);

  // timers (max = 6)
  int updateEthernetEvent = t.every(30000, updateEthernet, (void*)2);
  int writeMySQLEvent = t.every(10000, writeMySQL, (void*)2);
  int readSensorEvent = t.every(2500, readSensor, (void*)2);
  int solarEvent = t.every(2500, readSolar, (void*)2);
  int writesolarEvent = t.every(60000, writeSolar, (void*)2);
  int writeDispEvent = t.every(5000, writeDisp, (void*)2);

  digitalWrite(ROOD, LOW);

  display.setSegments(SEG_DONE);
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
  Serial.println(F("INFO: Read Temp Sensor"));

  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  byte i;

  if ( !ds.search(addr)) {
    ds.reset_search();
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    t.oscillate(ROOD, 75, LOW, 30);
    return;
  }

  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    default:
      digitalWrite(GEEL, HIGH);
      digitalWrite(ROOD, HIGH);
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
  if (type_s) {
    raw = raw << 3;
    if (data[7] == 0x10) {
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  celsius = (float)raw / 16.0;
  disptemp = (float)raw / 16.0;

}

void writeDisp(void* context)
{
  int8_t datadisplay[] = {0x0, 0x0, 0x0, 0x0};

  if (wissel) {

    Serial.println(F("INFO: Update display C"));
    int digitoneT = disptemp / 10;
    int digittwoT = disptemp % 10;

    datadisplay[0] = display.encodeDigit(digitoneT);
    datadisplay[1] = display.encodeDigit(digittwoT);
    datadisplay[3] = display.encodeDigit(12); // C

    display.setSegments(datadisplay);
    wissel = false;

  } else {

    Serial.println(F("INFO: Update display A"));
    int digitoneS = map(solarValue, 0, 587, 0, 100) / 10;
    int digittwoS = map(solarValue, 0, 587, 0, 100) % 10;

    datadisplay[0] = display.encodeDigit(digitoneS);
    datadisplay[1] = display.encodeDigit(digittwoS);
    datadisplay[3] = display.encodeDigit(10); // A

    display.setSegments(datadisplay);
    wissel = true;

  }

}


void writeMySQL(void* context)
{
  Serial.println(F("INFO:: Write Temp to MySQL"));

  Serial.print(F(" Temp: "));
  Serial.println(celsius);

  if (conn.connected()) {
    t.oscillate(GEEL, 75, LOW, 3);

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    String SQL = "UPDATE temperatuur.current SET value='" +  celsius + "', logtime=UNIX_TIMESTAMP(now()) WHERE log_id='3'";
    int str_len = SQL.length() + 1;
    char query[str_len];
    SQL.toCharArray(query, str_len);
    cur_mem->execute(query);
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


void writeSolar(void* context)
{

  Serial.println(F("INFO:: Write Solar to MySQL"));


  Serial.print(" Solar: ");
  Serial.print(solarValue);
  Serial.print(" Opbrengst: ");
  Serial.println(solarLevel);

  if (conn.connected()) {
    t.oscillate(GEEL, 75, LOW, 3);
    MySQL_Cursor *cur_mem2 = new MySQL_Cursor(&conn);
    String SQL2 = "INSERT INTO temperatuur.solar (value, date) VALUES('" +  solarLevel + "', UNIX_TIMESTAMP(now())) ";
    int str_len2 = SQL2.length() + 1;
    char char_array2[str_len2];
    SQL2.toCharArray(char_array2, str_len2);
    cur_mem2->execute(char_array2);
    delete cur_mem2;
    num_fails = 0;                 // reset failures
  } else {
    t.oscillate(ROOD, 250, LOW, 50); //error
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
  Serial.println(F("INFO: Read Solar"));
  solarValue = analogRead(solarPin);
  solarLevel = map(solarValue, 0, 587, 0, 100);
}

void soft_reset() {
  Serial.println("");
  Serial.println(F("INFO: RESET!"));
  teller();
  asm volatile("jmp 0");
}

void teller() {
  for ( int i = 5; i >= 0; i--) {
    display.showNumberDec(i, false, 4, 4);
    delay(1000);
  }
  display.setSegments(SEG_READY);
  delay(500);
}
