
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClibExtended.h>
#include <GSM.h>
#include <EEPROM.h>
#include <ctype.h>
#include <Wire.h>
#include <avr/sleep.h>

#define PINNUMBER "6029"

#define GPRS_APN       "internet.proximus.be" // replace your GPRS APN
#define GPRS_LOGIN     ""    // replace with your GPRS login
#define GPRS_PASSWORD  "" // replace with your GPRS password

#define wakePin 2
#define ledPin 13

OneWire oneWire(4);
DallasTemperature tSensor(&oneWire);
/*
  // this code is for sending sms
  GSM gsmAccess;
  GSM_SMS sms;

  char remoteNumber[14]= "01234567890";  // sms enabled telephone number
*/

GSMClient client;
GPRS gprs;
GSM gsmAccess;

char server[] = "172.104.242.113";
char path[] = "/api/v1/reading";
int port = 80; // port 80 is the default for HTTP

RTC_DS3231 RTC;

/*
  float doReading = "";
  boolean do_reading_complete = false;
*/

// FAKE a DO reading as the DO sensor is no longer connected to the arduino platform
float doReading = 11.11;
boolean do_reading_complete = true;

int NUMBER_OF_READINGS_TO_SAVE_BEFORE_SENDING = 5;

// SYSTEEM
void setup() {

  //setup connection to console
  Serial.begin(9600);
  Serial.println("Start connectie met console");

  pinMode(wakePin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  delay(1000);
  Serial.println("Start het sleep mechanisme");

  RTC.begin();
  RTC.adjust(1388534400);
  Serial.println("Setting the RTC to Jan 1, 2014 00:00:00");

  RTC.armAlarm(1, false);
  RTC.clearAlarm(1);
  RTC.alarmInterrupt(1, false);
  RTC.armAlarm(2, false);
  RTC.clearAlarm(2);
  RTC.alarmInterrupt(2, false);
  RTC.writeSqwPinMode(DS3231_OFF);

  setAlarm();

}

void loop() {

  if ( do_reading_complete == true) {
    int number = getNumberOfStoredReadings();
    saveReadings ( number, RTC.now().unixtime(), doReading, getTurbidity(), getTemperature() );
    if ( number >= NUMBER_OF_READINGS_TO_SAVE_BEFORE_SENDING ) {
      sendSaveReadings();
    }
    //    do_reading_complete = false;

    sleepNow();
    setupOnAwakening();
    requestReadingFromDO();

  }

}

// ----- beware of the methods below ;-)

void setupOnAwakening() {

  Serial.println ("Configuring sensors");
  //setup connection to do sensor
  Serial3.begin(9600);
  Serial.println("Start connectie met DO sensor");

  //setup connection to temperatur sensor
  Serial.println("Start connectie met temperatuur sensor");
  tSensor.begin();

  //tell DO sensor to take a reading
  requestReadingFromDO();
}

boolean SMS_INITED = false;
void setupSMS() {

  if ( SMS_INITED == false ) {
    setupSMS();
  }

  Serial.println("Start verbinding met GSM module");
  boolean notConnected = true;
  while (notConnected) {
    Serial.print (".");
    if (gsmAccess.begin(PINNUMBER) == GSM_READY) {
      notConnected = false;
    } else {
      delay(1000);
    }
  }
  Serial.println ("!");
  Serial.println("Verbinding met GSM module klaar");
}

void setupGPRS() {
  Serial.println("Opstarten van internet verbinding");
  // connection state
  boolean notConnected = true;

  // After starting the modem with GSM.begin()
  // attach the shield to the GPRS network with the APN, login and password
  while (notConnected) {
    if ((gsmAccess.begin(PINNUMBER) == GSM_READY) &
        (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD) == GPRS_READY)) {
      notConnected = false;
    } else {
      Serial.println("Not connected");
      delay(1000);
    }
  }
  Serial.println("Verbinding met het internet gemaakt");
}

void requestReadingFromDO() {
  Serial.println("Vraag reading aan DO module");
  Serial3.print ( "R" );
  Serial3.print ( "\r" );
}

void serialEvent3() {                                 //if the hardware serial port_3 receives a char
  String sensorstring = Serial3.readStringUntil(13);         //read the string until we see a <CR>
  if ( isValidNumber ( sensorstring ) ) {
    doReading = sensorstring.toFloat();
  } else {
    requestReadingFromDO();
  }

  do_reading_complete = true;                      //set the flag used to tell if we have received a completed string from the PC
}

int getNumberOfStoredReadings() {
  return EEPROM.read(0);
}

void saveReadings ( int numberOfStoredReadings, long samplingTimestamp, float doreadingAsFloat, float turbidityAsFloat, float temperatureAsFloat ) {

  EEPROM.write ( 0, numberOfStoredReadings + 1 );

  Serial.print ( "Wegschrijven van " );
  Serial.print ( numberOfStoredReadings + 1 );
  Serial.print ( ", samplingTimestamp ");
  Serial.print (samplingTimestamp);
  Serial.print ( ", doreadingAsFloat ");
  Serial.print (doreadingAsFloat);
  Serial.print ( ", turbidityAsFloat ");
  Serial.print (turbidityAsFloat);
  Serial.print ( ", temperatureAsFloat ");
  Serial.println (temperatureAsFloat);

  EEPROM_writeLong ( 1 + ( 4 * 5 * numberOfStoredReadings ), samplingTimestamp );
  EEPROM_writeDouble ( 5 + ( 4 * 5 * numberOfStoredReadings ), doreadingAsFloat );
  EEPROM_writeDouble ( 9 + ( 4 * 5 * numberOfStoredReadings ), turbidityAsFloat );
  EEPROM_writeDouble ( 13 + ( 4 * 5 * numberOfStoredReadings ), temperatureAsFloat );

}

