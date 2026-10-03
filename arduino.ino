#include <WiFi101.h>

// wifi module pins
const int WINC_CS  = 8, WINC_IRQ = 7, WINC_RST = 4, WINC_EN = 2;

bool emergencyStopActive = false;
bool obstacleDetectionEnabled = true; 

long ultrasonicDuration = 0;
float ultrasonicDistance = 0;

const float OBSTACLE_DISTANCE = 20.0; // cm

const int PIN_ULTRASONIC_TRIG = 0;
const int PIN_ULTRASONIC_ECHO = 1; 

// motors
const int FL_PWM = 6,  FL_DIR = 5;     // front left
const int FR_PWM = 9,  FR_DIR = 10;    // front right
const int BL_PWM = A4, BL_DIR = A5;    // back left
const int BR_PWM = 11, BR_DIR = 12;    // back right

// ir sensors
const int PIN_IR_LEFT_DIGITAL = A0;
const int PIN_IR_LEFT_ANALOG = A1;
const int PIN_IR_RIGHT_DIGITAL = A2;
const int PIN_IR_RIGHT_ANALOG = A3;

// sensor values
int irLeftDigital = 0;
int irRightDigital = 0;
int irLeftAnalog = 0;
int irRightAnalog = 0;

int readjustTimeMS = 350; // time to wait when turning
int motorSpeed = 100;

int motorSpeedStrafe = motorSpeed + 40; // strafe needs more power
char lastMotionCmd = 'x';

bool lineFollowingMode = false;
int lfBaseValue = 30;
enum Direction{
  LEFT = -1,
  NONE = 0,
  RIGHT = 1
};

Direction lastTriggeredSensor = NONE;

bool kidnappedMode = false;

// kidnapped mode line search
bool kidnappedForwardPhase = false;
unsigned long kidnappedPhaseStartMs = 0;
int kidnappedLoops = 0;

// kidnapped mode tuning
const unsigned long KIDNAP_360_MS = 6000;      // however long it takes to do a 360
const unsigned long KIDNAP_STEP_BASE_MS = 300; // first outward bump
const unsigned long KIDNAP_STEP_INC_MS  = 200;  // make spiral bigger with each loop

// manoeuvre stuff
bool manoeuvreActive = false;
int currentManoeuvre = 0;
int END_THRESHOLD = 50; // how long to wait before deciding we lost the line

// loop detection
int consecutiveLeftTurns = 0;
int consecutiveRightTurns = 0;
unsigned long lastIntersectionTime = 0;
unsigned long straightTravelTime = 0;
const int LOOP_THRESHOLD = 4;
const unsigned long MIN_INTERSECTION_INTERVAL = 2000;
const unsigned long RESET_TRAVEL_TIME = 3000;
int currentTurnStrategy = 0; // 0=left, 1=straight, 2=right

// parking states
enum ParkingState { M5_APPROACHING, M5_TURNING_IN, M5_ENTERING, M5_PARKED };
ParkingState parkingState = M5_APPROACHING;

// wifi
const char ssid[] = "Group24";
const char pass[] = "group24test";
WiFiServer server(80);

