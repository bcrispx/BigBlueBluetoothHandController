#include "BluetoothSerial.h"

// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Bluetooth Serial object
BluetoothSerial SerialBT;

// Pin definitions for direction control
const int PIN_NORTH = 2;  // GPIO2
const int PIN_SOUTH = 8;  // GPIO8
const int PIN_EAST = 12;  // GPIO12
const int PIN_WEST = 13;  // GPIO13

// Pin states
bool northActive = false;
bool southActive = false;
bool eastActive = false;
bool westActive = false;

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  
  // Initialize Bluetooth serial
  SerialBT.begin("ESP32_Direction_Control");
  Serial.println("Bluetooth device is ready to pair");

  // Configure pins as outputs
  pinMode(PIN_NORTH, OUTPUT);
  pinMode(PIN_SOUTH, OUTPUT);
  pinMode(PIN_EAST, OUTPUT);
  pinMode(PIN_WEST, OUTPUT);

  // Initialize all pins to LOW
  digitalWrite(PIN_NORTH, LOW);
  digitalWrite(PIN_SOUTH, LOW);
  digitalWrite(PIN_EAST, LOW);
  digitalWrite(PIN_WEST, LOW);
}

void processCommand(String command) {
  // Split command into direction and action
  int commaIndex = command.indexOf(',');
  if (commaIndex == -1) return;

  String direction = command.substring(0, commaIndex);
  String action = command.substring(commaIndex + 1);
  
  bool isStart = (action == "START");
  int pin = -1;
  bool* activeState = nullptr;

  // Determine which pin to control
  if (direction == "NORTH") {
    pin = PIN_NORTH;
    activeState = &northActive;
  } else if (direction == "SOUTH") {
    pin = PIN_SOUTH;
    activeState = &southActive;
  } else if (direction == "EAST") {
    pin = PIN_EAST;
    activeState = &eastActive;
  } else if (direction == "WEST") {
    pin = PIN_WEST;
    activeState = &westActive;
  }

  // Update pin state if valid
  if (pin != -1 && activeState != nullptr) {
    digitalWrite(pin, isStart ? HIGH : LOW);
    *activeState = isStart;
    
    // Debug output
    Serial.print("Pin ");
    Serial.print(pin);
    Serial.print(" set to ");
    Serial.println(isStart ? "HIGH" : "LOW");
  }
}

void loop() {
  // Check if data is available
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();  // Remove any whitespace/newlines
    
    Serial.print("Received command: ");
    Serial.println(command);
    
    processCommand(command);
  }
  
  // Optional: Add a small delay to prevent overwhelming the CPU
  delay(10);
}
