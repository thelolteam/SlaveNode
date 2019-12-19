#include <Arduino.h>
#include <EEPROM.h>
#include<ESP8266WebServer.h>
#include<ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#define EEPROM_SIZE 48
#define MAX_NODES 10
#define MAX_BODY_LINES 7
#define MAX_CLIENTS 5
#define powerBtn 2
#define led 16

using namespace std;

int ipAssigned = 0;
const char* default_SSID="ESP32";
const char* default_pass="12345678";
const char* default_name = "Node";
const int ssidLoc = 0, passLoc = 10, nameLoc = 20; 
const int port = 80;
char ssid[11], password[11], name[11];

IPAddress masterIP(192, 168, 1, 1);
ESP8266WebServer server(port);

int id = -1;
int conStat = 0;
int relayStat = 0;
const int type = 2;
String message, url="";
String msg="okay"; 

String parameter[7];
int parameterCount = 0;

WiFiEventHandler gotIpEventHandler;
WiFiEventHandler stationModeDisconnectedHandler;

void printDetails(){
  Serial.print("Id=");
  Serial.println(id);
  Serial.print("Constat=");
  Serial.println(conStat);
  Serial.print("RelayStat=");
  Serial.println(relayStat);
  Serial.print("ssid=");
  Serial.println(ssid);
  Serial.print("password=");
  Serial.println(password);
  Serial.print("Name=");
  Serial.println(name);
}

void writeMemory(char addr, char *data){
  int i;
  for(i=0; data[i]!='\0' && i<10; i++){
    EEPROM.write(addr+i, data[i]);
  }
  EEPROM.write(addr+i, '\0');
  EEPROM.commit();
  Serial.print("Wrote: ");
  Serial.println(data);
}

void readMemory(char addr, char *data){
  int l = 0;
  char k = EEPROM.read(addr);
  while(k!='\0' && l<10){
    k = EEPROM.read(addr+l);
    data[l] = k;
    l++;
  }
  data[l] = '\0';
}

void getMetaData(){
  readMemory(ssidLoc, ssid);
  readMemory(passLoc, password);
}

void setMetaData(){
  writeMemory(ssidLoc, ssid);
  writeMemory(passLoc, password);
}

void getName()
{
  readMemory(nameLoc, name);
}

void setName()
{
  writeMemory(nameLoc, name);
}

void sendPacket(IPAddress ip, int port, String &message){
  HTTPClient client;
  url = "http://";
  url.concat(ip.toString());
  url.concat("/message?data=");
  url.concat(message);

  Serial.print("URL: ");
  Serial.println(url);
  client.begin(url);

  int httpCode = client.GET();
  if(httpCode > 0){
    if(httpCode == HTTP_CODE_OK){
      Serial.println("Request Sent suceessfully");
    }
  }else{
    Serial.println("HTTP GET Error");
  }
  client.end();
}

void sendReply(String &message){
  Serial.println("Replying: ");
  Serial.println(message);
  server.send(200, "text/plain", message);
}

void configure()
{
  //type#id#name#conStat#relayStat#
  message = "client@node#action@config#2#0#";
  message.concat(name);
  message.concat("#0#");
  message.concat(relayStat);
  message.concat("#");
  sendPacket(masterIP, port, message);
}

void separateParameters(String &body){
  int startI = 0, endI = 0, i;
  Serial.println();
  for(i=0; i<7; i++){
    parameter[i] = "";
    if(startI<body.length()){
      endI = body.indexOf('#', startI);
      parameter[i] = body.substring(startI, endI);
      Serial.println(parameter[i]);
      startI = endI+1;
      parameterCount++;
    }
  }
  Serial.print("PC: ");
  Serial.println(parameterCount);
}

void restartDevice(){
  Serial.println("Restarting....");
  delay(1000);
  ESP.restart();
}

void resetDevice(){
  strcpy(ssid, default_SSID);
  strcpy(password, default_pass);
  strcpy(name, default_name);
  setMetaData();
  setName();
  restartDevice();
}

void parameterDecode()
{
  if(parameter[1].equals("action@stat"))
  {
    strcpy(name, parameter[4].c_str());
    conStat = parameter[5].toInt();
    relayStat = parameter[6].toInt();
    sendReply(msg);
  }
  else if(parameter[1].equals("action@config"))
  {
    id = parameter[3].toInt();
    sendReply(msg);
    //Serial.println("Do as parameter line 3");
  }
  else if(parameter[1].equals("action@apconfig"))
  {
    strcpy(ssid, parameter[2].c_str());
    strcpy(password, parameter[3].c_str());
    setMetaData();
    sendReply(msg);
    //Serial.println("Do as parameter line 2, 3");
  }
  else if(parameter[1].equals("action@reset"))
  {
    sendReply(msg);
    resetDevice();
  }
  printDetails();
}

void handleRoot(){
  Serial.println("Root page accessed by a client!");
  server.send ( 200, "text/plain", "Hello, you are at root!");
}


void handleNotFound(){
  server.send ( 404, "text/plain", "404, No resource found");
}

void handleMessage(){
  if(server.hasArg("data")){
    message = server.arg("data");
    separateParameters(message);
    parameterDecode();
  }else{
    server.send(200, "text/plain", "Message Without Body");
  }
}

void setup() {
  pinMode(powerBtn, INPUT);
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  delay(500);
  
  getMetaData();
  getName();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  Serial.println("STA MODE....");
  Serial.print("Connecting..");

  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event)
  {
    Serial.print("Station connected, IP: ");
    Serial.println(WiFi.localIP());
    ipAssigned = 1;
  });

  stationModeDisconnectedHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event)
  {
    ipAssigned = 0;
  });

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  delay(300);
  while(!ipAssigned);
  configure();
  server.on("/", handleRoot);
  server.on("/message", handleMessage);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Server Started!");  
}


void loop() {
  server.handleClient();
  if(WiFi.status() != WL_CONNECTED){
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    
    while(!ipAssigned);
    configure();
  }

  if(digitalRead(powerBtn) == LOW){
    digitalWrite(led, HIGH);
    unsigned long cur = millis();
    while(digitalRead(powerBtn) == LOW && millis() - cur < 1000);
    if(millis() - cur > 1000){
      Serial.println("Press and Hold");
      delay(1000);
    }else
    {
      cur = millis();
      while(millis() - cur < 500 && digitalRead(powerBtn) == HIGH);
      if(millis() - cur < 500)
      {
        Serial.println("DoubleTap");
        delay(1000);
      }
      else
      {
        Serial.println("SingleTap");
        delay(1000);
      }
    }
  }
}