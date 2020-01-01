#include <Arduino.h>
#include <EEPROM.h>
#include<ESP8266WebServer.h>
#include<ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#define EEPROM_SIZE 48
#define powerBtn 2
#define led 16
#define relay 4
#define minDown 100
#define clickGap 400
#define hold 1200

using namespace std;

unsigned long cur;

int ipAssigned = 0;
int mode = 0;
const char* default_SSID="ESP32";
const char* default_pass="12345678";
const char* default_name = "Node";
const int ssidLoc = 0, passLoc = 10, nameLoc = 20; 
char ssid[11], password[11], name[11];

const int port = 8080;
IPAddress masterIP(192, 168, 1, 1);
ESP8266WebServer server(port);
HTTPClient client;

int id = -1;
int conStat = 0;
int relayStat = 0;
const int type = 2;
String message, url="";

String parameter[7];

WiFiEventHandler gotIpEventHandler;
WiFiEventHandler stationModeDisconnectedHandler;

void printDetails(){
  Serial.print("Id: ");
  Serial.println(id);
  Serial.print("Constat: ");
  Serial.println(conStat);
  Serial.print("RelayStat: ");
  Serial.println(relayStat);
  Serial.print("ssid: ");
  Serial.println(ssid);
  Serial.print("password: ");
  Serial.println(password);
  Serial.print("Name: ");
  Serial.println(name);
}

/*void blink(int times){
  for(int i=0; i<times; i++){
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
    delay(300);
  }
}*/


void writeMemory(char addr, char *data){
  int i;
  for(i=0; data[i]!='\0' && i<10; i++){
    EEPROM.write(addr+i, data[i]);
  }
  EEPROM.write(addr+i, '\0');
  EEPROM.commit();
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

void restartDevice(){
  Serial.println("Restarting....");
  ESP.restart();
}

void resetDevice(){
  strcpy(ssid, default_SSID);
  strcpy(password, default_pass);
  strcpy(name, default_name);
  setMetaData();
  setName();
  Serial.println("Device Reset");
  restartDevice();
}

void separateParameters(String &body){
  int startI = 0, endI = 0, i;
  for(i=0; i<7; i++){
    parameter[i] = "";
    if(startI<body.length()){
      endI = body.indexOf('$', startI);
      parameter[i] = body.substring(startI, endI);
      startI = endI+1;
    }
  }
}

void sendReply(String message){
  Serial.print("Replying with: ");
  Serial.println(message);
  server.send(200, "text/plain", message);
}

void setRelay(){
  if(relayStat == 1){
    digitalWrite(relay, LOW);
    digitalWrite(led, HIGH);
  }else{
    digitalWrite(relay, HIGH);
    digitalWrite(led, LOW);
  }
}

void invertRelay(){
  if(relayStat == 1){
    digitalWrite(relay, HIGH);
    digitalWrite(led, LOW);
    relayStat = 0;
  }else{
    digitalWrite(relay, LOW);
    digitalWrite(led, HIGH);
    relayStat = 1;
  }
}

void parameterDecode()
{
  if(parameter[1].equals("action@stat"))
  { 
    conStat = parameter[5].toInt();
    relayStat = parameter[6].toInt();
    setRelay();
    if(strcmp(name, parameter[4].c_str()) != 0){
      strcpy(name, parameter[4].c_str());
      setName();
    }
    sendReply("Node: Stat RCVD");
  }
  else if(parameter[1].equals("action@config"))
  {
    id = parameter[3].toInt();
    conStat = parameter[5].toInt();
    sendReply("Node: Config RCVD");

  }
  else if(parameter[1].equals("action@apconfig"))
  {
    strcpy(ssid, parameter[2].c_str());
    strcpy(password, parameter[3].c_str());
    setMetaData();
    setName();
    sendReply("NODE: APConfig RCVD");
    delay(7000);
    restartDevice();
  }
  else if(parameter[1].equals("action@reset"))
  {
    sendReply("Node: Reset RCVD");
    resetDevice();
  }
  printDetails();
}

void sendPacket(IPAddress ip, int port, String &message){
  url = "http://";
  url.concat(ip.toString());
  url.concat(":8080/message?data=");
  url.concat(message);

  Serial.print("URL: ");
  Serial.println(url);
  client.begin(url);

  int retry = 5;
  int httpCode = client.GET();
  if(httpCode > 0){
    if(httpCode == HTTP_CODE_OK){
      Serial.printf("\nRequest Sent: %d\n", httpCode);
      client.end();
      retry--;
    }
  }else{
    Serial.println("HTTP GET Error");
    client.end();
    delay(1000);
    if(retry>0)
      sendPacket(ip, port, message);
  }
}

void sendNodeStat(){
  message = "client@node$action@stat$2$";
  message.concat(id);
  message.concat("$");
  message.concat(name);
  message.concat("$");
  message.concat(conStat);
  message.concat("$");
  message.concat(relayStat);
  message.concat("$");

  sendPacket(masterIP, port, message);
}

void configure()
{
  message = "client@node$action@config$2$0$";
  message.concat(name);
  message.concat("$0$");
  message.concat(relayStat);
  message.concat("$");
  sendPacket(masterIP, port, message);
}

void handleRoot(){
  Serial.println("Root page accessed by a client!");
  server.send ( 200, "text/plain", "Node: Hello, you are at root!");
}


void handleNotFound(){
  server.send ( 404, "text/plain", "Node: 404, No resource found");
}

void handleMessage(){
  if(server.hasArg("data")){
    message = server.arg("data");
    separateParameters(message);
    parameterDecode();
  }else{
    server.send(200, "text/plain", "Node: Message Without Body");
  }
}

void startAPMode(){
  Serial.println("AP mode");
  mode = 1;
  conStat = 0;
  WiFi.mode(WIFI_AP);
  String apSsid = "Node_";
  apSsid.concat(name);
  WiFi.softAP(apSsid, "", 1, 0, 1);
  delay(100);
  IPAddress myIP(192, 168, 1, 1);
  IPAddress mask(255, 255, 255, 0);
  WiFi.softAPConfig(myIP, myIP, mask);

  Serial.print("Node AP IP: ");
  Serial.println(WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/message", handleMessage);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("Server Started: %d\n\n", port);
}

void setup() {
  pinMode(powerBtn, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  delay(500);
  
  getMetaData();
  getName();
  printDetails();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event)
  {
    Serial.print("Connected to AP, IP: ");
    Serial.println(WiFi.localIP());
    ipAssigned = 1;
  });

  stationModeDisconnectedHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event)
  {
    ipAssigned = 0;
  });


  mode = 0;
}

