#include "Adafruit_VL53L0X.h"
#include "ESP32_NOW.h"
#include "WiFi.h"
#include <ESP_NOW_Broadcast_Peer.h>;
#include <esp_mac.h>  // For the MAC2STR and MACSTR macros

//#define SENSORPLACEMENT "SensorBottom"
#define SENSORPLACEMENT "SensorTop"

#define ESPNOW_WIFI_CHANNEL 6

ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

const unsigned long SensorReadinterval = 100; // Interval in milliseconds
unsigned long previousMillis = 0;   // Store the last time the task was executed

VL53L0X_RangingMeasurementData_t measure;

const uint16_t MIN_DISTANCE = 50; // Minimum distance for detection (5 cm)
const uint16_t MAX_DISTANCE = 800; // Maximum distance for detection (80 cm)

uint16_t rangeValue;

float alpha = 0.9; // Smoothing factor (0 < alpha ≤ 1)
float ema = 0;     // Initial EMA value (can be set to the first measurement)
bool firstMeasurement = true; // Flag for first measurement

void setup() {
  Serial.begin(115200);
    // wait until serial port opens for native USB devices
  while (! Serial) {
    delay(1);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("ESP-NOW Example - Broadcast Master");
  Serial.println("Wi-Fi parameters:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // Register the broadcast peer
  if (!broadcast_peer.begin()) {
    Serial.println("Failed to initialize broadcast peer");
    Serial.println("Reebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }

  lox.startRangeContinuous();
}

bool personDetected = false;

void loop() {

  if (lox.isRangeComplete())
  {
     // lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
      rangeValue = lox.readRange();
      if (firstMeasurement)
      {
        ema = rangeValue;
        firstMeasurement = false;
      } 
      else
      {
        ema = alpha * rangeValue + (1 - alpha) * ema;
      }
  }
  
    
 unsigned long currentMillis = millis(); 

   
    if (currentMillis - previousMillis >= SensorReadinterval) {
        previousMillis = currentMillis; 

      if (ema >= MIN_DISTANCE && ema <= MAX_DISTANCE) 
      {
        
        if(personDetected == false)
        {
           digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            Serial.println("Person detected!");
            char data[64];
            snprintf(data, sizeof(data), "%s,%s,%u", SENSORPLACEMENT , "Person_Detected",static_cast<uint16_t>(ema));
            if (!broadcast_peer.send_message((uint8_t *)data, sizeof(data)))
            {
              Serial.println("Failed to broadcast message");
            }  
            personDetected = true;
        }
      }
      else if(ema <= MIN_DISTANCE || ema >= MAX_DISTANCE)
      {
        if(personDetected == true)
        {
           digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            Serial.println("No person detected.");
            personDetected = false;
            char data[64] = "SensorTop = Out of range";
             snprintf(data, sizeof(data), "%s,%s,%u", SENSORPLACEMENT , "No_Person",static_cast<uint16_t>(ema));
            if (!broadcast_peer.send_message((uint8_t *)data, sizeof(data))) 
            {
              Serial.println("Failed to broadcast message");
            }
        }
      }

    }
}
