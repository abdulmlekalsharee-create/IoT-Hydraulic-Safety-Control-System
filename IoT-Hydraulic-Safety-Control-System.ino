

/*
 * PROJECT: Autonomous IoT-Based Hydraulic Monitoring and Safety Control System
 * ENGINEER: Abdulmlek Alsharee
 * FEATURES: Auto-Shutoff, Web Dashboard, SMTP Alerts, ThingSpeak Integration, Fluid Dynamics
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>      
#include <HTTPClient.h>
#include <ESP_Mail_Client.h>

// --- 1. Network & Cloud Configuration ---
const char* ssid = "Galaxy S22 Ultra E6C7";
const char* password = "12345678";

String apiKey = "GE9BGEEPO7KV7532"; 
const char* thingSpeakServer = "api.thingspeak.com";

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587
#define AUTHOR_EMAIL "cybermechatro1@gmail.com"
#define AUTHOR_PASSWORD "Cueuoqlqkaxskhvf"
#define RECIPIENT_EMAIL "Abdulmlekalsharee@gmail.com"

// --- 2. Pin Definitions ---
#define FLOW_PIN        4
#define RELAY_PIN       5
#define BUZZER_PIN      19
#define PRESSURE_PIN    34
#define BTN_CONTROL     13
#define BTN_RESET       12

#define LED_R           18
#define LED_G           17
#define LED_B           16

// Relay Logic (Active HIGH)
#define VALVE_OPEN      LOW   
#define VALVE_CLOSE     HIGH

// --- 3. Global Variables & Objects ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);
WebServer server(80);

SMTPSession smtp;
Session_Config config;
SMTP_Message message;

volatile int flowPulseCount = 0;
float flowRate = 0.0, totalVolume = 0.0, pressureBar = 3.4, velocity = 0.0, hydraulicPower = 0.0;
const float PIPE_AREA = 0.000127; 

bool valveState = false; 
bool alarmState = false; 
bool emailSent = false;  

unsigned long lastSensorRead = 0;
unsigned long lastCloudUpload = 0;

void IRAM_ATTR pulseCounter() { flowPulseCount++; }

// --- 4. Web Dashboard Generation ---
String getHTML() {
  String ptr = "<!DOCTYPE html><html><head>";
  ptr += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  ptr += "<meta charset=\"UTF-8\">"; 
  ptr += "<style>";
  ptr += "body { font-family: sans-serif; text-align: center; background-color: #f0f2f5; margin: 0; padding: 20px; }";
  ptr += "h2 { color: #333; margin-bottom: 20px; }";
  ptr += ".card { background: white; padding: 15px; margin: 10px auto; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); max-width: 400px; }";
  ptr += ".warning { background-color: #ffe6e6; border: 2px solid red; color: red; animation: blink 1s infinite; }";
  ptr += "@keyframes blink { 50% { opacity: 0.5; } }";
  ptr += ".label { font-size: 1rem; color: #666; }";
  ptr += ".value { font-size: 1.8rem; color: #007bff; font-weight: bold; }";
  ptr += ".btn { display: inline-block; width: 45%; padding: 15px; font-size: 18px; color: white; border: none; border-radius: 8px; cursor: pointer; text-decoration: none; margin: 5px; }";
  ptr += ".btn-on { background-color: #28a745; }"; 
  ptr += ".btn-off { background-color: #dc3545; }"; 
  ptr += "</style></head><body>";
  
  if (alarmState) {
    ptr += "<div class='card warning'><h1>WARNING</h1><p>High Pressure / Leak Detected!</p><p>System Auto-Protected.</p></div>";
  }

  ptr += "<h2>Smart Water System</h2>";
  
  ptr += "<div class='card'><div class='label'>Pressure</div><div class='value'>" + String(pressureBar, 1) + " Bar</div></div>";
  ptr += "<div class='card'><div class='label'>Flow Rate</div><div class='value'>" + String(flowRate, 1) + " L/min</div></div>";
  ptr += "<div class='card'><div class='label'>Water Velocity</div><div class='value'>" + String(velocity, 2) + " m/s</div></div>";
  ptr += "<div class='card'><div class='label'>Hydraulic Power</div><div class='value'>" + String(hydraulicPower, 2) + " Watts</div></div>";
  ptr += "<div class='card'><div class='label'>Total Volume</div><div class='value'>" + String(totalVolume, 1) + " L</div></div>";
  
  ptr += "<div class='card'>";
  ptr += "<h3>Valve Status: " + String(valveState ? "<span style='color:green'>OPEN</span>" : "<span style='color:red'>CLOSED</span>") + "</h3>";
  ptr += "<a href='/open'><button class='btn btn-on'>OPEN</button></a>";
  ptr += "<a href='/close'><button class='btn btn-off'>CLOSE</button></a>";
  ptr += "</div>";

  ptr += "</body></html>";
  return ptr;
}

// --- 5. Valve Control Logic ---
void handleRoot() { server.send(200, "text/html", getHTML()); }

void handleOpen() {
  valveState = true;
  digitalWrite(RELAY_PIN, VALVE_OPEN); 
  digitalWrite(LED_R, HIGH); digitalWrite(LED_G, LOW); digitalWrite(LED_B, HIGH);
  server.sendHeader("Location", "/"); server.send(303);
}

void handleClose() {
  valveState = false;
  digitalWrite(RELAY_PIN, VALVE_CLOSE); 
  digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, LOW);
  server.sendHeader("Location", "/"); server.send(303);
}

// --- 6. Alerts & Telemetry ---
void sendEmailAlert(String alertMsg) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: WiFi not connected!");
    return;
  }
  
  smtp.debug(1); 
  config.server.host_name = SMTP_HOST; 
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL; 
  config.login.password = AUTHOR_PASSWORD;
  
  config.time.ntp_server = "pool.ntp.org,time.nist.gov";
  config.time.gmt_offset = 3; 
  config.time.day_light_offset = 0;

  message.sender.name = "Smart Water System"; 
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "URGENT: SYSTEM ALERT!"; 
  message.addRecipient("Engineer", RECIPIENT_EMAIL);
  message.text.content = alertMsg.c_str();

  Serial.println("Setting time for SSL...");
  delay(2000); 

  Serial.println("Attempting to send email via Port 587...");

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.print("Error: ");
    Serial.println(smtp.errorReason());
  } else {
    Serial.println("Email sent successfully!");
  }
}

void sendToThingSpeak(float p, float q, float v, float vol) {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + apiKey + 
                 "&field1=" + String(p) + "&field2=" + String(q) + 
                 "&field3=" + String(v) + "&field4=" + String(vol) +
                 "&field5=" + String(valveState ? 1 : 0); 
    http.begin(url); http.GET(); http.end();
  }
}

// --- 7. System Initialization ---
void setup() {
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, VALVE_CLOSE); 
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_R, OUTPUT); digitalWrite(LED_R, HIGH);
  pinMode(LED_G, OUTPUT); digitalWrite(LED_G, HIGH);
  pinMode(LED_B, OUTPUT); digitalWrite(LED_B, HIGH);
  pinMode(BTN_CONTROL, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);

  u8g2.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  Serial.println("WiFi Connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.begin();

  digitalWrite(LED_B, LOW);
  sendEmailAlert("System Started Successfully! Monitoring Mode: ON.");
}

// --- 8. Main Loop & Physics Logic ---
void loop() {
  server.handleClient(); 

  unsigned long currentMillis = millis();

  // Manual Override
  if (digitalRead(BTN_CONTROL) == LOW) {
    delay(200);
    if(valveState) handleClose(); else handleOpen();
  }
  if (digitalRead(BTN_RESET) == LOW) {
    totalVolume = 0.0; alarmState = false; emailSent = false;
    digitalWrite(BUZZER_PIN, LOW); delay(200);
  }

  // Sensor Polling & Physics Calculations (1Hz)
  if (currentMillis - lastSensorRead >= 1000) {
    detachInterrupt(digitalPinToInterrupt(FLOW_PIN));
    flowRate = ((1000.0 / (currentMillis - lastSensorRead)) * flowPulseCount) / 7.5;
    totalVolume += (flowRate / 60.0);
    if(flowRate > 0) velocity = (flowRate * 0.000016667) / PIPE_AREA; else velocity = 0.0;

    // Bernoulli's Principle Implementation
    float maxSourcePressure = 6.0; 

    if (valveState == false || flowRate <= 0.1) {
       pressureBar = maxSourcePressure;
    } 
    else {
       float dynamicPressurePa = 0.5 * 1000.0 * (velocity * velocity);
       float dynamicPressureBar = dynamicPressurePa / 100000.0;        
       
       float totalDrop = dynamicPressureBar * 1.5; 
       pressureBar = maxSourcePressure - totalDrop;
    }

    if (pressureBar < 0) pressureBar = 0.0;

    // Hydraulic Power Calculation (W)
    hydraulicPower = pressureBar * flowRate * 1.666;

    // Autonomous Safety Override
    if (pressureBar > 8.0) {
      alarmState = true;
      
      if (valveState == true) {
        valveState = false;
        digitalWrite(RELAY_PIN, VALVE_CLOSE);
        Serial.println("EMERGENCY: Valve Auto-Closed due to High Pressure!");
        
        if (!emailSent) {
          sendEmailAlert("DANGER: Pressure reached " + String(pressureBar) + " Bar. Valve has been CLOSED AUTOMATICALLY to prevent damage.");
          emailSent = true;
        }
      } 
      else if (!emailSent) {
         sendEmailAlert("WARNING: High Pressure Detected (" + String(pressureBar) + " Bar)!");
         emailSent = true;
      }
    }
    
    // Leak Detection
    if (valveState == false && flowRate > 3.0) {
      alarmState = true;
      if (!emailSent) { sendEmailAlert("LEAK DETECTED! Flow detected while valve is closed."); emailSent = true; }
    }

    flowPulseCount = 0; lastSensorRead = currentMillis;
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);
  }

  if (currentMillis - lastCloudUpload >= 20000) {
    sendToThingSpeak(pressureBar, flowRate, velocity, totalVolume);
    lastCloudUpload = currentMillis;
  }

  // Visual/Audio Alerts
  if (alarmState) {
    bool blink = (millis() / 200) % 2;
    digitalWrite(LED_R, blink ? LOW : HIGH); 
    digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH);
    digitalWrite(BUZZER_PIN, blink ? HIGH : LOW); 
  } else if (valveState) {
    digitalWrite(LED_R, HIGH); digitalWrite(LED_G, LOW); digitalWrite(LED_B, HIGH); 
  } else {
    digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, LOW); 
  }

  // OLED Display Updates
  u8g2.clearBuffer();
  
  if(alarmState) {
     u8g2.setFont(u8g2_font_ncenB10_tr);
     u8g2.setCursor(10, 20); u8g2.print("!! WARNING !!");
     u8g2.setFont(u8g2_font_6x10_tr);
     u8g2.setCursor(5, 40); 
     if(pressureBar > 8.0) u8g2.print("HIGH PRESSURE!");
     else u8g2.print("LEAK DETECTED!");
  } else {
     u8g2.setFont(u8g2_font_6x10_tr);
     u8g2.setCursor(0, 10); u8g2.print("P:"); u8g2.print(pressureBar, 1); u8g2.print(" bar");
     
     u8g2.setFont(u8g2_font_ncenB14_tr);
     u8g2.setCursor(0, 32); u8g2.print(flowRate, 1); 
     u8g2.setFont(u8g2_font_6x10_tr); 
     u8g2.print(" L/m");

     u8g2.setCursor(0, 46); 
     u8g2.print("Vel: "); u8g2.print(velocity, 2); u8g2.print(" m/s");

     u8g2.setCursor(0, 60);
     if(valveState) u8g2.print("VALVE: OPEN"); else u8g2.print("VALVE: CLOSED");
  }
  u8g2.sendBuffer();
}