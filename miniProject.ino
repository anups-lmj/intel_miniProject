/*
 WiFiEsp test: ClientTest
http://www.kccistc.net/
작성일 : 2019.12.17 
작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI
#define AP_SSID "embA"
#define AP_PASS "embA1234"
#define SERVER_NAME "10.10.141.61"
#define SERVER_PORT 5000
#define LOGID "CLT_ARD"
#define PASSWD "PASSWD"

#define DHT_PIN 4  //dht11
#define WIFIRX 6   //6:RX-->ESP8266 TX
#define WIFITX 7   //7:TX -->ESP8266 RX
#define MOTOR_PIN 9
#define LED1_PIN 11
#define LED2_PIN 12
#define LED3_PIN 13
#define CDS_PIN A0
#define WATER_LEVEL_PIN A1
#define ONE_HOUR 3600
#define CMD_SIZE 50
#define ARR_CNT 5
#define DHTTYPE DHT11  //dht11

#include "WiFiEsp.h"
#include "SoftwareSerial.h"
#include <TimerOne.h>
#include <DHT.h>


char sendBuf[CMD_SIZE];
bool timerIsrFlag = false;
bool motorFlag = false;
char getsensorId[10] = "CLT_SQL";
unsigned long secCount;
unsigned long ledCount;
int sensorTime = 0;
int cds;
float humi;
float temp;
int water_level;
unsigned long timeSet = 12;
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
DHT dht(DHT_PIN, DHTTYPE);

void setup() {
  // put your setup code here, to run once:
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(LED1_PIN,OUTPUT);
  pinMode(LED2_PIN, OUTPUT);     //D12
  pinMode(LED3_PIN, OUTPUT);  //D13
  pinMode(CDS_PIN, INPUT);           //A0 CDS
  pinMode(WATER_LEVEL_PIN, INPUT);
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, HIGH);
  digitalWrite(LED3_PIN, HIGH);
  Serial.begin(115200);  //DEBUG
  wifi_Setup();
  Timer1.initialize(1000000);        //1000000uS ==> 1Sec
  Timer1.attachInterrupt(timerIsr);  // timerIsr to run every 1 seconds
  dht.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (client.available()) {
    socketEvent();
  }
  if (timerIsrFlag) {
    timerIsrFlag = false;
    if (!(secCount % 1)) {
      water_level = analogRead(WATER_LEVEL_PIN);
      // water_level = map(water_level,0,1023,0,100);
      cds = analogRead(CDS_PIN);
      cds = map(cds, 0, 1023, 0, 100);
      humi = dht.readHumidity();
      temp = dht.readTemperature();
      if (!motorFlag && water_level < 530) {
        Serial.println("MOTOR ON");
        digitalWrite(MOTOR_PIN, HIGH);
        motorFlag = true;
      }
      else if(motorFlag && water_level >= 600)
      {
        Serial.println("MOTOR OFF");
        digitalWrite(MOTOR_PIN,LOW);
        motorFlag = false;
      }
#ifdef DEBUG
      Serial.print("cds : ");
      Serial.print(cds);
      Serial.print(", temp : ");
      Serial.print(temp);
      Serial.print(", humi : ");
      Serial.print(humi);
      Serial.print(", water level : ");
      Serial.println(water_level);
#endif
      if (!(secCount % atoi(sensorTime))) {
        char tempStr[5];
        char humiStr[5];
        dtostrf(humi, 4, 1, humiStr);  //50.0   4:전체자리수,1:소수이하 자리수
        dtostrf(temp, 4, 1, tempStr);  //25.1
        sprintf(sendBuf, "[%s]SENSOR@%d@%s@%s@%d\n", getsensorId, cds, tempStr, humiStr, water_level);
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
      }
      
      if (!client.connected()) {
        server_Connect();
      }
    }
  }
}
void socketEvent() {
  int i = 0;
  char* pToken;
  char* pArray[ARR_CNT] = { 0 };
  char recvBuf[CMD_SIZE] = { 0 };
  int len;

  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  //  recvBuf[len] = '\0';
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL) {
    pArray[i] = pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  if (!strncmp(pArray[1], " New connected", 4))  // New Connected
  {
    Serial.write('\n');
    return;
  } else if (!strncmp(pArray[1], " Alr", 4))  //Already logged
  {
    Serial.write('\n');
    client.stop();
    server_Connect();
    return;
  } else if (!strcmp(pArray[1], "LAMP")) {
    ledCount =0;
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED1_PIN, HIGH);
      digitalWrite(LED2_PIN, HIGH);
      digitalWrite(LED3_PIN, HIGH);
    } else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, LOW);
      digitalWrite(LED3_PIN, LOW);
    }
    sprintf(sendBuf, "[LMJ_SQL]SETDB@%s@%s@%s\n", pArray[1], pArray[2], pArray[0]);
  } else if (!strcmp(pArray[1], "GETSTATE")) {
    if (!strcmp(pArray[2], "DEV")) {
      sprintf(sendBuf, "[%s]DEV@%s@%s\n", pArray[0], digitalRead(LED3_PIN) ? "ON" : "OFF", digitalRead(LED2_PIN) ? "ON" : "OFF");
    }
  } else if (!strcmp(pArray[1], "MOTOR")) {
    /*  int pwm = atoi(pArray[2]);
    pwm = map(pwm,0,100,0,255);
    analogWrite(MOTOR_PIN, pwm);*/
    if (!strcmp(pArray[2], "ON")) {
      motorFlag = true;
      digitalWrite(MOTOR_PIN, HIGH);
    } else if (!strcmp(pArray[2], "OFF")) {
      motorFlag = false;
      digitalWrite(MOTOR_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  } else if (!strcmp(pArray[1], "GETSENSOR")) {
    sensorTime = pArray[2];
    //strcpy(getsensorId,pArray[0]);
    sprintf(sendBuf, "[%s]%s@%s\n", getsensorId, pArray[1], pArray[2]);
  }

  client.write(sendBuf, strlen(sendBuf));
  client.flush();

#ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}
void timerIsr() {
  //  digitalWrite(LED_BUILTIN_PIN,!digitalRead(LED_BUILTIN_PIN));
  timerIsrFlag = true;
  secCount++;
  ledCount++;
  if(ledCount >= ONE_HOUR * timeSet)
  {
    digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
    digitalWrite(LED2_PIN, !digitalRead(LED2_PIN));
    digitalWrite(LED3_PIN, !digitalRead(LED3_PIN));
    ledCount=0;
  }
}
void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}
void wifi_Init() {
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    } else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}
int server_Connect() {
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connected to server");
#endif
    client.print("[" LOGID ":" PASSWD "]");
  } else {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}
void printWifiStatus() {
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
