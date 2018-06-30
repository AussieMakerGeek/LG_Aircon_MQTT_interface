#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <Ethernet.h>
#include <SPI.h>

SoftwareSerial swSer(3, 4);

byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0x55, 0x57 };
IPAddress ip;
EthernetClient ethClient;

char id[5] = {'5','5','5','7','\0'};

IPAddress MQTT_Server;
byte server[] = { 192, 168, 0, 253 };


volatile long lastCharTime = 0;
volatile long lastRx = 0;
int charCount = 0;
unsigned char charBuff[13];
unsigned char charBuffNew[13];

unsigned int checksum1 = 0;
unsigned int checksum2 = 0;
unsigned int lastChecksum = 0;

byte powerByte = 0;
byte fanByte = 0;
byte modeByte = 0;
byte tempByte = 0;
byte zoneByte = 0; 

long previousMillis = 0;       // will store last time loop ran
long interval = 5000;           // interval at which to limit loop
int i;

long previousMQTTCommand = 0;
int waitForCommand = 1000;
boolean changeWaiting = 0;
boolean justChanged = 0;


//*******************************
//      FUNCTIONS

void callback(char* topic, byte* payload,unsigned int length) {
    // If there has been no MQTT message received for a bit...
    if((millis() - previousMQTTCommand > waitForCommand) && (changeWaiting == 0)) {
      previousMQTTCommand = millis();
      //Copy the existing bytes
      for (int i = 0; i < 13;i++){
        charBuffNew[i] = charBuff[i];
      }
    }
  
    if (topic[11] == '/'){
      if (topic[12] == 'Z'){
        changeWaiting = 1;
        //Serial.println("Zones");
        if (payload[3] == '1'){
          bitWrite(charBuffNew[5],3, 1);
        }else{
          bitWrite(charBuffNew[5],3, 0);
        }
        if (payload[2] == '1'){
          bitWrite(charBuffNew[5],4, 1);
        }else{
          bitWrite(charBuffNew[5],4, 0);
        }
        if (payload[1] == '1'){
          bitWrite(charBuffNew[5],5, 1);
        }else{
          bitWrite(charBuffNew[5],5, 0);
        }
        if (payload[0] == '1'){
          bitWrite(charBuffNew[5],6, 1);
        }else{
          bitWrite(charBuffNew[5],6, 0);
        }
        //Serial.println(charBuffNew[6],BIN);
      }
      if (topic[12] == 'M'){
        changeWaiting = 1;
        if (payload[0] == '0'){  //Cooling
          bitWrite(charBuffNew[1], 2, 0);
          bitWrite(charBuffNew[1], 3, 0);
          bitWrite(charBuffNew[1], 4, 0);
        }
        if (payload[0] == '1'){  //Dehumidify
          bitWrite(charBuffNew[1], 2, 1);
          bitWrite(charBuffNew[1], 3, 0);
          bitWrite(charBuffNew[1], 4, 0);
        }
        if (payload[0] == '2'){  //Fan only
          bitWrite(charBuffNew[1], 2, 0);
          bitWrite(charBuffNew[1], 3, 1);
          bitWrite(charBuffNew[1], 4, 0);
        }
        if (payload[0] == '3'){  //Auto
          bitWrite(charBuffNew[1], 2, 1);
          bitWrite(charBuffNew[1], 3, 1);
          bitWrite(charBuffNew[1], 4, 0);
        }
        if (payload[0] == '4'){  //Heating
          bitWrite(charBuffNew[1], 2, 0);
          bitWrite(charBuffNew[1], 3, 0);
          bitWrite(charBuffNew[1], 4, 1);
        }
      }
      if (topic[12] == 'T'){
        changeWaiting = 1;
        char tmpChar[3] = {payload[0],payload[1], '\0'};  // Convert it to a null terminated string.
        unsigned int tempval = atoi(tmpChar)-15;  // Take off the offset of 15 Degrees.
        bitWrite(charBuffNew[6], 0, bitRead(tempval, 0));  // Write the bits
        bitWrite(charBuffNew[6], 1, bitRead(tempval, 1));
        bitWrite(charBuffNew[6], 2, bitRead(tempval, 2));
        bitWrite(charBuffNew[6], 3, bitRead(tempval, 3));
      }
      if (topic[12] == 'F'){
        changeWaiting = 1;
        if (payload[0] == '0'){  //Low
          bitWrite(charBuffNew[1],5,0);
          bitWrite(charBuffNew[1],6,0);
        }
        if (payload[0] == '1'){  //Med
          bitWrite(charBuffNew[1],5,1);
          bitWrite(charBuffNew[1],6,0);
        }
        if (payload[0] == '2'){  //High
          bitWrite(charBuffNew[1],5,0);
          bitWrite(charBuffNew[1],6,1);
        }
      }
      if (topic[12] == 'P'){
        changeWaiting = 1;
        //Serial.println("Power");
        if (payload[0] == '1'){
          bitWrite(charBuffNew[1],1,1);
        }else{
          bitWrite(charBuffNew[1],1,0);
        }
      }
    }
    if (justChanged == 1 && changeWaiting == 1) changeWaiting = 0;
}