// convert ip to string for printing
String ipToString(const IPAddress& ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void setupMotor(int pwm, int dir) {
  pinMode(pwm, OUTPUT);
  pinMode(dir, OUTPUT);
}

void setMotor(int pwm, int dir, int speed, bool forward) {
  digitalWrite(dir, forward ? HIGH : LOW);
  analogWrite(pwm, speed);
}

// stops everything and resets all modes
void stopAllMotors() {
  lineFollowingMode = false;
  kidnappedMode = false;
  manoeuvreActive = false;
  currentManoeuvre = 0;
  lastTriggeredSensor = NONE;
  consecutiveLeftTurns = 0;
  consecutiveRightTurns = 0;
  currentTurnStrategy = 0;
  straightTravelTime = 0;
  
  setMotor(FL_PWM, FL_DIR, 0, true);
  setMotor(FR_PWM, FR_DIR, 0, true);
  setMotor(BL_PWM, BL_DIR, 0, true);
  setMotor(BR_PWM, BR_DIR, 0, true);
}

// just stops motors without resetting modes
void stopMotorsOnly() {
  setMotor(FL_PWM, FL_DIR, 0, true);
  setMotor(FR_PWM, FR_DIR, 0, true);
  setMotor(BL_PWM, BL_DIR, 0, true);
  setMotor(BR_PWM, BR_DIR, 0, true);
}

// movement functions
void moveBackward() {
  emergencyStopActive = false;
  setMotor(FL_PWM, FL_DIR, motorSpeed, true);
  setMotor(FR_PWM, FR_DIR, motorSpeed, true);
  setMotor(BL_PWM, BL_DIR, motorSpeed, true);
  setMotor(BR_PWM, BR_DIR, motorSpeed, true);
}

void moveForward() {
  emergencyStopActive = false;
  setMotor(FL_PWM, FL_DIR, motorSpeed, false);
  setMotor(FR_PWM, FR_DIR, motorSpeed, false);
  setMotor(BL_PWM, BL_DIR, motorSpeed, false);
  setMotor(BR_PWM, BR_DIR, motorSpeed, false);
}

void rotateCCW() {
  emergencyStopActive = false;
  setMotor(FL_PWM, FL_DIR, motorSpeed, true);
  setMotor(FR_PWM, FR_DIR, motorSpeed, false);
  setMotor(BL_PWM, BL_DIR, motorSpeed, true);
  setMotor(BR_PWM, BR_DIR, motorSpeed, false);
}

void rotateCW() {
  emergencyStopActive = false;
  setMotor(FL_PWM, FL_DIR, motorSpeed, false);
  setMotor(FR_PWM, FR_DIR, motorSpeed, true);
  setMotor(BL_PWM, BL_DIR, motorSpeed, false);
  setMotor(BR_PWM, BR_DIR, motorSpeed, true);
}

void strafeLeft() {
  emergencyStopActive = false;
  setMotor(FL_PWM, FL_DIR, motorSpeedStrafe, true);
  setMotor(FR_PWM, FR_DIR, motorSpeedStrafe, false);
  setMotor(BL_PWM, BL_DIR, motorSpeedStrafe, false);
  setMotor(BR_PWM, BR_DIR, motorSpeedStrafe, true);
}

void strafeRight() {
  emergencyStopActive = false;
  setMotor(FL_PWM, FL_DIR, motorSpeedStrafe, false);
  setMotor(FR_PWM, FR_DIR, motorSpeedStrafe, true);
  setMotor(BL_PWM, BL_DIR, motorSpeedStrafe, true);
  setMotor(BR_PWM, BR_DIR, motorSpeedStrafe, false);
}

// slower versions for precision
void moveBackwardSlow() {
  setMotor(FL_PWM, FL_DIR, motorSpeed * 0.7, true);
  setMotor(FR_PWM, FR_DIR, motorSpeed * 0.7, true);
  setMotor(BL_PWM, BL_DIR, motorSpeed * 0.7, true);
  setMotor(BR_PWM, BR_DIR, motorSpeed * 0.7, true);
}

void moveForwardSlow() {
  setMotor(FL_PWM, FL_DIR, motorSpeed * 0.7, false);
  setMotor(FR_PWM, FR_DIR, motorSpeed * 0.7, false);
  setMotor(BL_PWM, BL_DIR, motorSpeed * 0.7, false);
  setMotor(BR_PWM, BR_DIR, motorSpeed * 0.7, false);
}

// steering - one side goes slower
void steerLeft() {
  setMotor(FL_PWM, FL_DIR, motorSpeed * 0.6, false);
  setMotor(FR_PWM, FR_DIR, motorSpeed, false);
  setMotor(BL_PWM, BL_DIR, motorSpeed * 0.6, false);
  setMotor(BR_PWM, BR_DIR, motorSpeed, false);
}

void steerRight() {
  setMotor(FL_PWM, FL_DIR, motorSpeed, false);
  setMotor(FR_PWM, FR_DIR, motorSpeed * 0.6, false);
  setMotor(BL_PWM, BL_DIR, motorSpeed, false);
  setMotor(BR_PWM, BR_DIR, motorSpeed * 0.6, false);
}

void steerLeftReverse() {
  setMotor(FL_PWM, FL_DIR, motorSpeed * 0.5, true);
  setMotor(FR_PWM, FR_DIR, motorSpeed * 0.7, true);
  setMotor(BL_PWM, BL_DIR, motorSpeed * 0.5, true);
  setMotor(BR_PWM, BR_DIR, motorSpeed * 0.7, true);
}

void steerRightReverse() {
  setMotor(FL_PWM, FL_DIR, motorSpeed * 0.7, true);
  setMotor(FR_PWM, FR_DIR, motorSpeed * 0.5, true);
  setMotor(BL_PWM, BL_DIR, motorSpeed * 0.7, true);
  setMotor(BR_PWM, BR_DIR, motorSpeed * 0.5, true);
}

void rotate90CW() {
  rotateCW();
  delay(400);
  stopMotorsOnly();
}

void rotate90CCW() {
  rotateCCW();
  delay(400);
  stopMotorsOnly();
}

void rotate180() {
  rotateCW();
  delay(800);
  stopMotorsOnly();
}

void spinUntilLineFound(bool spinLeft) {
  if (spinLeft) {
    rotateCCW();
  } else {
    rotateCW();
  }
  delay(300);
  
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    irLeftDigital = digitalRead(PIN_IR_LEFT_DIGITAL);
    irRightDigital = digitalRead(PIN_IR_RIGHT_DIGITAL);
    
    if (irLeftDigital == HIGH || irRightDigital == HIGH) {
      stopMotorsOnly();
      return;
    }
    
    if (spinLeft) {
      rotateCCW();
    } else {
      rotateCW();
    }
    delay(10);
  }
  
  stopMotorsOnly();
}

