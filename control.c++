#include <AFMotor.h>
#include <SoftwareSerial.h>
#include <Servo.h>

// ====== BLUETOOTH SETUP ======
SoftwareSerial bluetoothSerial(9, 10); // RX, TX

// ====== MOTOR SETUP ======
AF_DCMotor motor1(1, MOTOR12_1KHZ);
AF_DCMotor motor2(2, MOTOR12_1KHZ);
AF_DCMotor motor3(3, MOTOR34_1KHZ);
AF_DCMotor motor4(4, MOTOR34_1KHZ);

// ====== SERVO SETUP ======
Servo lockServo;
const int servoPin = A0;
const int lockPos = 0;
const int openPos = 90;

// ====== SECURITY VARIABLES ======
String inputPasscode = "";
const String correctPasscode = "1234";
bool accessGranted = false;
const int maxPasscodeLength = 8;
const int maxAttempts = 3;
int failedAttempts = 0;
unsigned long lockoutEndTime = 0;
const unsigned long lockoutDuration = 30000; // 30 seconds lockout

// ====== MOTOR CONTROL VARIABLES ======
int motorSpeed = 200; // Default speed (0-255)
const int minSpeed = 100;
const int maxSpeed = 255;

// ====== AUTO-LOCK FEATURE ======
unsigned long lastActivityTime = 0;
const unsigned long autoLockTimeout = 60000; // 60 seconds of inactivity

char command;

void setup() {
  Serial.begin(9600); // For debugging
  bluetoothSerial.begin(9600);
  
  lockServo.attach(servoPin);
  lockServo.write(lockPos);
  
  bluetoothSerial.println("=== System Ready ===");
  bluetoothSerial.println("Enter passcode + '#'");
  
  lastActivityTime = millis();
}

void loop() {
  // Check for auto-lock timeout
  if (accessGranted && (millis() - lastActivityTime > autoLockTimeout)) {
    bluetoothSerial.println("⏱️ Auto-lock activated");
    lockSystem();
  }
  
  // Check if in lockout period
  if (isLockedOut()) {
    unsigned long remaining = (lockoutEndTime - millis()) / 1000;
    if (bluetoothSerial.available()) {
      bluetoothSerial.read(); // Discard input
      bluetoothSerial.print("🚫 Locked out. Wait ");
      bluetoothSerial.print(remaining);
      bluetoothSerial.println(" seconds");
    }
    return;
  }
  
  if (bluetoothSerial.available() > 0) {
    command = bluetoothSerial.read();
    lastActivityTime = millis(); // Update activity timer
    
    // --- Control commands (only when access granted) ---
    if (accessGranted) {
      Stop(); // Stop before executing new command
      
      switch (command) {
        case 'F': forward(); break;
        case 'B': back(); break;
        case 'L': left(); break;
        case 'R': right(); break;
        case 'S': Stop(); break;
        
        // Speed control
        case '+': increaseSpeed(); break;
        case '-': decreaseSpeed(); break;
        
        // Manual lock
        case 'X': lockSystem(); break;
        
        // Status query
        case '?': sendStatus(); break;
      }
    }
    
    // --- Passcode input (0-9 or '#') ---
    if (isDigit(command)) {
      if (inputPasscode.length() < maxPasscodeLength) {
        inputPasscode += command;
        bluetoothSerial.print("* "); // Echo asterisk for privacy
        bluetoothSerial.print(inputPasscode.length());
        bluetoothSerial.println(" digits entered");
      } else {
        bluetoothSerial.println("⚠️ Max length reached. Press '#'");
      }
    }
    else if (command == '#') {
      checkPasscode();
    }
    else if (command == '*') { // Clear input
      inputPasscode = "";
      bluetoothSerial.println("🔄 Input cleared");
    }
  }
}

// ====== SECURITY FUNCTIONS ======
bool isLockedOut() {
  if (failedAttempts >= maxAttempts) {
    if (millis() < lockoutEndTime) {
      return true;
    } else {
      failedAttempts = 0; // Reset after lockout expires
    }
  }
  return false;
}

