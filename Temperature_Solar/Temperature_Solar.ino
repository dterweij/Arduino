#include <OneWire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <Timer.h>

// ---------------------------------------------------------------------

Timer t;

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 0, 17);

IPAddress server_addr(192, 168, 0, 91); // IP of the MySQL *server* here
char user[] = "temperature";
char password[] = "justafakepassword";
EthernetClient client;
MySQL_Connection conn((Client *)&client);

char timeServer[] = "192.168.0.91";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE];
EthernetUDP Udp;

OneWire  ds(8); // pin 8

//int rood = 5;
//int geel = 6;

const unsigned int LEDGROEN = 7;
const unsigned long PERIOD1 = 500;    //one second
String UnixTime = "";
String celsius;

const int solarPin = A0;      // solar
int solarValue;
String solarLevel;


// Begin reboot code
int num_fails;                  // variable for number of failure attempts
#define MAX_FAILED_CONNECTS 5   // maximum number of failed connects to MySQL

void soft_reset() {
  asm volatile("jmp 0");
}
// End reboot code

// ----------------------------------------------------------------------

void setup() {
  Serial.begin(9600);

  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(LEDGROEN, OUTPUT);

  // heartbeat
  t.oscillate(LEDGROEN, PERIOD1, HIGH);


  digitalWrite(5, HIGH);

  Serial.print(F("Init IP:"));
  if (Ethernet.begin(mac) == 0) {
    Ethernet.begin(mac, ip);
  }
  Serial.println(Ethernet.localIP());

  Udp.begin(8888);

  int updateEthernetEvent = t.every(20000, updateEthernet, (void*)2);
  int syncUnixTimeEvent = t.every(5000, syncUnixTime, (void*)2);
  int writeMySQLEvent = t.every(10000, writeMySQL, (void*)2);
  int readSensorEvent = t.every(1000, readSensor, (void*)2);
  int solarEvent = t.every(1500, readSolar, (void*)2);
  int writesolarEvent = t.every(60000, writeSolar, (void*)2);

  digitalWrite(5, LOW);
  if (conn.connect(server_addr, 3306, user, password)) {
      delay(150);
    }
  Serial.println(F("Ready"));
}


void loop() {
  t.update();
}

void sendNTPpacket(char* address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void updateEthernet(void* context)
{
Ethernet.maintain();
}

void syncUnixTime(void* context)
{
  sendNTPpacket(timeServer);
  if (Udp.parsePacket()) {
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = (secsSince1900 - seventyYears) + 3600; // 3600 is GMT+1 --- 7200  is GMT+2
    UnixTime = String(epoch);
  }
}

void readSensor(void* context)
{

  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  byte i;

  if ( !ds.search(addr)) {
    ds.reset_search();
  //  digitalWrite(6, HIGH);
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    digitalWrite(6, HIGH);
    return;
  }

  switch (addr[0]) {
    case 0x10:
      //   Chip = DS18S20 or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //   Chip = DS18B20
      type_s = 0;
      break;
    case 0x22:
      //   Chip = DS1822
      type_s = 0;
      break;
    default:
  //    Serial.println(F("No DS18x20 fam."));
      digitalWrite(6, HIGH);
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
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7;
    else if (cfg == 0x20) raw = raw & ~3;
    else if (cfg == 0x40) raw = raw & ~1;
  }
  celsius = (float)raw / 16.0;
}

void writeMySQL(void* context)
{

 if (String(UnixTime) != "") {

  Serial.print(" Temp: ");
  Serial.print(celsius);
  Serial.print(" UnixTime: ");
  Serial.println(UnixTime);

if (conn.connected()) {
  digitalWrite(6, HIGH);

  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  String SQL = "UPDATE temperatuur.current SET value='" +  celsius + "', logtime='" + UnixTime + "' WHERE log_id='3'";
  int str_len = SQL.length() + 1;
  char query[str_len];
  SQL.toCharArray(query, str_len);
  cur_mem->execute(query);
  delete cur_mem;

  num_fails = 0;                 // reset failures
  } else {
    t.oscillate(5, 250, LOW, 50); // error
    if (conn.connect(server_addr, 3306, user, password)) {
      delay(500);
    } else {
      t.oscillate(5, 250, LOW, 50); // error
      num_fails++;
      if (num_fails == MAX_FAILED_CONNECTS) {
        delay(2000);
        soft_reset();
       }
     }
   } //
     digitalWrite(6, LOW);
 }
}


void writeSolar(void* context)
{

 if (String(UnixTime) != "") {

  Serial.print(" Solar: ");
  Serial.print(solarValue);
  Serial.print(" Opbrengst: ");
  Serial.println(solarLevel);

if (conn.connected()) {
  digitalWrite(6, HIGH);
  MySQL_Cursor *cur_mem2 = new MySQL_Cursor(&conn);
  String SQL2 = "INSERT INTO temperatuur.solar (value, date) VALUES('" +  solarLevel + "', '" + UnixTime + "')";
  int str_len2 = SQL2.length() + 1;
  char char_array2[str_len2];
  SQL2.toCharArray(char_array2, str_len2);
  cur_mem2->execute(char_array2);
  delete cur_mem2;
  num_fails = 0;                 // reset failures
  } else {
    t.oscillate(5, 250, LOW, 50); //error
    if (conn.connect(server_addr, 3306, user, password)) {
      delay(500);
    } else {
      t.oscillate(5, 250, LOW, 50); // error
      num_fails++;
      if (num_fails == MAX_FAILED_CONNECTS) {
        delay(2000);
        soft_reset();
       }
     }
   } //
     digitalWrite(6, LOW);

 }

}

void readSolar(void* context)
{
  solarValue = analogRead(solarPin);
  solarLevel = map(solarValue, 0,587, 0, 100);
}