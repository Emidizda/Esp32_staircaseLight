/*
    ESP-NOW Broadcast Slave
    Lucas Saavedra Vaz - 2024

    This sketch demonstrates how to receive broadcast messages from a master device using the ESP-NOW protocol.

    The master device will broadcast a message every 5 seconds to all devices within the network.

    The slave devices will receive the broadcasted messages. If they are not from a known master, they will be registered as a new master
    using a callback function.
*/

#include "ESP32_NOW.h"
#include "WiFi.h"
#include "FastLED.h"
#include <TaskScheduler.h>
#include <esp_mac.h>  // For the MAC2STR and MACSTR macros

/* Definitions */

#define ESPNOW_WIFI_CHANNEL 6

String sensorPlacement, detection;
uint16_t distanceDetected;

#define TimeMsBetweenStair 300

#define LED_PIN         4
#define NUM_LEDS        352
#define NUM_LEDS_STEP   32
#define BRIGHTNESS      120
#define LED_TYPE        WS2811
#define COLOR_ORDER     GRB
CRGB leds[NUM_LEDS];

int stepSize = NUM_LEDS_STEP; // Number of LEDs per step
int numSteps = NUM_LEDS / stepSize;
int stepIndex = 0;

Scheduler runner;

void RunStairsStateMachine();
Task RunStateMachineTask(TimeMsBetweenStair, TASK_FOREVER, &RunStairsStateMachine);

void StoppingTask();
Task DelayStoppingTask(1000, TASK_FOREVER, &StoppingTask);

void lightStairs(bool fromBottom, int stepIndex);
void TurnOffLight();
void RunLightsFrom(String FromSensor, String Detection, uint16_t distance);

enum DeviceState {
    Stopped,
    RunningFromBottom,
    RunningFromTopp,
    Stopping, 
    StoppingFromTop,
    StoppingFromBottom
};

DeviceState deviceState = Stopped;

void parseData(const char *data, String &var1, String &var2, uint16_t &var3) {
    // Convert to String for easier manipulation
    String rawData = String(data);

    // Remove unwanted characters (e.g., non-alphanumeric and punctuation except ',')
    for (int i = 0; i < rawData.length(); i++) {
        if (!isalnum(rawData[i]) && rawData[i] != ',' && rawData[i] != '_') {
            rawData[i] = ' '; // Replace with space for trimming
        }
    }

    // Split the cleaned data using commas
    int firstComma = rawData.indexOf(',');
    int secondComma = rawData.indexOf(',', firstComma + 1);

    if (firstComma != -1 && secondComma != -1) {
        // Extract the first string
        var1 = rawData.substring(0, firstComma);
        var1.trim(); // Ensure no leading/trailing spaces

        // Extract the second string
        var2 = rawData.substring(firstComma + 1, secondComma);
        var2.trim(); // Ensure no leading/trailing spaces

        // Extract the number
        String numberStr = rawData.substring(secondComma + 1);
        numberStr.trim(); // Ensure no leading/trailing spaces
        var3 = (uint16_t)numberStr.toInt();
    } else {
        // Handle errors
        Serial.println("Error: Data is not in the expected format.");
    }
}

/* Callbacks */