byte calcChecksum(){
  unsigned int checksum;
  checksum2 = 0;
  for (int i = 0; i < 12; i++){
    //Serial.print(charBuff[i]);
    //Serial.print(".");
    checksum2 = checksum2 + charBuffNew[i];
  }

    checksum = checksum2 ^ 0x55;
    return checksum - 256;
}

void serialFlush(){
  while(swSer.available() > 0) {
    char t = swSer.read();
  }
} 


void sendConfig(){
  charBuffNew[0] = 40;  // Slave
  charBuffNew[2] = 0;   //Clear out any other misc bits.
  charBuffNew[3] = 0;   //we dont need
  charBuffNew[4] = 0;
  charBuffNew[8] = 0;
  charBuffNew[9] = 0;
  charBuffNew[10] = 0;
  charBuffNew[11] = 0;
  
  //Calculate the checksum for the data
  charBuffNew[12] = calcChecksum();

  //Send it of to the AC
  Serial.println("Sending to AC");
  swSer.write(charBuffNew,13);
  
  // Make sure we are not listening to the data we sent...
  serialFlush();
}

//MQTT
PubSubClient MQTTClient(server, 1883, callback, ethClient);
char subPath[] = {'h', 'a', '/', 'm', 'o', 'd', '/', '5', '5', '5', '7', '/', '#','\0'};
byte topicNumber = 12;

void mqttConnect() {
  if (!MQTTClient.connected()) {
    Serial.println("Connecting to broker");
    if (MQTTClient.connect(id)){
      Serial.print("Subscribing to :");
      Serial.println(subPath);
      MQTTClient.subscribe(subPath);
    }else{
      Serial.println("Failed to connect!");
    }
  }
}


void publishTopicValue(char* strString, char* value) {
  Serial.print("S:");
  Serial.print(strString);
  Serial.print("/");
  Serial.println(value);
  MQTTClient.publish(strString, value);
}

