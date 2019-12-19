
#define powerBtn 2
#define led 16

void setup() {
  pinMode(led, OUTPUT);
  pinMode(powerBtn, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("BOOOOT");
}

void loop() {
  if(digitalRead(powerBtn) == LOW){
    digitalWrite(led, HIGH);
    unsigned long cur = millis();
    while(digitalRead(powerBtn) == LOW && millis()-cur < 1500);
    if(millis() - cur > 1000){
      Serial.println("Press and Hold");
      digitalWrite(led, LOW);  
    }else{
      cur = millis();
      while(millis() - cur < 500 && digitalRead(powerBtn) == HIGH);
      if(millis() - cur < 500)
        Serial.println("Double Tap");
      else{
        Serial.println("Single Tap");
      }
    }
    delay(1000);
  }
    
}