void spinUntilLineConfirmed(bool spinLeft) {
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    irLeftDigital = digitalRead(PIN_IR_LEFT_DIGITAL);
    irRightDigital = digitalRead(PIN_IR_RIGHT_DIGITAL);
    
    if (spinLeft) {
      if (irRightDigital == HIGH) {
        stopMotorsOnly();
        moveForward();
        return;
      }
    } else {
      if (irLeftDigital == HIGH) {
        stopMotorsOnly();
        moveForward();
        return;
      }
    }
    
    if (spinLeft) {
      rotateCCW();
    } else {
      rotateCW();
    }
    delay(10);
  }
  
  stopMotorsOnly();
}

void lineFollowing() {
  lineFollowingMode = !lineFollowingMode;
  if (!lineFollowingMode) stopAllMotors();
}

// kidnapped mode - spin until we find the line again
void kidnappedControl() {
  if (irLeftDigital == HIGH || irRightDigital == HIGH) {
    // found it!
    kidnappedMode = false;
    stopMotorsOnly();
    return;
  }

  unsigned long now = millis();

  if (!kidnappedForwardPhase) {
    // rotate 360
    rotateCW();
    if (now - kidnappedPhaseStartMs >= KIDNAP_360_MS) {
      stopMotorsOnly();
      delay(50);
      kidnappedForwardPhase = true;
      kidnappedPhaseStartMs = millis();
    }
  } else {
    // move outward a bit
    moveForwardSlow();
    unsigned long stepMs = KIDNAP_STEP_BASE_MS + (unsigned long)kidnappedLoops * KIDNAP_STEP_INC_MS;

    if (now - kidnappedPhaseStartMs >= stepMs) {
      stopMotorsOnly();
      delay(50);
      kidnappedForwardPhase = false;
      kidnappedPhaseStartMs = millis();
      kidnappedLoops++;
    }
  }
}


void startKidnappedMode() {
  kidnappedMode = true;
  lineFollowingMode = false;
  manoeuvreActive = false;
  currentManoeuvre = 0;
  emergencyStopActive = false;

  kidnappedForwardPhase = false;
  kidnappedPhaseStartMs = millis();
  kidnappedLoops = 0;

  rotateCW();
}


