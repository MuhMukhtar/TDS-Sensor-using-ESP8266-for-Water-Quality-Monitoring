#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
 
const char *ssid =  "Fast";     // replace with your wifi ssid and wpa2 key
const char *password =  "lasagna enak";
const char *mqtt_server = "138.2.87.224";

WiFiClient espClient;
PubSubClient client(espClient);

long now = millis();
long lastMeasure = 0;
long lastMsg = 0;
char msg[50];
int value = 0;

unsigned long previousSend = 0;

int DSPIN = D5; // Dallas Temperature Sensor
 
namespace pin 
{
const byte tds_sensor = A0;   // TDS Sensor
}
 
namespace device 
{
float aref = 3.3;
}
 
namespace sensor 
{
float ec = 0;
unsigned int tds = 0;
float ecCalibration = 1;
} 

void setup_wifi(){
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");  
  }  
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
  
}
void reconnect(){
  while (!client.connected()){
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESP8266Client")){
      Serial.println("connected");
      client.subscribe("lampu");
    }else{
      Serial.print("failed, rc= ");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(3000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}
 
void loop(){
if(!client.connected()){
    reconnect();
  }

  if(!client.loop())
    client.connect("ESP8266Client");

  now = millis();
  
  double waterTemp = TempRead();
  waterTemp  = waterTemp*0.0625; // conversion accuracy is 0.0625 / LSB
  
  float rawEc = analogRead(pin::tds_sensor) * device::aref / 1024.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0); // temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
  
  sensor::ec = (rawEc / temperatureCoefficient) * sensor::ecCalibration; // temperature and calibration compensation
  sensor::tds = (133.42 * pow(sensor::ec, 3) - 255.86 * sensor::ec * sensor::ec + 857.39 * sensor::ec) * 0.5; //convert voltage value to tds value
  
  Serial.print(F("TDS:")); Serial.println(sensor::tds);
  Serial.print(F("EC:")); Serial.println(sensor::ec, 2);
  Serial.print(F("Temperature:")); Serial.println(waterTemp,2);  
  Serial.print(F(""));

  unsigned long currentSend = millis();
  if (currentSend - previousSend >= 1000) {
    previousSend = currentSend;
    char suhuAir[8];
    dtostrf(waterTemp, 1, 2, suhuAir);
    client.publish("suhu", suhuAir);

    char tds[8];
    dtostrf(sensor::tds, 1, 2, tds);
    client.publish("tds", tds);

    char ec[8];
    dtostrf(sensor::ec, 1, 2, ec);
    client.publish("ec", ec);
  }
 
}
 
 
boolean DS18B20_Init()
{
  pinMode(DSPIN, OUTPUT);
  digitalWrite(DSPIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(DSPIN, LOW);
  delayMicroseconds(750);//480-960
  digitalWrite(DSPIN, HIGH);
  pinMode(DSPIN, INPUT);
  int t = 0;
  while(digitalRead(DSPIN))
  {
    t++;
    if(t > 60) return false;
    delayMicroseconds(1);
  }
  t = 480 - t;
  pinMode(DSPIN, OUTPUT);
  delayMicroseconds(t);
  digitalWrite(DSPIN, HIGH);
  return true;
}
 
void DS18B20_Write(byte data)
{
  pinMode(DSPIN, OUTPUT);
  for(int i=0; i<8; i++)
  {
    digitalWrite(DSPIN, LOW);
    delayMicroseconds(10);
    if(data & 1) digitalWrite(DSPIN, HIGH);
    else digitalWrite(DSPIN, LOW);
    data >>= 1;
    delayMicroseconds(50);
    digitalWrite(DSPIN, HIGH);
  }
}
 
byte DS18B20_Read()
{
  pinMode(DSPIN, OUTPUT);
  digitalWrite(DSPIN, HIGH);
  delayMicroseconds(2);
  byte data = 0;
  for(int i=0; i<8; i++)
  {
    digitalWrite(DSPIN, LOW);
    delayMicroseconds(1);
    digitalWrite(DSPIN, HIGH);
    pinMode(DSPIN, INPUT);
    delayMicroseconds(5);
    data >>= 1;
    if(digitalRead(DSPIN)) data |= 0x80;
    delayMicroseconds(55);
    pinMode(DSPIN, OUTPUT);
    digitalWrite(DSPIN, HIGH);
  }
  return data;
}
 
int TempRead()
{
  if(!DS18B20_Init()) return 0;
   DS18B20_Write (0xCC); // Send skip ROM command
   DS18B20_Write (0x44); // Send reading start conversion command
  if(!DS18B20_Init()) return 0;
   DS18B20_Write (0xCC); // Send skip ROM command
   DS18B20_Write (0xBE); // Read the register, a total of nine bytes, the first two bytes are the conversion value
   int waterTemp = DS18B20_Read (); // Low byte
   waterTemp |= DS18B20_Read () << 8; // High byte
  return waterTemp;
}
