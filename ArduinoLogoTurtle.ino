
#include <SyncedMotors.h>
#include <Motor.h>
#include <math.h>

#define MOTOR_STEPS 200
#define MOTOR_MICROSTEPS 4

#define DEFAULT_MOTOR_RPM 300  // == cca 4.8 km/h
#define DEFAULT_LINEAR_ACCEL 200      // RPM per second
#define DEFAULT_ROTARY_ACCEL 300      // RPM per second

#define WHEEL_CIRCUMFERENCE_MM 415
#define WHEEL_DISTANCE_MM 434

#define GPIO_LEFT_DIR 3
#define GPIO_LEFT_STEP 4
#define GPIO_LEFT_ENABLE 6
#define GPIO_RIGHT_DIR 9
#define GPIO_RIGHT_STEP 8
#define GPIO_RIGHT_ENABLE 7

#define PI 3.141593
#define degreesToRadians(deg) ((float)(deg) * PI / 180.0)
#define radiansToDegrees(rad) ((float)(rad) * 180.0 / PI)

#define toStepsPerSecondSquared(rpmPerSecond) ((long)MOTOR_STEPS * (long)MOTOR_MICROSTEPS * (long)rpmPerSecond / 60L)

int linearAccelRPMperSecond = DEFAULT_LINEAR_ACCEL;
int rotaryAccelRPMperSecond = DEFAULT_ROTARY_ACCEL;
int wheelCircumferenceMm = WHEEL_CIRCUMFERENCE_MM;
int wheelDistanceMm = WHEEL_DISTANCE_MM;

#define MAX_ARGS 5
String command = "";
String args[MAX_ARGS];


struct Function {
  String name;
  byte argCount;
  String argNames[MAX_ARGS];
  String body;
};

#define MAX_FUNCTIONS 5
Function functions[MAX_FUNCTIONS];
Function* func;  // the function we're currently declaring

#define READING_COMMAND 0
#define READING_ARGS 1
#define READING_PARENTHESIZED_ARG 3
#define READING_FUNCTION_DEF 4
byte state = READING_COMMAND;
byte argCount = 0;

// current position
float x = 0.0;
float y = 0.0;
int orientation = 90;

Motor lMotor(MOTOR_STEPS * MOTOR_MICROSTEPS, GPIO_LEFT_DIR, GPIO_LEFT_STEP, GPIO_LEFT_ENABLE);
Motor rMotor(MOTOR_STEPS * MOTOR_MICROSTEPS, GPIO_RIGHT_DIR, GPIO_RIGHT_STEP, GPIO_RIGHT_ENABLE);

SyncedMotors motors(lMotor, rMotor);


void setup() {
  Serial.begin(57600);

  initDrivers();

  Serial.println(F("Logo turtle ready for action!"));
  Serial.println();
  
  printConfig();
  Serial.println();
  printHelp();
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("- \"FORWARD 100\" moves turtle forward 100 cm"));
  Serial.println(F("- \"BACKWARD 50\" moves turtle bacward 50 cm"));
  Serial.println(F("- \"LEFT 90\" moves turtle left 90 degrees"));
  Serial.println(F("- \"RIGHT 45\" moves turtle right 45 degrees"));
  Serial.println(F("- \"PEN UP\" raises the turtle's pen"));
  Serial.println(F("- \"PEN DOWN\" lowers the turtle's pen"));
  Serial.println(F("- \"REPEAT 4 (FORWARD 10; LEFT 90)\" draws a square"));
  Serial.println(F("- \"FORWARD 2*40+20\" moves forward 100 cm"));
  Serial.println();
}


void initDrivers() {
  motors.setRPM(DEFAULT_MOTOR_RPM);
  motors.setAcceleration(toStepsPerSecondSquared(linearAccelRPMperSecond));
  motors.disable();
}

