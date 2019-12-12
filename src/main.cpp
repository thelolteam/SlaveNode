#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiServer.h>
#include <ESP8266WiFi.h>

#define EEPROM_SIZE 48
#define MAX_NODES 10
#define MAX_BODY_LINES 7
#define MAX_CLIENTS 5

using namespace std;

const char* default_SSID="ESP32";
const char* default_pass="12345678";
const char* default_name = "Node";
const int ssidLoc = 0, passLoc = 10, nameLoc = 20; 
const int port = 9999;
int currClientIndex = 0;
int curBodyLineCount = 0;
char ssid[11], password[11], name[11];
IPAddress masterIP(192, 168, 1, 1);
WiFiServer server(port);

int id = -1;
int conStat = 0;
int relayStat = 0;
const int type = 2;
String message; 

String parameter[7];
int parameterCount = 0;

WiFiEventHandler gotIpEventHandler;

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
  Serial.println("Sending Packet: ");
  Serial.println(message);
  WiFiClient client;
  Serial.print("To IP: ");
  Serial.println(ip);
  if(client.connect(ip, port)){
    client.print(message);
    client.stop();
    Serial.println("Sent!");
  }else{
    Serial.println("Connection To client Failed!");
  }
}

void configure()
{
  Serial.println("In configure!");
  message = "HTTP/1.1 200 OK\n\n";
  message.concat("client@node#action@config#2#0");
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

void readPacket(WiFiClient client){
  String packetData = "", bodyLine = "", curLine = "";
  int m = client.available();
  while(m!=0){
    packetData.concat(client.read());
    m--;
  }

  Serial.println("Packet: ");
  Serial.println(packetData);
  int n = 0, i;
  for(i=0; i<packetData.length(); i++){
    if(packetData[i] == '\n'){
      if(curLine.length() == 0){
        n = ++i;
        break;
      }else{
        curLine = "";
      }
    }else if(packetData[i] == '\r'){
      curLine += packetData[i];
    }
  }
  bodyLine = packetData.substring(n, packetData.length());
  Serial.print("Body Line: ");
  Serial.print(bodyLine);
  Serial.println("|");
  separateParameters(bodyLine);
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

void recevicePacket()
{
  if(parameter[1].equals("action@stat"))
  {
    strcpy(name, parameter[4].c_str());
    conStat = parameter[5].toInt();
    relayStat = parameter[6].toInt();

  }
  else if(parameter[1].equals("action@config"))
  {
    id = parameter[3].toInt();
    //Serial.println("Do as parameter line 3");
  }
  else if(parameter[1].equals("action@apconfig"))
  {
    strcpy(ssid, parameter[2].c_str());
    strcpy(password, parameter[3].c_str());
    setMetaData();
    //Serial.println("Do as parameter line 2, 3");
  }
  else if(parameter[1].equals("action@reset"))
  {
    resetDevice();
  }
}



void setup() {
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
    configure();
  });


  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  delay(300);
  server.begin();
  Serial.printf("\nServer Started: %d", port);
}


void loop() {
  if(server.hasClient())
  {
    Serial.println("New Client");
    WiFiClient client = server.available();
    Serial.print("Available: ");
    Serial.println(client.available());
    while(!client.available());
    Serial.println("Available");
    readPacket(client);
    client.stop();
    recevicePacket();
  }
}