#include <DoubleResetDetector.h>  // DoubleResetDetector https://github.com/datacute/DoubleResetDetector.git
#include <Arduino.h>
#include <ESP8266WiFi.h>          // ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include "fauxmoESP.h"            // Lib for alexa https://github.com/vintlabs/fauxmoESP

#define SERIAL_BAUDRATE 115200

fauxmoESP fauxmo;

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 1

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

// PWM value for off state
#define PWM_LED_DISABLE 255

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

#define LEDPIN 2
#define BUTTONPIN 0

volatile bool ledState = false;
volatile unsigned char ledBriState = 0;
volatile bool lastState = false;
volatile bool currState = false;

// function to control leds
void ledOn() {
  ledState = true;
  Serial.printf("Pin: %d , value: %d\n", LEDPIN, ledBriState);
  analogWrite(LEDPIN, ledBriState);
}

void ledOff() {
  ledState = false;
  Serial.printf("Pin: %d , value: %d\n", LEDPIN, PWM_LED_DISABLE);
  analogWrite(LEDPIN, PWM_LED_DISABLE);
}

void ledSetBri(unsigned char value) {
  ledBriState = (ledState) ? PWM_LED_DISABLE-15-(value*0.7) : PWM_LED_DISABLE;

  Serial.printf("Pin: %d , value: %d\n", LEDPIN, ledBriState);
  analogWrite(LEDPIN, ledBriState);
}

void ICACHE_RAM_ATTR ledToogle() {
  if(ledState) {
    Serial.printf("off");
    ledOff();
  } else {
    Serial.printf("on");
    ledOn();
  }
}

// callback function for events alexa, google assistant, ...
void callback(unsigned char device_id, const char * device_name, bool state, unsigned char value) {
  Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
  ledState = state;
  ledSetBri(value);
}

void setup() {
  // init pins
  pinMode(LEDPIN, OUTPUT);
  pinMode(BUTTONPIN, INPUT_PULLUP);

  // led off by default
  analogWrite(LEDPIN, PWM_LED_DISABLE);
  
  // init uart port
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println("");
  
  // gpio interrupt config for the button
  attachInterrupt(digitalPinToInterrupt(BUTTONPIN),ledToogle,FALLING); 

  // set default ssid prefix + id chip
  String ssid = "HP" + String(ESP.getChipId());
  
  WiFiManager wifiManager;
  
  // detect double tap on reset
  if (drd.detectDoubleReset()) {
    // reset saved settings
    wifiManager.resetSettings();
    Serial.println("Double Reset Detected");
    Serial.println("WiFi configuraion reset");
    WiFi.disconnect();
    ESP.eraseConfig();
    ESP.restart();
  } else {
    Serial.println("No Double Reset Detected");
  }
  
  WiFi.hostname(ssid.c_str());
  wifiManager.autoConnect(ssid.c_str());
  
  // init library for alexa, google assistant, ...
  fauxmo.createServer(true);
  fauxmo.setPort(80);

  Serial.println("After connection, ask Alexa/Echo/Google to 'turn <devicename> on' or 'off'");
  
  fauxmo.enable(true);
  
  // set device name for alexa, google assistant, ...
  fauxmo.addDevice(ssid.c_str());

  // set callback function for events
  fauxmo.onSetState(callback);
}

void loop() {
  fauxmo.handle();
  drd.loop();
}