void printConfig() {
  Serial.print(F("Config: RPM: "));
  Serial.print(motors.getRPM());

  Serial.print(F("; linear accel: "));
  Serial.print(linearAccelRPMperSecond);
  Serial.print(F(" rpm/s"));

  Serial.print(F("; rotary accel: "));
  Serial.print(rotaryAccelRPMperSecond);
  Serial.print(F(" rpm/s"));

  Serial.print(F("; wheel circumference: "));
  Serial.print(wheelCircumferenceMm);
  Serial.print(F(" mm"));

  Serial.print(F("; wheel distance: "));
  Serial.print(wheelDistanceMm);
  Serial.println(F(" mm"));

  Serial.print(F("Free memory: "));
  Serial.print(freeMemory());
  Serial.println(F(" bytes"));
}



void loop() {
  if (Serial.available() == 0) {
    return;
  }

  int ch = Serial.read();
  processChar(ch);
}

void processChar(int ch) {
  if (ch == -1 || ch == '\r') {
    return;
  }

  if (state == READING_FUNCTION_DEF) {
    func->body += char(ch);
    if (func->body.length() > 5) {
      int idx = func->body.length()-5;
      String end = func->body.substring(idx);
      end.toUpperCase();
      if (end == "\nEND\n") {
        func->body.remove(idx);
        resetParser();
        Serial.print(F("New function "));
        Serial.print(func->name);
        Serial.println(F(" defined"));
      }
    }
    
  } else if (state == READING_PARENTHESIZED_ARG) {
    if (ch == '\n') {
      Serial.println(F("ERROR: missing closing parenhesis"));
      resetParser();
    } else if (ch == ')') {
      state = READING_ARGS;
    } else {
      args[argCount-1] += char(ch);
    }
  } else {
    if (ch == ' ') {
      if (state == READING_COMMAND && command == "" || state == READING_ARGS && args[argCount-1] == "") {  // ignore multiple spaces & space before the command (e.g. after a semicolon)
        return;
      }
      state = READING_ARGS;
      argCount++;
    } else if (ch == '\n' || ch == ';') {
      if (command == "") {  // ignore empty lines
        return;
      }
      if (argCount > 0 && args[argCount-1] == "") { // ignore spaces at the end
        argCount--;
      }

      executeCommand();
      if (command == "TO") {
        resetParser();
        state = READING_FUNCTION_DEF;
      } else {
        resetParser();
      }
    } else if (ch == '(' && state == READING_ARGS) {
      state = READING_PARENTHESIZED_ARG;
    } else {
      if (state == READING_COMMAND) {
        command += char(toupper(ch));
      } else {
        args[argCount-1] += char(ch);
      }
    }
  }
}

void resetParser() {
  command = "";
  argCount = 0;
  for (byte i = 0; i < MAX_ARGS; i++) {
    args[i] = "";
  }
  state = READING_COMMAND;
}

void executeCommandFromString(String s) {
  resetParser();
  int len = s.length();
  for (int i=0; i<len; i++) {
    char ch = s[i];
    processChar(ch);
  }
  processChar('\n');  // required to trigger the command, since s does not include a final '\n' or ';'
}


void executeCommand() {
  if (command == "HELP" || command == "POMOČ") {
    printHelp();
  } else if (command == "RESET") {
    resetCmd();
  } else if (command == "SET") {
    setConfigCmd();
  } else if (command == "GET" || command == "CONFIG") {
    printConfig();
  } else if (command == "ROTATE" || command == "ROT") {
    rotateCmd();

  } else if (command == "FORWARD" || command == "FWD" || command == "F" || command == "NAPREJ") {
    forwardCmd();
  } else if (command == "BACKWARD" || command == "BACK" || command == "B" || command == "NAZAJ") {
    backwardCmd();
  } else if (command == "LEFT" || command == "L" || command == "LEVO") {
    leftCmd();
  } else if (command == "RIGHT" || command == "R" || command == "DESNO") {
    rightCmd();
  } else if (command == "PEN" || command == "P") {
    penCmd();
  } else if (command == "HOME" || command == "DOMOV") {
    homeCmd();

  } else if (command == "SQUARE" || command == "KVADRAT") {
    squareCmd();
  } else if (command == "TRIANGLE" || command == "TRIKOTNIK") {
    triangleCmd();

  } else if (command == "REPEAT" || command == "PONOVI") {
    repeatCmd();
  } else if (command == "TO" || command == "ZA") {
    defineCmd();


  } else {
    func = getFunction(command, false);
    if (func == NULL) {
      Serial.print(F("Unknown command: "));
      Serial.println(command);
    } else {
      executeFunction(func);
    }
  }

  //printPosition();
}