// M1 - reverse along line until we reach the end
void manoeuvre1_ReverseStraight() {
  static int m1LostCounter = 0;
  static bool oscillatingLeft = true;
  static unsigned long lastOscillateTime = 0;
  static int oscillateCount = 0;
  const int OSCILLATE_INTERVAL = 300;
  const int MAX_OSCILLATE_WITHOUT_LINE = 4;
  
  bool lineDetected = (irLeftDigital == HIGH || irRightDigital == HIGH);
  
  if (lineDetected) {
    m1LostCounter = 0;
    oscillateCount = 0;
    moveBackwardSlow();
    return;
  }
  
  unsigned long now = millis();
  
  if (now - lastOscillateTime > OSCILLATE_INTERVAL) {
    lastOscillateTime = now;
    oscillateCount++;
    oscillatingLeft = !oscillatingLeft;
    
    if (oscillateCount > MAX_OSCILLATE_WITHOUT_LINE) {
      // probably at the end
      stopMotorsOnly();
      manoeuvreActive = false;
      currentManoeuvre = 0;
      m1LostCounter = 0;
      oscillateCount = 0;
      return;
    }
  }
  
  if (oscillatingLeft) {
    setMotor(FL_PWM, FL_DIR, motorSpeed * 0.5, true);
    setMotor(FR_PWM, FR_DIR, motorSpeed * 0.8, true);
    setMotor(BL_PWM, BL_DIR, motorSpeed * 0.5, true);
    setMotor(BR_PWM, BR_DIR, motorSpeed * 0.8, true);
  } else {
    setMotor(FL_PWM, FL_DIR, motorSpeed * 0.8, true);
    setMotor(FR_PWM, FR_DIR, motorSpeed * 0.5, true);
    setMotor(BL_PWM, BL_DIR, motorSpeed * 0.8, true);
    setMotor(BR_PWM, BR_DIR, motorSpeed * 0.5, true);
  }
}

void startManoeuvre1() {
  manoeuvreActive = true;
  lineFollowingMode = false;
  kidnappedMode = false;
  currentManoeuvre = 1;
  moveBackwardSlow();
}

const unsigned long M5_BRIEF_STOP_MS   = 120;
const unsigned long M5_FORWARD_ONLY_MS = 1000; // at speed 90

// M5 - parking
void manoeuvre5_Parking() {
  static bool m5Triggered = false;

  if (!m5Triggered) {
    moveForward();

    if (irLeftDigital == HIGH && irRightDigital == HIGH) {
      m5Triggered = true;

      stopMotorsOnly();
      delay(M5_BRIEF_STOP_MS);

      moveForward();               
      delay(M5_FORWARD_ONLY_MS);

      stopMotorsOnly();

      manoeuvreActive = false;
      currentManoeuvre = 0;
      m5Triggered = false;
    }
  }
}

void startManoeuvre5() {
  manoeuvreActive = true;
  lineFollowingMode = false;
  kidnappedMode = false;
  currentManoeuvre = 5;
  moveForward();
}


// M6 - follow line until obstacle detected
void manoeuvre6_EmergencyStop() {
  static int m6LostCounter = 0;
  
  float dist = readUltrasonicDistance();
  if (dist > 0 && dist < OBSTACLE_DISTANCE) {
    stopMotorsOnly();
    manoeuvreActive = false;
    currentManoeuvre = 0;
    m6LostCounter = 0;
    return;
  }
  
  if (irLeftDigital == LOW && irRightDigital == LOW) {
    m6LostCounter = 0;
    moveForward();
    return;
  }
  
  if (irLeftDigital == HIGH && irRightDigital == HIGH) {
    m6LostCounter++;
    if (m6LostCounter > END_THRESHOLD) {
      stopMotorsOnly();
      manoeuvreActive = false;
      currentManoeuvre = 0;
      m6LostCounter = 0;
      return;
    }
    moveForward();
    return;
  }
  
  m6LostCounter = 0;
  
  if (irLeftDigital == HIGH && irRightDigital == LOW) {
    steerLeft();
    return;
  }
  
  if (irLeftDigital == LOW && irRightDigital == HIGH) {
    steerRight();
    return;
  }
}

void startManoeuvre6() {
  manoeuvreActive = true;
  lineFollowingMode = false;
  kidnappedMode = false;
  currentManoeuvre = 6;
  obstacleDetectionEnabled = true;
  moveForward();
}