void checkPasscode() {
  if (inputPasscode.length() == 0) {
    bluetoothSerial.println("⚠️ No input entered");
    inputPasscode = "";
    return;
  }
  
  if (inputPasscode == correctPasscode) {
    bluetoothSerial.println("✅ Access Granted");

bluetoothSerial.println("Controls: F=Forward, B=Back, L=Left, R=Right, S=Stop");
    bluetoothSerial.println("Speed: +=Faster, -=Slower");
    bluetoothSerial.println("X=Lock, ?=Status");
    accessGranted = true;
    failedAttempts = 0;
    unlockServo();
    lastActivityTime = millis();
  } else {
    failedAttempts++;
    bluetoothSerial.print("❌ Wrong Passcode (");
    bluetoothSerial.print(failedAttempts);
    bluetoothSerial.print("/");
    bluetoothSerial.print(maxAttempts);
    bluetoothSerial.println(")");
    
    if (failedAttempts >= maxAttempts) {
      lockoutEndTime = millis() + lockoutDuration;
      bluetoothSerial.println("🚫 Too many attempts! Locked for 30s");
    }
    
    accessGranted = false;
    lockServo.write(lockPos);
  }
  inputPasscode = "";
}

void lockSystem() {
  bluetoothSerial.println("🔒 System Locked");
  accessGranted = false;
  failedAttempts = 0;
  inputPasscode = "";
  lockServo.write(lockPos);
  Stop();
}

void unlockServo() {
  bluetoothSerial.println("🔓 Unlocking...");
  lockServo.write(openPos);
  delay(500);
}

void sendStatus() {
  bluetoothSerial.println("--- Status ---");
  bluetoothSerial.print("Access: ");
  bluetoothSerial.println(accessGranted ? "Granted" : "Denied");
  bluetoothSerial.print("Speed: ");
  bluetoothSerial.print((motorSpeed * 100) / 255);
  bluetoothSerial.println("%");
  bluetoothSerial.print("Failed Attempts: ");
  bluetoothSerial.println(failedAttempts);
}

// ====== MOTOR CONTROL FUNCTIONS ======
void increaseSpeed() {
  if (motorSpeed < maxSpeed) {
    motorSpeed = min(motorSpeed + 50, maxSpeed);
    bluetoothSerial.print("⚡ Speed: ");
    bluetoothSerial.print((motorSpeed * 100) / 255);
    bluetoothSerial.println("%");
  }
}

void decreaseSpeed() {
  if (motorSpeed > minSpeed) {
    motorSpeed = max(motorSpeed - 50, minSpeed);
    bluetoothSerial.print("🐌 Speed: ");
    bluetoothSerial.print((motorSpeed * 100) / 255);
    bluetoothSerial.println("%");
  }
}

void setAllMotors(int speed, int direction) {
  motor1.setSpeed(speed);
  motor1.run(direction);
  motor2.setSpeed(speed);
  motor2.run(direction);
  motor3.setSpeed(speed);
  motor3.run(direction);
  motor4.setSpeed(speed);
  motor4.run(direction);
}

void forward() {
  setAllMotors(motorSpeed, FORWARD);
}

void back() {
  setAllMotors(motorSpeed, BACKWARD);
}

void left() {
  motor1.setSpeed(motorSpeed);
  motor1.run(BACKWARD);
  motor2.setSpeed(motorSpeed);
  motor2.run(BACKWARD);
  motor3.setSpeed(motorSpeed);
  motor3.run(FORWARD);
  motor4.setSpeed(motorSpeed);
  motor4.run(FORWARD);
}

void right() {
  motor1.setSpeed(motorSpeed);
  motor1.run(FORWARD);
  motor2.setSpeed(motorSpeed);
  motor2.run(FORWARD);
  motor3.setSpeed(motorSpeed);
  motor3.run(BACKWARD);
  motor4.setSpeed(motorSpeed);
  motor4.run(BACKWARD);
}

void Stop() {
  setAllMotors(0, RELEASE);
}