// Callback called when an unknown peer sends a message
void register_new_master(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg)
{
  if (memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, 6) == 0) 
  {
    // Serial.printf("broadcast message mac adress: \n", MAC2STR(info->src_addr));
    // Serial.println("Registering the peer as a master");
    char dataToParse[len]; 
    for(int i = 0; i < len; i++)
    {
      dataToParse[i] = data[i];
    }

     parseData(dataToParse, sensorPlacement,detection, distanceDetected);

    if(RunStateMachineTask.isEnabled() || DelayStoppingTask.isEnabled())
    {
      Serial.println("State Machine task is enabled");
    }
    else
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      Serial.println("Task is not enabled");
      RunLightsFrom(sensorPlacement, detection, distanceDetected);
    }
   
    // Serial.println("Parsed Variables:");
    // Serial.println("String 1: " + sensorPlacement);
    // Serial.println("String 2: " + detection);
    // Serial.println("Uint16: " + String(distanceDetected));
  
  }
}
/* Main */

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize the Wi-Fi module
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("ESP-NOW Example - Broadcast Slave");
  Serial.println("Wi-Fi parameters:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // Initialize the ESP-NOW protocol
  if (!ESP_NOW.begin()) {
    Serial.println("Failed to initialize ESP-NOW");
    Serial.println("Reeboting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // Register the new peer callback
  ESP_NOW.onNewPeer(register_new_master, NULL);

  runner.init();

  runner.addTask(RunStateMachineTask);
  RunStateMachineTask.disable();

  runner.addTask(DelayStoppingTask);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  TurnOffLight();

  Serial.println("Setup complete. Waiting for a master to broadcast a message...");
}

void loop()
{
   runner.execute();
}


void RunLightsFrom(String FromSensor, String Detection, uint16_t distance)
{
   if(FromSensor == "SensorTop")
   {
      Serial.println("Sensor Top triggered");
      if(detection == "Person_Detected")
      {
        Serial.println("Person Detected on top");
        deviceState = RunningFromTopp;
        RunStateMachineTask.enable();
      }
      else if(detection == "No_Person")
      {
        Serial.println("Person left top sensor");
      }
   }
   else if(FromSensor == "SensorBottom")
   {
     Serial.println("Sensor Bottom triggered");
     if(detection == "Person_Detected")
     {
        Serial.println("Person Detected on Bottom");
        deviceState = RunningFromBottom;
        RunStateMachineTask.enable();
     }
     else if(detection == "No_Person")
     {
        Serial.println("Person left bottom sensor");
     }
   }
   else {
     Serial.println("No Sensor");
   }
}

void lightStairs(bool fromBottom, int stepIndex) {
 
  // for (int i = 0; i < numSteps; i++) {
    int startLED = fromBottom ? stepIndex * stepSize : NUM_LEDS - ((stepIndex + 1) * stepSize);
    Serial.print("START LED: " );
    Serial.println(startLED);
    for (int j = startLED; j < startLED + stepSize; j++) {
      leds[j] = CRGB::White; // Set color
    }
    FastLED.show();
   
   //}
}


void StoppingTask()
{
  Serial.println("STOPPING");
  if(deviceState == StoppingFromBottom)
  {
      for(int i = 0; i < NUM_LEDS; i++)
      {
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(10);
      }
  }
  else if(deviceState == StoppingFromTop)
  {
    Serial.println("Stoping from top");
      for(int j = NUM_LEDS - 1; j >= 0; j--)
      {
        leds[j] = CRGB::Black;
        FastLED.show();
        delay(10);
      }
  }
  
  deviceState = Stopped;
  DelayStoppingTask.disable();
  Serial.println("STOPPED!");
}

void TurnOffLight()
{
  for(int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Black;
  }
   FastLED.show();
    delay(10);
}

void RunStairsStateMachine()
{
    switch (deviceState)
    {
    case Stopped:
      Serial.println("Stopped");
      RunStateMachineTask.disable();
      break;
    case RunningFromBottom:
      Serial.println("Bunn Swith");
      lightStairs(true, stepIndex);
      if(stepIndex == numSteps -1 )
      {
        stepIndex = 0;
        deviceState = StoppingFromBottom;
        DelayStoppingTask.enableDelayed(5000);
        RunStateMachineTask.disable();
      }
      else{
        stepIndex++;
      }
      break;
    case RunningFromTopp:
      Serial.println("Top Swith");
      lightStairs(false, stepIndex);
      if(stepIndex == numSteps - 1)
      {
        stepIndex = 0;
        deviceState = StoppingFromTop;
        DelayStoppingTask.enableDelayed(5000);
        RunStateMachineTask.disable();
      }
      else{
        stepIndex++;
      }
      break;
    case Stopping: 
      Serial.println("Stopp");
    break;
    default:
      break;
    }
}


