#include "BluetoothSerial.h"
#include <math.h>

// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

BluetoothSerial SerialBT;

// Pin definitions
const int NORTH_PIN = 33;
const int SOUTH_PIN = 25;
const int EAST_PIN = 21;
const int WEST_PIN = 26;

// Bluetooth device name
const char* DEVICE_NAME = "ESP32_Direction_Control";

// Variables to track pin states
bool northState = false;
bool southState = false;
bool eastState = false;
bool westState = false;

// Spiral search variables
bool spiralSearchActive = false;
float spiralPhase = 0.0;      // 0 to 360 degrees
float baseGrowth = 0.5;      // Base growth rate
float spiralGrowth = 0.5;     // Current growth rate
int speedMultiplier = 1;     // Speed multiplier (1-3x)
int spiralCycles = 0;         // Number of complete rotations
const int CHECK_INTERVAL = 50; // Check every 50ms for smooth transitions
unsigned long lastSpiralUpdateTime = 0;  // Last time spiral was updated

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

void startSpiralSearch() {
  spiralSearchActive = true;
  spiralPhase = 0.0;
  spiralCycles = 0;
  lastSpiralUpdateTime = millis();
  spiralGrowth = baseGrowth * speedMultiplier;
  
  // Start with North
  digitalWrite(NORTH_PIN, HIGH);
  northState = true;
  
  Serial.print("Starting spiral search at speed ");
  Serial.println(speedMultiplier);
}

void stopSpiralSearch() {
  spiralSearchActive = false;
  resetAllPins();
}

void updateSpiralSearch() {
  unsigned long currentTime = millis();
  
  // Update every CHECK_INTERVAL ms
  if (currentTime - lastSpiralUpdateTime >= CHECK_INTERVAL) {
    // Update phase
    spiralPhase += spiralGrowth;
    if (spiralPhase >= 360.0) {
      spiralPhase -= 360.0;
      spiralCycles++;
      // Slow down the spiral as it grows
      spiralGrowth = max(0.1, spiralGrowth * 0.95);
    }
    
    // Calculate pin states based on phase
    float phaseRad = spiralPhase * PI / 180.0;
    bool northActive = sin(phaseRad) > 0;
    bool southActive = sin(phaseRad) < 0;
    bool eastActive = cos(phaseRad) > 0;
    bool westActive = cos(phaseRad) < 0;
    
    // Update pins
    digitalWrite(NORTH_PIN, northActive);
    digitalWrite(SOUTH_PIN, southActive);
    digitalWrite(EAST_PIN, eastActive);
    digitalWrite(WEST_PIN, westActive);
    
    // Update states
    northState = northActive;
    southState = southActive;
    eastState = eastActive;
    westState = westActive;
    
    // Debug output every 500ms
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime >= 500) {
      Serial.print("Spiral: Phase=");
      Serial.print(spiralPhase);
      Serial.print(" Cycles=");
      Serial.print(spiralCycles);
      Serial.print(" Growth=");
      Serial.println(spiralGrowth);
      lastDebugTime = currentTime;
    }
    
    lastSpiralUpdateTime = currentTime;
  }
}

void resetAllPins() {
  // Reset all pins to LOW
  digitalWrite(NORTH_PIN, LOW);
  digitalWrite(SOUTH_PIN, LOW);
  digitalWrite(EAST_PIN, LOW);
  digitalWrite(WEST_PIN, LOW);
  northState = false;
  southState = false;
  eastState = false;
  westState = false;
  Serial.println("All pins reset to LOW");
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000); // Wait for serial to be ready
  Serial.println("\nESP32 Direction Control Starting...");
  SerialBT.begin("ESP32_Direction_Control");

  // Configure pins as outputs
  pinMode(NORTH_PIN, OUTPUT);
  pinMode(SOUTH_PIN, OUTPUT);
  pinMode(EAST_PIN, OUTPUT);
  pinMode(WEST_PIN, OUTPUT);
  
  Serial.println("Pins configured as outputs");

  // Initialize all pins to LOW
  resetAllPins();

  // Register callback for Bluetooth events
  SerialBT.register_callback(btCallback);
}

void setPin(int pin, bool active) {
  if (!isConnected) return;
  digitalWrite(pin, active ? HIGH : LOW);
}

void handleCommand(String command) {
  if (!isConnected) return; // Only process commands when connected

  // Split command into direction and action
  int commaIndex = command.indexOf(',');
  if (commaIndex == -1) return;

  String direction = command.substring(0, commaIndex);
  String action = command.substring(commaIndex + 1);

  // Handle spiral search commands
  if (direction == "SPIRAL") {
    if (action == "STOP") {
      stopSpiralSearch();
      return;
    }
    
    // Check for speed command which has an additional parameter
    int secondComma = action.indexOf(',');
    if (secondComma != -1) {
      String subAction = action.substring(0, secondComma);
      int speed = action.substring(secondComma + 1).toInt();
      
      if (subAction == "START") {
        speedMultiplier = speed;
        spiralGrowth = baseGrowth * speedMultiplier;
        startSpiralSearch();
      } else if (subAction == "SPEED") {
        speedMultiplier = speed;
        spiralGrowth = baseGrowth * speedMultiplier;
        Serial.print("Speed updated to: ");
        Serial.println(speedMultiplier);
      }
    }
    return;
  }

  // If spiral search is active, stop it when any other command is received
  if (spiralSearchActive) {
    stopSpiralSearch();
  }

  // Handle directional commands with digital output
  bool pinState = (action == "START");
  Serial.print("Command: "); Serial.print(direction);
  Serial.print(" Action: "); Serial.print(action);
  Serial.print(" State: "); Serial.println(pinState ? "HIGH" : "LOW");

  if (direction == "NORTH") {
    northState = pinState;
    digitalWrite(NORTH_PIN, pinState ? HIGH : LOW);
  }
  else if (direction == "SOUTH") {
    southState = pinState;
    digitalWrite(SOUTH_PIN, pinState ? HIGH : LOW);
  }
  else if (direction == "EAST") {
    eastState = pinState;
    digitalWrite(EAST_PIN, pinState ? HIGH : LOW);
  }
  else if (direction == "WEST") {
    westState = pinState;
    digitalWrite(WEST_PIN, pinState ? HIGH : LOW);
  }
}

void updatePinStates() {
  // Send state update over Bluetooth and Serial
  Serial.print("States (N,S,E,W): ");
  Serial.print(northState);
  Serial.print(",");
  Serial.print(southState);
  Serial.print(",");
  Serial.print(eastState);
  Serial.print(",");
  Serial.println(westState);
  
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
  static unsigned long lastDebugTime = 0;
  
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    Serial.print("Received command: "); Serial.println(command);
    handleCommand(command);
  }
  
  // Print debug info every second
  if (millis() - lastDebugTime >= 1000) {
    Serial.println("\nStatus:");
    Serial.print("Connected: "); Serial.println(isConnected ? "Yes" : "No");
    Serial.print("Spiral Active: "); Serial.println(spiralSearchActive ? "Yes" : "No");
    updatePinStates();
    lastDebugTime = millis();
  }

  // Handle spiral search movement
  if (spiralSearchActive) {
    updateSpiralSearch();
  }

  delay(1); // Minimal delay for system stability
}
