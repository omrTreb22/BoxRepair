/*
  Analog input, analog output, serial output

  Reads an analog input pin, maps the result to a range from 0 to 255 and uses
  the result to set the pulse width modulation (PWM) of an output pin.
  Also prints the results to the Serial Monitor.

  The circuit:
  - potentiometer connected to analog pin 0.
    Center pin of the potentiometer goes to the analog pin.
    side pins of the potentiometer go to +5V and ground
  - LED connected from digital pin 9 to ground

  created 29 Dec. 2008
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/AnalogInOutSerial
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include "wifi_id.h"


// These constants won't change. They're used to give names to the pins used:
const int digitalOutPin = 4; // Digital output pin where is connected the relay (D2)

const int UDP_PORT_RELAY = 5009;

WiFiUDP udp;

const char* ssid     = SSID;     // Identifiant du reseau WIFI
const char* password = PASSWORD; // Mot de passe du reseau WIFI utilise

#define LED  2

enum States 
{
  STATE_NORMAL = 0,
  STATE_CHECKING,
  STATE_POWER_OFF,
  STATE_RECOVERING,
};

enum POWER_MODE {
  POWER_ON = 0,
  POWER_OFF = 1
};

int G_numberOfRetry = 0;
int G_numberOfPowerOff = 0;
const int MAX_RETRY = 5;
const int NORMAL_CHECK_DURATION = 60;
const int RECOVERY_AFTER_POWER_ON_DURATION = 300;
const int LONG_RECOVERY_AFTER_POWER_ON_DURATION = 3600;
const int POWER_OFF_DURATION = 20;
int G_indexDelay = 0;
int G_delayChecking[MAX_RETRY] = { 30, 30, 60, 60, 60 };
bool flagPowerOffCycle = false;
States G_state = STATE_NORMAL;
int G_tempo = 30;

// Simulation mode for testing
int G_simulationMode = false;
int G_currentTime = 0;
int G_forceConnectionDown = false;
int G_timeOff =  100;
int G_timeOn  = 160;

// Test protocol
// STATE        timeOff     TimeOn    Result
// NORMAL       100          160      Remains NORMAL
// NORMAL       100          190      One retry, NORMAL
// NORMAL       100          250      2nd retry, NORMAL
// NORMAL       100          370      Last retry, NORMAL
// NORMAL       20           320      POWER_OFF cycle : NORMAL
// NORMAL       20          5000      POWER_OFF cycle, 1 retry, NORMAL
// NORMAL       20         10000      POWER OFF cycle, 2 retries NORMAL 


// End Simulation mode

void setup() {
  int nbtent = 0;
  
  // initialize serial communications at 9600 bps:
  Serial.begin(115200);

  pinMode(digitalOutPin, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(digitalOutPin, POWER_ON);
  digitalWrite(LED, 1);

  WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      if (nbtent<10){
        nbtent++ ;   
        delay(5000);
        Serial.println("Trying to connect to Wifi.");
      }
      else{
        Serial.println("Reset");
        ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT);
        delay(5000);
        nbtent = 0;
      }    
    }

    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Setup watchdog
  //ESP.wdtDisable();
  //ESP.wdtEnable(8000);
}

bool checkConnection()
{
  if (G_simulationMode)
  {
    printState(G_state);
    Serial.println("for " + String(G_tempo) + " seconds. IsAfterPowerOffCycle=" + String(flagPowerOffCycle));

    if (G_forceConnectionDown)
    {
      Serial.println("Simulation mode - @" + String(G_currentTime) + " Return connection Down");
      return false;
    }
    Serial.println("Simulation mode - @" + String(G_currentTime) + " Return connection OK");
    return true;
  }

  // Envoie de la requete HTTP
  HTTPClient http;

  http.begin("http://www.google.fr");
  int httpCode = http.GET();

  if (httpCode == 200) 
  {
    Serial.println("HTTP request returns code 200, OK");
    http.end();
    return true;
  }
  Serial.println("HTTP request failed with return code : " + String(httpCode));
  http.end();
  return false;
}

void printState(int state)
{
  switch(state)
  {
    case STATE_NORMAL:
      Serial.print("State NORMAL     ");
      break;

    case STATE_CHECKING:
      Serial.print("State CHECKING   ");
      break;

    case STATE_POWER_OFF:
      Serial.print("State POWER_OFF  ");
      break;

    case STATE_RECOVERING:
      Serial.print("State RECOVERING ");
      break;
  }
}

void loop() 
{
  char buffer[128];
  int n;
  bool ret;
  int state = 255;
  int value = 255;
    
  // For simulation mode only
  if (G_simulationMode)
  {
    G_currentTime += 5;
  
    if (G_currentTime >= G_timeOn)
      G_forceConnectionDown = false;
    else
      if (G_currentTime >= G_timeOff)
        G_forceConnectionDown = true;
  }
 
  digitalWrite(LED, 0);
  delay(300);
  digitalWrite(LED, 1);

  G_tempo -= 5;
  if (G_tempo > 0)
  {
    printState(G_state);
    Serial.println("for " + String(G_tempo) + " seconds. IsAfterPowerOffCycle=" + String(flagPowerOffCycle));
    delay(4700);
    return;
  }

  switch(G_state)
  {
    case STATE_NORMAL:
      ret = checkConnection();
      if (ret)
      {
        G_tempo = NORMAL_CHECK_DURATION;
        flagPowerOffCycle = false;
        Serial.println("Remaining in NORMAL state for " + String(G_tempo) + " seconds");
        break;
      }
      G_state = STATE_CHECKING;
      G_indexDelay = 0;
      G_tempo = G_delayChecking[G_indexDelay];
      Serial.println("Moving to CHECKING state for " + String(G_tempo) + " seconds");
      break;

    case STATE_CHECKING:
      ret = checkConnection();
      if (ret)
      {
        G_tempo = NORMAL_CHECK_DURATION;
        G_state = STATE_NORMAL;
        flagPowerOffCycle = false;
        Serial.println("Moving to NORMAL state for " + String(G_tempo) + " seconds");
        break;
      }
      G_indexDelay++;
      if (G_indexDelay < MAX_RETRY)
      {
        G_tempo = G_delayChecking[G_indexDelay];
        Serial.println("Remaining in CHECKING state for " + String(G_tempo) + " seconds - Retry=" + String(G_indexDelay));
        break;
      }
      G_state = STATE_POWER_OFF;
      G_tempo = POWER_OFF_DURATION;
      Serial.println("Last retry failed : POWER OFF for " + String(G_tempo) + " seconds");
      digitalWrite(digitalOutPin, POWER_OFF);
      break;

    case STATE_POWER_OFF:
      Serial.println("POWER ON...");
      digitalWrite(digitalOutPin, POWER_ON);
      if (flagPowerOffCycle)
        G_tempo = LONG_RECOVERY_AFTER_POWER_ON_DURATION;
      else
        G_tempo = RECOVERY_AFTER_POWER_ON_DURATION;
      flagPowerOffCycle = true;
      Serial.println("Waiting for restart during " + String(G_tempo) + " seconds");
      G_state = STATE_RECOVERING;
      break;

    case STATE_RECOVERING:
      Serial.println("End of Power OFF/ON cycle...");
      G_tempo = 5;
      G_state = STATE_NORMAL;
      break;
  }

  delay(4700);
 
  //ESP.wdtFeed();
  return;
 }
