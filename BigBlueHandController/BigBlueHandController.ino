#include "BluetoothSerial.h"

// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

BluetoothSerial SerialBT;

// Pin definitions for directional outputs
const int NORTH_PIN = 33;  // GPIO33
const int SOUTH_PIN = 25;  // GPIO25
const int EAST_PIN = 21;   // GPIO21
const int WEST_PIN = 26;   // GPIO26

// Bluetooth device name
const char* DEVICE_NAME = "ESP32_Direction_Control";

// Variables to track pin states
bool northState = false;
bool southState = false;
bool eastState = false;
bool westState = false;



// Connection state tracking
bool isConnected = false;

// Callback for Bluetooth events
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println("Bluetooth connected");
    isConnected = true;
  } else if (event == ESP_SPP_CLOSE_EVT) {
    Serial.println("Bluetooth disconnected");
    isConnected = false;
    // Reset all pins on disconnection
    resetAllPins();
  }
}

void resetAllPins() {
  // Reset all pins to LOW and update states
  digitalWrite(NORTH_PIN, LOW);
  digitalWrite(SOUTH_PIN, LOW);
  digitalWrite(EAST_PIN, LOW);
  digitalWrite(WEST_PIN, LOW);
  northState = false;
  southState = false;
  eastState = false;
  westState = false;
}

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  
  // Initialize Bluetooth serial with callback
  SerialBT.begin(DEVICE_NAME);
  SerialBT.register_callback(btCallback);
  
  // Configure all pins as outputs with internal pulldown
  pinMode(NORTH_PIN, OUTPUT);
  pinMode(SOUTH_PIN, OUTPUT);
  pinMode(EAST_PIN, OUTPUT);
  pinMode(WEST_PIN, OUTPUT);
  
  // Initially reset all pins
  resetAllPins();
  
  // Initialize with a delay to allow system to stabilize
  
  delay(1000);
}

void setPin(int pin, bool active) {
  if (!isConnected) return;
  digitalWrite(pin, active ? HIGH : LOW);
}

void handleCommand(String command) {
  // Split command into parts
  int firstComma = command.indexOf(',');
  if (firstComma == -1) return;
  
  String direction = command.substring(0, firstComma);
  String action = command.substring(firstComma + 1);
  bool isStart = (action == "START");
  
  int pin;
  bool* stateVar;
  
  // Quick direction check using first character
  switch(direction[0]) {
    case 'N': // NORTH
      pin = NORTH_PIN;
      stateVar = &northState;
      break;
    case 'S': // SOUTH
      pin = SOUTH_PIN;
      stateVar = &southState;
      break;
    case 'E': // EAST
      pin = EAST_PIN;
      stateVar = &eastState;
      break;
    case 'W': // WEST
      pin = WEST_PIN;
      stateVar = &westState;
      break;
    default:
      return;
  }
  
  setPin(pin, isStart);
  *stateVar = isStart;
}

void updatePinStates() {
  // Update state variables for debugging
  northState = digitalRead(NORTH_PIN);
  southState = digitalRead(SOUTH_PIN);
  eastState = digitalRead(EAST_PIN);
  westState = digitalRead(WEST_PIN);
  
  // Send state update over Bluetooth
  SerialBT.print("States (N,S,E,W): ");
  SerialBT.print(northState);
  SerialBT.print(",");
  SerialBT.print(southState);
  SerialBT.print(",");
  SerialBT.print(eastState);
  SerialBT.print(",");
  SerialBT.println(westState);
}

void loop() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    if (command.length() > 0) {
      command.trim();
      handleCommand(command);
    }
  }
  delay(5);
}