// --- Setup ---
void setup() {
  // ultrasonic setup
  pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONIC_ECHO, INPUT);
  
  Serial.begin(115200);
  
  // wifi setup
  WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST, WINC_EN);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WINC1500 not detected"); 
    while (1) {}
  }

  Serial.print("FW: "); 
  Serial.println(WiFi.firmwareVersion());

  Serial.println("Starting AP...");
  int s = WiFi.beginAP(ssid, pass, 6);
  if (s != WL_AP_LISTENING) {
    Serial.print("WPA2 AP failed ("); 
    Serial.print(s); 
    Serial.println("). Trying OPEN...");
    s = WiFi.beginAP(ssid, 6);
    if (s != WL_AP_LISTENING) { 
      Serial.println("AP failed"); 
      while (1) {} 
    }
  }

  delay(8000);

  Serial.println("WiFi ready");
  Serial.print("SSID: "); 
  Serial.println(ssid);
  Serial.print("IP: "); 
  Serial.println(ipToString(WiFi.localIP()));
  
  server.begin();

  // sensors
  pinMode(PIN_IR_LEFT_DIGITAL, INPUT);
  pinMode(PIN_IR_RIGHT_DIGITAL, INPUT);
  pinMode(PIN_IR_LEFT_ANALOG, INPUT);
  pinMode(PIN_IR_RIGHT_ANALOG, INPUT);

  // motors
  setupMotor(FL_PWM, FL_DIR);
  setupMotor(FR_PWM, FR_DIR);
  setupMotor(BL_PWM, BL_DIR);
  setupMotor(BR_PWM, BR_DIR);

  stopAllMotors();
}

// handles incoming http requests
void serve(WiFiClient& client) {
  client.setTimeout(1500);
  String rl = client.readStringUntil('\n');
  int sp1 = rl.indexOf(' '), sp2 = rl.indexOf(' ', sp1 + 1);
  String uri = (sp1 > 0 && sp2 > sp1) ? rl.substring(sp1 + 1, sp2) : "/";
  int q = uri.indexOf('?'); 
  String pth = (q >= 0) ? uri.substring(0, q) : uri; 
  String qry = (q >= 0) ? uri.substring(q + 1) : "";
  
  while (true) { 
    String h = client.readStringUntil('\n'); 
    if (h.length() == 0 || h == "\r") break; 
  }
  
  route(client, pth, qry);
}

// url routing
void route(WiFiClient& c, const String& path, const String& q) {

  if (path == "/f" || path == "/forward") { 
    handleForward(c); 
    return; 
  }
  if (path == "/b" || path == "/backward") { 
    handleBackward(c); 
    return; 
  }
  if (path == "/l" || path == "/left") { 
    handleLeft(c); 
    return; 
  }
  if (path == "/r" || path == "/right") { 
    handleRight(c); 
    return; 
  }
  if (path == "/e" || path == "/cw") { 
    handleRotateCW(c); 
    return; 
  }
  if (path == "/q" || path == "/ccw") { 
    handleRotateCCW(c); 
    return; 
  }
  if (path == "/x" || path == "/stop") { 
    handleStop(c); 
    return; 
  }
  if (path == "/lf" || path == "/line"){
    handleLineFollowing(c);
    return;
  }
  if (path == "/kidnapped") {
    handleKidnapped(c);
    return;
  }
  if (path == "/m1") {
    handleManoeuvre1(c);
    return;
  }
  if (path == "/m5") {
    handleManoeuvre5(c);
    return;
  }
  if (path == "/m6") {
    handleManoeuvre6(c);
    return;
  }
  
  // speed
  if (path.startsWith("/speed")) {
    int newSpeed = path.substring(6).toInt();
    if (newSpeed >= 0 && newSpeed <= 255) {
      motorSpeed = newSpeed;
      motorSpeedStrafe = motorSpeed + 40;
      sendText(c, "Speed: " + String(motorSpeed));
    } else {
      sendText(c, "bad speed");
    }
    return;
  }
  // ir sensor data
  if (path == "/infrared") {
    String out = String(irLeftAnalog) + "," + String(irRightAnalog);
    sendText(c, out);
    return;
  }
  sendText(c, "404");
}

void handleForward(WiFiClient& client) {
  moveForward();
  if (!checkObstacle()) {  
    sendText(client, "Moving Forward");
  } else {
    sendText(client, "Blocked by obstacle");
  }
}

void handleBackward(WiFiClient& client) {
  moveBackward();
  sendText(client, "Moving Backward");
}

void handleLeft(WiFiClient& client) {
  strafeLeft();
  sendText(client, "Moving Left");
}