void sendSaveReadings() {
  String message = calculateMessageToSend();
  Serial.println ( message );
  sendMessageOverGPRS ( message );
  EEPROM.write ( 0, 0 );
}

String calculateMessageToSend() {

  int number = getNumberOfStoredReadings();
  String data = "";
  char buffer[10];
  for ( int i = 0; i < number; i ++ ) {

    long samplingTimestamp = EEPROM_readLong ( 1 + ( 4 * 5 * i ) );
    float doreadingAsFloat = EEPROM_readDouble ( 5 + ( 4 * 5 * i ) );
    float turbidityAsFloat = EEPROM_readDouble ( 9 + ( 4 * 5 * i ) );
    float temperatureAsFloat = EEPROM_readDouble ( 13 + ( 4 * 5 * i ) );

    String doreading = dtostrf(doreadingAsFloat, 5, 2, buffer);
    String turbidity = dtostrf(turbidityAsFloat, 5, 2, buffer);
    String temperatuur = dtostrf(temperatureAsFloat, 5, 2, buffer);
    String timestamp = dtostrf(samplingTimestamp, 10, 0, buffer);

    if ( data.length() > 0 ) {
      data += ",";
    }

    data += "{ \"disolvedOxygen\": " + doreading
            + ", \"turbidity\": " + turbidity
            + ", \"temperature\": " + temperatuur
            + ", \"samplingTimestamp\": " +  timestamp + "}";
  }

  String timestamp = dtostrf(RTC.now().unixtime(), 10, 0, buffer);

  return
    "{ \"timestamp\": " + timestamp + ", "
    + "\"name\": \"Arduino1\", "
    + "\"type\": \"FIXED\", "
    + "\"data\": [ "
    + data
    + " ] }"
    ;
}

float getTemperature() {
  tSensor.requestTemperatures();
  return tSensor.getTempCByIndex(0);
}

float getTurbidity() {
  return analogRead(A0) * (5.0 / 1024.0);
}

boolean isValidNumber(String str) {
  for (byte i = 0; i < str.length(); i++) {
    if (!isDigit(str.charAt(i)) && str.charAt(i) != '.' ) {
      return false;
    }
  }
  return true;
}

/*
  void sendSMS ( String message )  {
    message.toCharArray(txtMsg, 200);
    sms.beginSMS(remoteNumber);
    sms.print(txtMsg);
    sms.endSMS();
  }*/

boolean GRPS_INITED = false;

void sendMessageOverGPRS ( String message ) {

  if ( GRPS_INITED == false ) {
    setupGPRS();
  }

  if (client.connect(server, port)) {
    Serial.println("connected");

    client.print("POST ");
    client.print(path);
    client.println(" HTTP/1.1");

    client.print("Host: ");
    client.println(server);

    client.println("Content-Type: application/json; charset=utf-8");

    client.print("Content-Length: ");
    client.println(message.length() + 2);

    client.println("");
    client.println(message);

    client.stop();

    Serial.println("message send");

  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
}

void sleepNow() {
  Serial.println ( "We gaan slapen");
  delay(1000);
  attachInterrupt(0, wakeUp, LOW);
  digitalWrite(ledPin, LOW);
  Serial.println("sleepnow");
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, wakeUpNow, LOW);
  sleep_mode();
  sleep_disable();
  detachInterrupt(0);
  RTC.armAlarm(1, false);
  RTC.clearAlarm(1);
  RTC.alarmInterrupt(1, false);
  digitalWrite(ledPin, HIGH);
  setAlarm();
  Serial.println ( "We zijn terug wakker");
}

void wakeUp() {}
void wakeUpNow() {}

void setAlarm() {
  DateTime now = RTC.now();
  DateTime future (now + TimeSpan(0, 0, 2, 0)); //dagen uren minuten seconden
  RTC.setAlarm(ALM1_MATCH_HOURS, future.minute(), future.hour(), future.second());
  RTC.alarmInterrupt(1, true);
  Serial.print("Next alarm on ");
  Serial.print(future.hour());
  Serial.print(":");
  Serial.print(future.minute());
  Serial.print(":");
  Serial.println(future.second());
  delay(1000);
}

/*
  String formatDateAsString ( DateTime dateTime ) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", dateTime.year(), dateTime.month(), dateTime.day(), dateTime.hour(), dateTime.minute(), dateTime.second());
  return String(buffer);
  }
*/

void EEPROM_writeDouble(int ee, double value) {
  byte* p = (byte*)(void*)&value;
  for (int i = 0; i < sizeof(value); i++) {
    EEPROM.write(ee++, *p++);
  }
}

double EEPROM_readDouble(int ee) {
  double value = 0.0;
  byte* p = (byte*)(void*)&value;
  for (int i = 0; i < sizeof(value); i++) {
    *p++ = EEPROM.read(ee++);
  }
  return value;
}

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROM_writeLong(int address, long value) {
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

long EEPROM_readLong(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}









