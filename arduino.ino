
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GSM.h>
#include <ctype.h>

#define PINNUMBER "****"

#define GPRS_APN       "internet.proximus.be" // replace your GPRS APN
#define GPRS_LOGIN     ""    // replace with your GPRS login
#define GPRS_PASSWORD  "" // replace with your GPRS password

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

char path[] = "/pod";
int port = 8080; // port 80 is the default for HTTP

RTC_DS3231 rtc;

String sensorstring = "11.11";
boolean sensor_string_complete = true;
/*
 * 
 String DO_String = "";
String T_String = "";
String Tur_String = "";
String tempDO = "";
*/
//boolean DODO = true;

// SYSTEEM
void setup() {

  //setup connection to console
  Serial.begin(9600);
  Serial.println("Start connectie met console");

  rtc.begin();
  rtc.adjust(1388534400);
  Serial.println("Setting the RTC to Jan 1, 2014 00:00:00");

  Serial.println ( nowAsString() );

  //setup connection to do sensor
  Serial3.begin(9600);
  Serial.println("Start connectie met DO sensor");

  //setup connection to temperatur sensor
  Serial.println("Start connectie met temperatuur sensor");
  tSensor.begin();

  /*
// this code is for sending sms
  Serial.println("Start verbinding met GSM module");
  boolean notConnected = true;
  while(notConnected) {
    Serial.print (".");
    if(gsmAccess.begin(PINNUMBER)==GSM_READY){
      notConnected = false;
    } else {
      delay(1000);
    }
  }
  Serial.println ("!");
  Serial.println("Verbinding met GSM module klaar");
*/

  setupGPRS();

  //tell DO sensor to take a reading
  requestReadingFromDO();
}

void setupGPRS() {
  Serial.println("Opstarten van internet verbinding");
  // connection state
  boolean notConnected = false;

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
  sensorstring = Serial3.readStringUntil(13);         //read the string until we see a <CR>
  sensor_string_complete = true;                      //set the flag used to tell if we have received a completed string from the PC
}

void loop() {

  if ( sensor_string_complete == true ) {
    if ( isValidNumber ( sensorstring ) ) {
      Serial.println ( sensorstring );
    } else {
      Serial.println ( sensorstring );
      sensor_string_complete = false;
      requestReadingFromDO();
    }
  }

  if ( sensor_string_complete == true) {
    sendMessage ( sensorstring, getTur(), getT(0) );
    sensor_string_complete = false;
    requestReadingFromDO();
  }

}

void sendMessage ( String doreading, String turbiditeit, String temperatuur ) {
  char txtMsg[200];
  String message = "{ \"timestamp\": \"" + nowAsString() + "\", \"data\": [ " +  
    "{ \"disolvedOxygen\": " + doreading + ", \"turbidity\": " + turbiditeit + ", \"temperature\": " + temperatuur + ", \"samplingTimestamp\": \""+nowAsString()+"\"}"
    + " ] }"
    ;
  Serial.println ( message );
//  sendMessageOverGPRS ( message );
  /*
  message.toCharArray(txtMsg, 200);
  sms.beginSMS(remoteNumber);
  sms.print(txtMsg);
  sms.endSMS();
  */
}




/*
 String getDO(){
  String string ="";
   myserial.print("R");  
   myserial.print('\r');
   boolean A = true;
   while(A == true){
      if (myserial.available() > 0) {                    
      char inchar = (char)myserial.read();            
       string += inchar;                           
        if (inchar == '\r') {                           
        if(string != "*OK"){
         
          A = false;
        return string;
          
        }
        string = "";
        }
        
       }
   }
}

*/

String getT(int index){
  tSensor.requestTemperatures();
  float currentTemp = tSensor.getTempCByIndex(index);
  char buffer[10];
  return dtostrf(currentTemp, 5, 2, buffer);
}

String getTur(){
  int sensorValue = analogRead(A0);
  float voltage = sensorValue * (5.0 / 1024.0); 
  char buffer[7];
  return dtostrf(voltage, 5, 2, buffer);
}

boolean isValidNumber(String str){
   for(byte i=0;i<str.length();i++) {
      if(!isDigit(str.charAt(i)) && str.charAt(i) != '.' ) {
        return false;
      }
   }
   return true;
} 

void sendMessageOverGPRS ( String message ) {
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

String nowAsString() {
  DateTime now = rtc.now();
  char buffer[20]; 
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(buffer);
}





