void handleRight(WiFiClient& client) {
  strafeRight();
  sendText(client, "Moving Right");
}

void handleRotateCW(WiFiClient& client) {
  rotateCW();
  sendText(client, "Rotating Clockwise");
}

void handleRotateCCW(WiFiClient& client) {
  rotateCCW();
  sendText(client, "Rotating Counter-Clockwise");
}

void handleStop(WiFiClient& client) {
  stopAllMotors();
  sendText(client, "Stopped");
}

void handleLineFollowing(WiFiClient& client){
  lineFollowing();
  sendText(client, "Following line.");
}

void handleKidnapped(WiFiClient& client) {
  startKidnappedMode();
  sendText(client, "Kidnapped ON");
}

void handleManoeuvre1(WiFiClient& client) {
  startManoeuvre1();
  sendText(client, "M1");
}

void handleManoeuvre5(WiFiClient& client) {
  startManoeuvre5();
  sendText(client, "M5");
}

void handleManoeuvre6(WiFiClient& client) {
  startManoeuvre6();
  sendText(client, "M6");
}

void sendText(WiFiClient& client, const String& text) {
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/plain\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: "); 
  client.print(text.length()); 
  client.print("\r\n\r\n");
  client.print(text);
  delay(1);
}

// helps with 90 degree turns
void ninetyDegreeTurnAssist(){
  if(lastTriggeredSensor == LEFT){
    if (irRightDigital == HIGH) {
      lastTriggeredSensor = NONE;
      rotateCW();
      delay(readjustTimeMS);
      moveForward();
    }
    return;
  }
  if(lastTriggeredSensor == RIGHT){
    if (irLeftDigital == HIGH) {
      lastTriggeredSensor = NONE;
      rotateCCW();
      delay(readjustTimeMS);
      moveForward();
    }
    return;
  }
}

// main line following logic
void lineFollowingTurningControl(){
  if(lastTriggeredSensor != NONE){
    ninetyDegreeTurnAssist();
    return;
  }

  // both on line = go straight
  if (irLeftDigital == LOW && irRightDigital == LOW){
    if (straightTravelTime == 0) {
      straightTravelTime = millis();
    } else if (millis() - straightTravelTime > RESET_TRAVEL_TIME) {
      // reset loop detection
      consecutiveLeftTurns = 0;
      consecutiveRightTurns = 0;
      currentTurnStrategy = 0;
    }
    moveForward();
    return;
  }

  straightTravelTime = 0;

  // intersection
  if (irLeftDigital == HIGH && irRightDigital == HIGH){
    handleIntersection();
    return;
  }

  // left sensor triggered
  if (irLeftDigital == HIGH && irRightDigital == LOW){
    if (irLeftAnalog > 110){ // big turn needed
      lastTriggeredSensor = LEFT;
      setMotor(FL_PWM, FL_DIR, motorSpeed * 1.2, true);
      setMotor(FR_PWM, FR_DIR, motorSpeed * 1.2, false);
      setMotor(BL_PWM, BL_DIR, motorSpeed * 1.2, true);
      setMotor(BR_PWM, BR_DIR, motorSpeed * 1.2, false);
      return;
    }
    // small correction
    setMotor(FL_PWM, FL_DIR, motorSpeed, false);
    setMotor(FR_PWM, FR_DIR, motorSpeed * 1.4, false);
    setMotor(BL_PWM, BL_DIR, motorSpeed, false);
    setMotor(BR_PWM, BR_DIR, motorSpeed * 1.4, false);
    return;
  }

  // right sensor triggered
  if (irLeftDigital == LOW && irRightDigital == HIGH){
    if (irRightAnalog > 110){ // big turn needed
      lastTriggeredSensor = RIGHT;
      setMotor(FL_PWM, FL_DIR, motorSpeed* 1.2, false);
      setMotor(FR_PWM, FR_DIR, motorSpeed* 1.2, true);
      setMotor(BL_PWM, BL_DIR, motorSpeed* 1.2, false);
      setMotor(BR_PWM, BR_DIR, motorSpeed* 1.2, true);
      return;
    }
    // small correction
    setMotor(FL_PWM, FL_DIR, motorSpeed * 1.4, false);
    setMotor(FR_PWM, FR_DIR, motorSpeed, false);
    setMotor(BL_PWM, BL_DIR, motorSpeed * 1.4, false);
    setMotor(BR_PWM, BR_DIR, motorSpeed, false);
    return;
  }
}