void loop() {
  server.handleClient();
  if(WiFi.status() != WL_CONNECTED && mode == 0){
    Serial.print("Connecting in STA Mode..");
    while (digitalRead(powerBtn) == HIGH && WiFi.status() != WL_CONNECTED && mode==0) {
      delay(100);
      Serial.print(".");
      //blink(1);
    }
    
    if(WiFi.status() == WL_CONNECTED){
      while(!ipAssigned);
      configure();
      server.on("/", handleRoot);
      server.on("/message", handleMessage);
      server.onNotFound(handleNotFound);
      server.begin();
      Serial.printf("\nServer ON: %d\n", port);
      //blink(3);
    }
  }

  if(digitalRead(powerBtn) == LOW){
    cur = millis();
    while(digitalRead(powerBtn) == LOW && (millis() - cur) < hold);

    if(millis() - cur < clickGap){
      Serial.println("Here");
      delay(minDown);
      cur = millis();
      boolean doubleClick = true;
      while(digitalRead(powerBtn) == HIGH){
        if(millis() - cur > clickGap){
          doubleClick = false;
          break;
        }
      }
      if(doubleClick){
        Serial.println("Double Tap");
        if(mode == 0)
          startAPMode();
        else
          restartDevice();
      }else{
        Serial.println("Single Tap");
        invertRelay();
        if(conStat && mode==0)
          sendNodeStat();
      }
    }else{
      Serial.println("Press and Hold");
      resetDevice();
    }
  }


  /*if(digitalRead(powerBtn) == LOW){
    digitalWrite(led, HIGH);
    unsigned long cur = millis();
    while(digitalRead(powerBtn) == LOW && millis() - cur < 1500);
    if(millis() - cur > 1000){
      digitalWrite(led ,LOW);
      Serial.println("Press and Hold");
     
    }else
    {
      cur = millis();
      while(millis() - cur < 500 && digitalRead(powerBtn) == HIGH);
      if(millis() - cur < 400)
      {
        digitalWrite(led ,LOW);
        Serial.println("DoubleTap");
        if(mode == 0)
          startAPMode();
        else
          restartDevice();
      }
      else
      {
        Serial.println("SingleTap");
        invertRelay();
        if(conStat && mode==0)
          sendNodeStat();
      }
    }
  }*/
}