void executeFunction(Function* f) {
  if (argCount != f->argCount) {
    Serial.print(F("Usage:   "));
    Serial.print(f->name);
    for (byte i=0; i < f->argCount; i++) {
      Serial.print(F(" <"));
      Serial.print(f->argNames[i]);
      Serial.print(F(">"));
    }
    Serial.println();
    return;
  }

  String body = f->body;
  for (byte i=0; i < f->argCount; i++) {
    body.replace(f->argNames[i], args[i]);
  }
  
  executeCommandFromString(body);
}

void resetCmd() {
  x = 0.0;
  y = 0.0;
  orientation = 90;
  Serial.println(F("Position reset to (0,0)."));
}

void homeCmd() {
  Serial.print(F("Returning home..."));
  home();
  Serial.println(F("done!"));
}

void forwardCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage:   FORWARD <cm>"));
    Serial.println(F("Example: FORWARD 100"));
    return;
  }
  int cm = parseNum(args[0]);
  Serial.print(F("Driving forward "));
  Serial.print(cm);
  Serial.print(F(" cm..."));
  forward(cm);
  Serial.println(F("done!"));
}

void backwardCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage:   BACKWARD <cm>"));
    Serial.println(F("Example: BACKWARD 100"));
    return;
  }
  int cm = parseNum(args[0]);
  Serial.print(F("Driving backward "));
  Serial.print(cm);
  Serial.print(F(" cm..."));
  backward(cm);
  Serial.println(F("done!"));
}

void leftCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage:   LEFT <degrees>"));
    Serial.println(F("Example: LEFT 90"));
    return;
  }
  int deg = parseNum(args[0]);
  Serial.print(F("Turning left "));
  Serial.print(deg);
  Serial.print(F(" degrees..."));
  left(deg);
  Serial.println(F("done!"));
}

void rightCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage:   RIGHT <degrees>"));
    Serial.println(F("Example: RIGHT 90"));
    return;
  }
  int deg = parseNum(args[0]);
  Serial.print(F("Turning right "));
  Serial.print(deg);
  Serial.print(F(" degrees..."));
  right(deg);
  Serial.println(F("done!"));
}

void penCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage:   PEN <UP/DOWN>"));
    Serial.println(F("Example: PEN UP"));
    return;
  }
  String dir = args[0];
  dir.toUpperCase();
  if (dir == "UP") {
    Serial.print(F("Raising pen..."));
    penUp();
    Serial.println(F("done!"));
  } else if (dir == "DOWN") {
    Serial.print(F("Lowering pen..."));
    penDown();
    Serial.println(F("done!"));
  } else {
    Serial.println(F("The pen can either go UP or DOWN."));
  }
}

void squareCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage: SQUARE <cm>"));
    return;
  }
  int size = parseNum(args[0]);
  Serial.print(F("Tracing a square of size "));
  Serial.print(size);
  Serial.print(F(" cm..."));
  for (byte i=0; i<4; i++) {
    forward(size);
    left(90);
  }
  Serial.println(F("done!"));
}

void triangleCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage: TRIANGLE <cm>"));
    return;
  }
  int size = parseNum(args[0]);
  Serial.print(F("Tracing a triangle of size "));
  Serial.print(size);
  Serial.print(F(" cm..."));
  for (byte i=0; i<3; i++) {
    forward(size);
    left(120);
  }
  Serial.println(F("done!"));
}