void publishSettings(){
  justChanged = 1;

  //Power
  powerByte = bitRead(charBuff[1],1);

  // Fan speed 0-2 = Low, Med, High
  bitWrite(fanByte, 0, bitRead(charBuff[1],5));
  bitWrite(fanByte, 1, bitRead(charBuff[1],6));

  //Mode 0 = Cool, 1 = Dehumidify, 2 = Fan only, 3 = Auto, 4 = Heat
  bitWrite(modeByte,0, bitRead(charBuff[1],2));
  bitWrite(modeByte,1, bitRead(charBuff[1],3));
  bitWrite(modeByte,2, bitRead(charBuff[1],4));

  //Set Temp - Binary 0011 -> 1111 = 18 - 30 Deg (decimal 3 offset in value, starts at 18, possibly cool can be set at 15?)
  bitWrite(tempByte,0, bitRead(charBuff[6],0));
  bitWrite(tempByte,1, bitRead(charBuff[6],1));
  bitWrite(tempByte,2, bitRead(charBuff[6],2));
  bitWrite(tempByte,3, bitRead(charBuff[6],3));

  //Zone control - Single bits for each zone
  bitWrite(zoneByte,0, bitRead(charBuff[5],3)); //Zone 4
  bitWrite(zoneByte,1, bitRead(charBuff[5],4)); //Zone 3
  bitWrite(zoneByte,2, bitRead(charBuff[5],5)); //Zone 2
  bitWrite(zoneByte,3, bitRead(charBuff[5],6)); //Zone 1

  char strPath1[] = {'h', 'a', '/', 'm', 'o', 'd', '/', id[0], id[1], id[2], id[3], '/', 'P','\0'};  // Power State
  char strPath2[] = {'h', 'a', '/', 'm', 'o', 'd', '/', id[0], id[1], id[2], id[3], '/', 'T','\0'};  //Set Temp
  char strPath3[] = {'h', 'a', '/', 'm', 'o', 'd', '/', id[0], id[1], id[2], id[3], '/', 'M','\0'};  //Mode
  char strPath4[] = {'h', 'a', '/', 'm', 'o', 'd', '/', id[0], id[1], id[2], id[3], '/', 'Z','\0'};  //Zones
  char strPath5[] = {'h', 'a', '/', 'm', 'o', 'd', '/', id[0], id[1], id[2], id[3], '/', 'F','\0'};  //Fan

  char tempChar[2] = {powerByte + 48, '\0'};
  publishTopicValue(strPath1,tempChar);
  
  tempChar[0] = modeByte + 48;
  publishTopicValue(strPath3,tempChar);

  tempChar[0] = fanByte + 48;
  publishTopicValue(strPath5,tempChar);

  char charStr[5] = {bitRead(zoneByte,3)+48, bitRead(zoneByte,2)+48, bitRead(zoneByte,1)+48, bitRead(zoneByte,0)+48, '\0'};
  publishTopicValue(strPath4,charStr);

  char tmpChar[3];
  char* myPtr = &tmpChar[0];
  snprintf(myPtr, 3, "%02u", tempByte+15);
  publishTopicValue(strPath2,tmpChar);
  lastChecksum = charBuff[12];
}

//***********************************************

void setup() {
  Serial.begin(115200);

  //Set up software serial at 104 baud to talk to AC
  swSer.begin(104);

  Serial.print( "Ethernet MAC =" );
  for( i = 0; i < 6; i++ )
  {
    Serial.write( ' ' );
    Serial.print( mac[i], HEX );
  }
  Serial.println();
  Serial.print("Device ID = ");
  Serial.println(id);
  
  digitalWrite(13, HIGH);  
  // start the Ethernet connection:
  delay(500);
  digitalWrite(13, LOW);  
  Serial.println("Trying to get an IP address using DHCP");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    return;
  } 
 
  // print your local IP address:
  Serial.print("My IP address: ");
  ip = Ethernet.localIP();
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(ip[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();

  //MQTTClient.setServer(mqtt_server, 1883);
  //MQTTClient.setCallback(callback);

  Serial.println("All your AC belong to us");
}

void loop() {
  unsigned long currentMillis = millis();

  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    mqttConnect();
    if (justChanged == 1){
      justChanged = 0;
      serialFlush();  //Get rid of any data we received in the mean time.
    }
   }
   
   if((millis() - previousMQTTCommand > waitForCommand) && (changeWaiting == 1)) {  //More than 1 second and we have received an update via MQTT
     //We have received some updates and we need to send them off via serial.
     
     while (millis() - lastRx < 2000) {  //Make sure we are not stomping on someone else sending
       delay(100);                        //Main unit sends every 60 seconds, Master every 20
     }
     
     sendConfig();
     delay(200);
     sendConfig();  //Twice, just to be sure (This is how the factory unit operates...)
     changeWaiting = 0;
   }
   
   MQTTClient.loop();

   if (swSer.available()) { 
     lastCharTime=millis();
     charBuff[charCount] = swSer.read();
     charCount++;
     if (charCount == 13){
       Serial.print("R: ");
       for (int i=0; i < 12; i++){
         Serial.print(charBuff[i],DEC);
         Serial.print(",");  
       }
       Serial.println(charBuff[12],DEC);
       charCount = 0;
       
       if (charBuff[0] == 168){ // && charBuff[12] != lastChecksum){  //Only publish data back to MQTT from the Master controller.
         lastRx = millis(); //Track when we received the last 168 (master) packet
         if (changeWaiting == 0) publishSettings(); // If there is nothing pending FROM MQTT, Send the data off to MQTT
       }
     }
  }

  if (charCount < 13 && millis() - lastCharTime > 100){
    charCount = 0;  //Expired or we got half a packet. Ignore
  }
}