void handleIntersection() {
  unsigned long now = millis();
  
  bool quickIntersection = (now - lastIntersectionTime < MIN_INTERSECTION_INTERVAL);
  lastIntersectionTime = now;
  
  // looping? try different direction
  if (quickIntersection) {
    currentTurnStrategy = (currentTurnStrategy + 1) % 3;
  }
  
  stopMotorsOnly();
  delay(100);
  moveForwardSlow();
  delay(250);
  stopMotorsOnly();
  delay(100);
  
  int turnDirection = currentTurnStrategy;
  
  if (turnDirection == 0) {
    // turn left
    consecutiveLeftTurns++;
    consecutiveRightTurns = 0;
    
    if (consecutiveLeftTurns >= LOOP_THRESHOLD) {
      currentTurnStrategy = 1;
      consecutiveLeftTurns = 0;
    }
    
    // blind turn ~30 degrees to avoid detecting straight path
    rotateCCW();
    delay(133);
    stopMotorsOnly();
    delay(50);
    spinUntilLineConfirmed(true);
    
  } else if (turnDirection == 1) {
    // go straight
    consecutiveLeftTurns = 0;
    consecutiveRightTurns = 0;
    
    moveForward();
    delay(200);
    
    irLeftDigital = digitalRead(PIN_IR_LEFT_DIGITAL);
    irRightDigital = digitalRead(PIN_IR_RIGHT_DIGITAL);
    if (irLeftDigital == LOW && irRightDigital == LOW) {
      // no line ahead, turn right instead
      currentTurnStrategy = 2;
      rotateCW();
      delay(133);
      stopMotorsOnly();
      delay(50);
      spinUntilLineConfirmed(false);
    }
    
  } else {
    // turn right
    consecutiveRightTurns++;
    consecutiveLeftTurns = 0;
    
    if (consecutiveRightTurns >= LOOP_THRESHOLD) {
      currentTurnStrategy = 0;
      consecutiveRightTurns = 0;
    }
    
    rotateCW();
    delay(133);
    stopMotorsOnly();
    delay(50);
    spinUntilLineConfirmed(false);
  }
  
  lastTriggeredSensor = NONE;
}

// get distance from ultrasonic sensor
float readUltrasonicDistance() {
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  
  digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
  
  ultrasonicDuration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, 30000);
  ultrasonicDistance = (ultrasonicDuration * 0.034) / 2;
  
  return ultrasonicDistance;
}

bool checkObstacle() {
  if (!obstacleDetectionEnabled) return false;
  if (emergencyStopActive) return true;
  
  float dist = readUltrasonicDistance();
  
  if (dist > 0 && dist < OBSTACLE_DISTANCE) {
    stopMotorsOnly();
    delay(200);
    
    if (lineFollowingMode) {
      spinUntilLineFound(false);
      return false;
    }
    
    emergencyStopActive = true; 
    return true;
  }
  return false;
}

void loop() {
  // read sensors
  irLeftDigital = digitalRead(PIN_IR_LEFT_DIGITAL);
  irRightDigital = digitalRead(PIN_IR_RIGHT_DIGITAL);  
  irLeftAnalog = analogRead(PIN_IR_LEFT_ANALOG);
  irRightAnalog = analogRead(PIN_IR_RIGHT_ANALOG);

  // check for obstacles (except m6 handles this itself)
  if (currentManoeuvre != 6 && checkObstacle()){
    stopAllMotors();
    emergencyStopActive = true;
    delay(1000);
    emergencyStopActive = false;
    return;
  }

  // run whatever mode is active
  if (manoeuvreActive) {
    switch (currentManoeuvre) {
      case 1: manoeuvre1_ReverseStraight(); break;
      case 5: manoeuvre5_Parking(); break;
      case 6: manoeuvre6_EmergencyStop(); break;
    }
  } else if (kidnappedMode) {
    kidnappedControl();
  } else if (lineFollowingMode) {
    lineFollowingTurningControl();
  }

  // handle web requests
  WiFiClient client = server.available();
  if (!client) return;

  client.setTimeout(2000);
  serve(client);
  client.stop();
}