void rotateCmd() {
  if (argCount < 2) {
    Serial.println(F("Usage: ROTATE <left/right/both> <degrees>"));
    Serial.println(F("Usage: ROTATE <left degrees> <right degrees>"));
    return;
  }
  String motor = args[0];
  motor.toUpperCase();
  int deg = parseNum(args[1]);
  if (motor == "LEFT" || motor == "L") {
    Serial.print(F("Rotating LEFT motor "));
    Serial.print(deg);
    Serial.print(F(" degrees..."));
    rotate(deg, 0);
  } else if (motor == "RIGHT" || motor == "R") {
    Serial.print(F("Rotating RIGHT motor "));
    Serial.print(deg);
    Serial.print(F(" degrees..."));
    rotate(0, deg);
  } else if (motor == "BOTH") {
    Serial.print(F("Rotating BOTH motors "));
    Serial.print(deg);
    Serial.print(F(" degrees..."));
    rotate(deg, deg);
  } else {
    int deg1 = parseNum(args[0]);
    int deg2 = parseNum(args[1]);
    Serial.print(F("Rotating motors "));
    Serial.print(deg1);
    Serial.print(F(" and "));
    Serial.print(deg2);
    Serial.print(F(" degrees..."));
    rotate(deg1, deg2);
  }
  Serial.println(F("done!"));
}

void setConfigCmd() {
  if (argCount == 0) {
    printConfig();
    return;
  }
  if (argCount != 2) {
    Serial.println(F("Usage:   SET <key> <value>"));
    Serial.println(F("Example: SET RPM 100"));
    return;
  }
  String key = args[0];
  String value = args[1];

  setConfig(key, value);
}

void repeatCmd() {
  if (argCount != 2) {
    Serial.println(F("Usage:   REPEAT <num> (<commands separated by ;>)"));
    Serial.println(F("Example: REPEAT 4 (FORWARD 10; LEFT 90)"));
    return;
  }
  int count = parseNum(args[0]);
  String cmd = args[1]; // do not inline, because executeCommandFromString overrides it!
  for (int i=0; i<count; i++) {
    executeCommandFromString(cmd);
  }
}

void defineCmd() {
  if (argCount < 1) {
    Serial.println(F("Usage:   TO <new command name> $<paramName1> $<paramName2> ..."));
    Serial.println(F("         <command 1>"));
    Serial.println(F("         <command 2>"));
    Serial.println(F("         ..."));
    Serial.println(F("         <command N>"));
    Serial.println(F("         END"));
    Serial.println();
    Serial.println(F("Example: TO triangle $size"));
    Serial.println(F("         FORWARD $size"));
    Serial.println(F("         LEFT 120"));
    Serial.println(F("         FORWARD $size"));
    Serial.println(F("         LEFT 120"));
    Serial.println(F("         FORWARD $size"));
    Serial.println(F("         LEFT 120"));
    Serial.println(F("         END"));
    return;
  }

  args[0].toUpperCase();
  func = getFunction(args[0], true);
  func->argCount = argCount-1;  // paramNames start at arg[1] (arg[0] holds the command name)
  for (byte i=1; i<argCount; i++) {
    func->argNames[i-1] = args[i];
  }
  Serial.println(F("Type your commands then end the function by typing END"));
}


Function* getFunction(String name, bool createNewIfNotFound) {
  for (byte i=0; i<MAX_FUNCTIONS; i++) {
    Function* f = &functions[i];
    if (f->name == name) {
      return f;
    } else if (f->name == "") {
      if (createNewIfNotFound) {
        f->name = name;
        f->body = "";
        return f;
      } else {
        return NULL;
      }
    }
  }
  if (createNewIfNotFound) {
    Serial.println(F("ERROR: no space to create new function; overwriting last function!"));
  }
  return NULL;
}





void rotate(int deg1, int deg2) {
  motors.enable();
  motors.rotate(deg1, deg2);
  motors.disable();
}

void setConfig(String key, String value) {
  key.toUpperCase();
  if (key == "RPM") {
    motors.setRPM(parseNum(value));
    Serial.print(F("RPM set to "));
    Serial.println(value);
    printConfig();
  } else if (key == "ACCEL" || key == "LINEAR_ACCEL") {
    linearAccelRPMperSecond = parseNum(value);
    motors.setAcceleration(toStepsPerSecondSquared(linearAccelRPMperSecond));
    Serial.print(F("Linear accel set to "));
    Serial.println(linearAccelRPMperSecond);
    printConfig();
  } else if (key == "ROTARY_ACCEL") {
    rotaryAccelRPMperSecond = parseNum(value);
    Serial.print(F("Rotary accel set to "));
    Serial.println(rotaryAccelRPMperSecond);
    printConfig();
  } else if (key == "WHEEL_CIRCUMFERENCE_MM") {
    wheelCircumferenceMm = parseNum(value);
    Serial.print(F("Wheel circumference set to "));
    Serial.print(wheelCircumferenceMm);
    Serial.println(F(" mm"));
    printConfig();
  } else if (key == "WHEEL_DISTANCE_MM") {
    wheelDistanceMm = parseNum(value);
    Serial.print(F("Wheel distance set to "));
    Serial.print(wheelDistanceMm);
    Serial.println(F(" mm"));
    printConfig();
  } else {
    Serial.print(F("Unknown setting: "));
    Serial.println(key);
  }
}

void home() {
  float a = radiansToDegrees(atan2(y, x));
  int homeDirection = (int(a) + 180) % 360;

  // turn towards home
  leftOptimized(homeDirection - orientation);

  // drive to home
  forward(round(sqrt(x*x + y*y)));

  // turn back to 90°
  leftOptimized(90 - orientation);
}

// optimized turn to the left (turns right if the turn is bigger than 180°)
void leftOptimized(int deg) {
  if (deg < 0) {
    deg += 360;
  }
  if (deg < 180) {
    left(deg);
  } else {
    right(360 - deg);
  }
}

void forward(int cm) {
  motors.enable();
  forward0(cm);
  motors.disable();
}

void backward(int cm) {
  forward(-cm);
}

void left(int deg) {
  right(-deg);
}

void right(int deg) {
  motors.enable();
  right0(deg);
  motors.disable();
}

void left0(int deg) {
  right0(-deg);
}

void backward0(int cm) {
  forward0(-cm);
}



void forward0(int cm) {
  int deg = (int)360 * cm * 10 / wheelCircumferenceMm;
  motors.setAcceleration(toStepsPerSecondSquared(linearAccelRPMperSecond));
  motors.rotate(deg, deg);
  float rad = (float)degreesToRadians(orientation);
  x += (float)cm * cos(rad);
  y += (float)cm * sin(rad);
}

void right0(int deg) {
  //long cm = (long)((float)PI*wheelDistanceMm * deg / 360.0 / 10.0);
  //long wheelDeg = (long)360 * cm * 10 / wheelCircumferenceMm;
  long wheelDeg = (long)((float)PI * wheelDistanceMm * deg) / wheelCircumferenceMm;
  motors.setAcceleration(toStepsPerSecondSquared(rotaryAccelRPMperSecond));
  motors.rotate(wheelDeg, -wheelDeg);
  motors.setAcceleration(toStepsPerSecondSquared(linearAccelRPMperSecond));
  orientation = (orientation - deg) % 360;
  if (orientation <= -180) {
    orientation = orientation + 360;
  }
}

void penUp() {
}

void penDown() {
}


void printPosition() {
  Serial.print(F("Current position: "));
  Serial.print(x);
  Serial.print(F("cm, "));
  Serial.print(y);
  Serial.print(F("cm, "));
  Serial.print(orientation);
  Serial.println(F("°"));
}

long parseNum(String s) {
  float num = 0.0;
  String token = "";
  char op = '+';
  byte len = s.length();
  for (byte i=0; i<len; i++) {
    char ch = s[i];
    if (ch == ' ') {
      // ignore
    } else if (isDigit(ch)) {
      token += ch;
    } else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
      num = performCalcOp(num, op, token.toInt());
      op = ch;
      token = "";
    }
  }
  num = performCalcOp(num, op, token.toInt());
  return (long)num;
}

float performCalcOp(float num1, char op, float num2) {
  switch (op) {
    case '+': return num1 + num2;
    case '-': return num1 - num2;
    case '*': return num1 * num2;
    case '/': return num1 / num2;
    default: 
      Serial.print(F("ERROR: unknown operation: "));
      Serial.println(op); 
      return 0;
  }
}


int freeMemory() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

