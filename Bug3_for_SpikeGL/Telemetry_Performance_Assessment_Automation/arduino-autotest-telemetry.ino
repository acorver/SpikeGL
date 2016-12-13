
/*
  This script controls a bead on a line, and is updated to interface with the Arena Automation system.

  Last updated by Abel Corver, December 2016
  Derived from code by Matteo Mischiati & Huai-Ti Lin
*/

// ================================================================
// Includes
// ================================================================

#include <EEPROM.h>
#include <Encoder.h>
#include <Kangaroo.h>

#include <ArduinoJson.h>

// ================================================================
// Helper functions
// ================================================================

// -- command parsing function
uint8_t parse(char *line, char **argv, uint8_t maxArgs, char sep = ' ') {

  uint8_t argCount = 0;

  while (*line != '\0') {
    /* if not the end of line ....... */
    while (*line == sep || *line == '\n')
      *line++ = '\0';     /* replace commas and white spaces with 0    */
    *argv++ = line;         /* save the argument position     */
    argCount++;
    if (argCount == maxArgs - 1)
      break;
    while (*line != '\0' && *line != sep && *line != '\n')
      line++;             /* skip the argument until ...    */
  }

  *argv = '\0';             /* mark the end of argument list  */
  return argCount;
}

// ================================================================
//
// ================================================================

#define PC_SERIAL_BUF_LENGTH    512
#define PC_SERIAL_MAX_ARGS      16

// Read buffer for commands from PC
char    g_PCSerialBuffer[PC_SERIAL_BUF_LENGTH];
uint8_t g_PCSerialBufferIdx;
char*   g_PCSerialArgs[PC_SERIAL_MAX_ARGS];

// ----------------------
// Timing variables
// ----------------------

unsigned long delTime     = 0;
unsigned long Interval    = 20000;        // 20ms
unsigned long LastTime    = 0;            // last time stamp
unsigned long CurrentTime = 0;            // current time stamp
unsigned long StartTime   = CurrentTime;  // mark the t0 point in microsecond

// ----------------------
// Kangaroo variables
// ----------------------

#define TUNE_MODE_NUMBER    3                   // MechStop is default for the FlySim X control

Encoder Enc2(2, 3); // FlySim desktop model objects

#define MAX_X_ERROR         32                  // Maximum error in Kangaroo positioning that still counts as "goal achieved"
#define MAX_Z_ERROR         32                  // Maximum error in Kangaroo positioning that still counts as "goal achieved"

long WHEEL_SIZE_X  =  40;
float CONVERSION_X  =  WHEEL_SIZE_X * PI / 256;

long WHEEL_SIZE_Z  =  34;
float CONVERSION_Z  =  WHEEL_SIZE_Z * PI / 35;

KangarooSerial  *kangarooSerialX  = 0;   // This is the X-axis Kangaroo
KangarooChannel *kangarooX        = 0;   // This is the FlySim X

KangarooSerial  *kangarooSerialZ  = 0;   // This is the Z-axis Kangaroo
KangarooChannel *kangarooZ1       = 0;   // This is the FlySim pod 1
KangarooChannel *kangarooZ2       = 0;   // This is the FlySim pod 2

int g_AbortKangaroo = 0;

#define KANGAROO_COMMAND_TIMEOUT_MS   100

// ----------------------
//  Pins...
// ----------------------

#define PIN_DFPRESENT    8
#define PIN_DFMOVE       9
#define PIN_MACTRIGGER  10
#define PIN_SYNC        11

#define PIN_STOP         7

// ----------------------
//  Misc parameters
// ----------------------

// Maximum length in bytes/characters of JSON objets (e.g. status output)
#define JSON_MAX_LENGTH 2048

// Is cortex recording?
bool g_bCortexRecording = false;

// ----------------------
// Variables indicating what function is running
// ----------------------

// The name of the function currently running (for status/debugging purposes)
String g_CurrentUpdateFunctionStr = "";

// The function in charge of updating the trajectory until its completion
void (*g_CurrentUpdateFunction)(long elapsedTime);

// Status of program
const char* g_Status = "";

// Function start time
long g_CurrentCommandStartTime;

// ----------------------
// sensor variables
// ----------------------

long g_CurPosX;
long g_CurPosZ1;
long g_CurPosZ2;

long g_CurVelX;
long g_CurVelZ1;
long g_CurVelZ2;

long g_MinPosX;
long g_MaxPosX;

// ----------------------
//  State variables for trajectory functions
// ----------------------

#define MVM_TYPE_WAIT    0
#define MVM_TYPE_LINEAR  1

#define MOVEMENT_SEGM_MAX_COUNT  64
#define MOVEMENT_SEGM_MAX_ARGS   5

long   g_MovementSegments[MOVEMENT_SEGM_MAX_COUNT][MOVEMENT_SEGM_MAX_ARGS];
long   g_curMovementSegment;
long   g_numMovementSegments;

#define NUM_VELOCITY_SEGMENTS 2

long g_VelocitySegments     [NUM_VELOCITY_SEGMENTS  ];
long g_VelocitySegmentBounds[NUM_VELOCITY_SEGMENTS - 1];

long currentVelocityNoiseX;
long targetVelocityNoiseTimeLeft;
long targetVelocityNoiseX;

long targetVelocityX;
long targetVelocityZ;
long targetPositionX;
long targetPositionZ;

long g_TimeUntilTrial = 0;

long g_VelocityBoostTimeLeft = 0;
long g_VelocityBoostDir = 0;
bool g_VelocityBoostEnabled = false;

// ----------------------
//  Variables for space exploration function
// ----------------------

long g_TimeUntilNextPosition = 0;

// ----------------------
//  DF state variables
// ----------------------

boolean       DF_presence         = 0;
boolean       DF_moving           = 0;
boolean       DF_ready            = 0;  //this variable indicates that the DF is ready for the next stimulus
unsigned long DF_idle_time        = 0;
unsigned long idle_time_threshold = 5 * 1000000; // if the DF is idling for 5s

// ================================================================
// Check DF status
// ================================================================

void DF_check() {

  if (DF_presence == 1 && DF_moving == 0) {
    DF_idle_time = DF_idle_time + delTime;
  } else {
    DF_idle_time = 0;
  }
  if (DF_idle_time >= idle_time_threshold) {
    DF_ready = 1;
  } else {
    DF_ready = 0;
  }
}

// ================================================================
// (Un)Trigger cortex
// ================================================================

void triggerCortex(bool trigger) {

  if (g_bCortexRecording != trigger) {

    g_bCortexRecording = trigger;

    digitalWrite(PIN_MACTRIGGER, LOW);
    delay(100);
    digitalWrite(PIN_MACTRIGGER, HIGH);
  }
}

// ================================================================
// Setup
// ================================================================

void setup() {

  Serial.begin(9600); // This is COM6 that talks to the PC

  // Initialize random seed with noise from analog pin
  randomSeed(analogRead(0));

  // Initialize the emergency stop switch
  pinMode(PIN_STOP, INPUT);

  // This aux recieves DF presence signal
  pinMode(PIN_DFPRESENT, INPUT);

  // This aux recieves DF movement signal
  pinMode(PIN_DFMOVE, INPUT);

  // This aux initiate the recording via SyncMaster: rising edge triggers MAC capture, falling edge triggers Photron end
  pinMode(PIN_MACTRIGGER, OUTPUT);

  // This aux recieves the sync signal from SyncMaster
  pinMode(PIN_SYNC, INPUT);

  // Identify
  cmdIdentify(NULL, NULL);

  // (Try to) Start motors
  cmdStartKangaroo(NULL, NULL);
}

// ================================================================
// Error reporting function
// ================================================================

void sendError(const char* str) {
  Serial.println(str);
}

// ================================================================
// Detect error with Kangaroo operation
// ================================================================

bool kangarooAvailable(char a = ' ') {

  bool kx = true, kz = true;

  if (a == ' ' || a == 'x') {
    if (kangarooSerialX == 0) {
      kx = false;
    } else {
      if (kangarooX->getP().error()) {
        kx = false;
      }
    }
  }

  if (a == ' ' || a == 'z') {
    if (kangarooSerialZ == 0) {
      kz = false;
    } else {
      if (kangarooZ1->getP().error() || kangarooZ2->getP().error()) {
        kz = false;
      }
    }
  }

  if (a == ' ') {
    return (kx && kz);
  } else if (a == 'x') {
    return kx;
  } else if (a == 'z') {
    return kz;
  } else {
    // Invalid parameters...
    return false;
  }
}

// ================================================================
// ====== COMMAND: IDENTIFY
// ================================================================

void cmdIdentify(char** pArgs, uint8_t numArgs) {
  Serial.println("FlySim Arduino Controller --- last edited by Abel Corver");
}

// ================================================================
// ====== COMMAND: STATUS
// ================================================================

void cmdStatus(char** pArgs, uint8_t numArgs) {

  Serial.print("{");

  Serial.print("\"function\":\""+g_CurrentUpdateFunctionStr+"\",");

  Serial.print("\"status\":\""+String(g_Status)+"\",");
  
  Serial.print("\"PosX\":" +String(g_CurPosX )+",");
  Serial.print("\"PosZ1\":"+String(g_CurPosZ1)+",");
  Serial.print("\"PosZ2\":"+String(g_CurPosZ2)+",");

  Serial.print("\"MinPosX\":"+String(g_MinPosX)+",");
  Serial.print("\"MaxPosX\":"+String(g_MaxPosX)+",");

  Serial.print("\"VelX\":" +String(g_CurVelX )+",");
  Serial.print("\"VelZ1\":"+String(g_CurVelZ1)+",");
  Serial.print("\"VelZ2\":"+String(g_CurVelZ2)+",");

  Serial.print("\"TargetPosX\":" +String(targetPositionX)+",");
  Serial.print("\"TargetPosZ1\":"+String(targetPositionZ)+",");
  Serial.print("\"TargetPosZ2\":"+String(targetPositionZ)+",");
  
  Serial.print("\"TargetVelX\":" +String(targetVelocityX)+",");
  Serial.print("\"TargetVelZ1\":"+String(targetVelocityZ)+",");
  Serial.print("\"TargetVelZ2\":"+String(targetVelocityZ)+",");
  
  Serial.print("\"TargetVelX_Segments\":[");
  for (int i = 0; i < NUM_VELOCITY_SEGMENTS  ; i++) {
    Serial.print(g_VelocitySegments[i]);
    if (i < NUM_VELOCITY_SEGMENTS-1) { Serial.print(","); }
  }
  Serial.print("],");
  
  Serial.print("\"TargetVelX_Bounds\":[");
  for (int i = 0; i < NUM_VELOCITY_SEGMENTS - 1; i++) {
     Serial.print(g_VelocitySegmentBounds[i]);
     if (i < NUM_VELOCITY_SEGMENTS-2) { Serial.print(","); }
  }
  Serial.print("],");

  Serial.print("\"TargetVelNoiseX\":"+String(targetVelocityNoiseX)+",");
  Serial.print("\"TimeUntilTrial\":"+String(g_TimeUntilTrial));
  
  Serial.println("}");
}

/* Old cmdStatus code:
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["function"] = g_CurrentUpdateFunctionStr;

  root["status"] = g_Status;

  root["PosX"]  = g_CurPosX;
  root["PosZ1"] = g_CurPosZ1;
  root["PosZ2"] = g_CurPosZ2;

  root["MinPosX"] = g_MinPosX;
  root["MaxPosX"] = g_MaxPosX;

  root["VelX"]  = g_CurVelX;
  root["VelZ1"] = g_CurVelZ1;
  root["VelZ2"] = g_CurVelZ2;

  Serial.println("status=");
  Serial.println(String(root.measureLength()));
  root.printTo(Serial);
  return;

  root["TargetPosX"]  = targetPositionX;
  root["TargetPosZ1"] = targetPositionZ;
  root["TargetPosZ2"] = targetPositionZ;

  JsonArray& vTargetVelX       = root.createNestedArray("TargetVelX_Segments");
  JsonArray& vTargetVelXbounds = root.createNestedArray("TargetVelX_Bounds");

  for (int i = 0; i < NUM_VELOCITY_SEGMENTS  ; i++) {
    vTargetVelX.add      (g_VelocitySegments[i]      );
  }
  for (int i = 0; i < NUM_VELOCITY_SEGMENTS - 1; i++) {
    vTargetVelXbounds.add(g_VelocitySegmentBounds[i] );
  }

  root["TargetVelX"]      = targetVelocityX;
  root["TargetVelZ1"]     = targetVelocityZ;
  root["TargetVelZ2"]     = targetVelocityZ;

  root["TargetVelNoiseX"] = targetVelocityNoiseX;

  root["TimeUntilTrial"]  = g_TimeUntilTrial;

  // Write JSON-formatted information to serial
  Serial.println("status=");
  root.printTo(Serial);
  Serial.println("");
*/

// ================================================================
// ====== COMMAND: (Re)Start Kangaroo drivers
// ================================================================

void cmdStartKangaroo(char** pArgs, uint8_t numArgs) {

  Serial.println("Starting Kangaroo drivers");

  // delete previous objects
  if (kangarooSerialX != 0 || kangarooSerialZ != 0) {

    kangarooX->powerDown();
    kangarooZ1->powerDown();
    kangarooZ2->powerDown();

    delete kangarooSerialX;
    delete kangarooSerialZ;
    delete kangarooX;
    delete kangarooZ1;
    delete kangarooZ2;
  }

  // Configure serial ports
  Serial1.begin(115200); // This is the serial port to FlySim
  Serial2.begin(115200); // This is the serial port for FlySim height controller

  // Configure Kangaroo serial
  kangarooSerialX = new KangarooSerial(Serial1);           // This is the X-axis Kangaroo
  kangarooSerialZ = new KangarooSerial(Serial2);           // This is the Z-axis Kangaroo

  delay(1000);

  // Configure controllers
  kangarooX  = new KangarooChannel(*kangarooSerialX, '1');   // This is the FlySim X
  kangarooZ1 = new KangarooChannel(*kangarooSerialZ, '1');  // This is the FlySim pod 1
  kangarooZ2 = new KangarooChannel(*kangarooSerialZ, '2');  // This is the FlySim pod 2

  delay(1000);

  // Configure timeouts
  kangarooX->commandTimeout(100);
  kangarooZ1->commandTimeout(100);
  kangarooZ2->commandTimeout(100);

  // Initialize the X controller
  KangarooError e = KANGAROO_NO_ERROR;

  // Initialize the Z controllers
  e = kangarooZ1->start();
  e = kangarooZ2->start();

  delay(2000);

  e = kangarooZ1->home().wait(KANGAROO_COMMAND_TIMEOUT_MS).status().error();
  if (e != KANGAROO_NO_ERROR) {
    sendError("Kangaroo Z1 failed: ");
    Serial.println(e);
  }
  e = kangarooZ2->home().wait(KANGAROO_COMMAND_TIMEOUT_MS).status().error();
  if (e != KANGAROO_NO_ERROR) {
    sendError("Kangaroo Z2 failed: ");
    Serial.println(e);
  }

  e = kangarooX->start();

  delay(2000);

  e = kangarooX->home().wait(4000).status().error();
  if (e != KANGAROO_NO_ERROR) {
    sendError("Kangaroo X failed: ");
    Serial.println(e);
  }

  Serial.println("Started Kangaroo drivers");
}

// ================================================================
// ====== COMMAND: TUNE
// ================================================================

void cmdTune(char** pArgs, uint8_t numArgs) {

  Serial.println("Starting auto-tuning");

  if (!kangarooAvailable('x')) {
    sendError("Error: Can't tune, X Kangaroo not initialized.");
    return;
  }

  KangarooError e = KANGAROO_NO_ERROR;

  // Enter the desired tune mode.
  long enterModeParams[1] = { TUNE_MODE_NUMBER };
  e = kangarooX->systemCommand(KANGAROO_SYS_TUNE_ENTER_MODE, false, enterModeParams, 1);

  if (e != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_ENTER_MODE failed: ");
    Serial.println(e);
    return;
  }

  // Set the disabled channel bitmask to 0 (tune all channels).
  long disableChannelsParams[1] = { 2 };
  e = kangarooX->systemCommand(KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS, false, disableChannelsParams, 1);

  if (e != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_SET_DISABLED_CHANNELS failed: ");
    Serial.println(e);
    return;
  }

  // Begin the tune.
  long goParams[0];
  e = kangarooX->systemCommand(KANGAROO_SYS_TUNE_GO, false, goParams, 0);

  if (e != KANGAROO_NO_ERROR) {
    sendError("KANGAROO_SYS_TUNE_GO failed: ");
    Serial.println(e);
    return;
  }

  // Save the function we're using
  g_CurrentUpdateFunctionStr = "cmdTune";
  g_CurrentUpdateFunction = &updateTune;
}

void updateTune(long elapsedTime) {

  if ( CurrentTime - g_CurrentCommandStartTime > 30 * 1000000 ) {

    Serial.println("Tuning finished");
    g_CurrentUpdateFunctionStr = "";
    g_CurrentUpdateFunction = NULL;
  }
}

// ================================================================
// ====== COMMAND: Functions to generate complex trajectories
// ================================================================

void cmdTrajectory(char** pArgs, uint8_t numArgs) {

  char buf[MOVEMENT_SEGM_MAX_ARGS][512];
  long numSegmArgs;

  // Parse into movement commands
  for (int i = 0; i < numArgs; i++) {

    numSegmArgs = parse( pArgs[i], (char**) buf, MOVEMENT_SEGM_MAX_ARGS, ',');

    // clear previous values
    memset(g_MovementSegments, 0, sizeof(long) * MOVEMENT_SEGM_MAX_ARGS);

    for (int j = 0; j < numSegmArgs; j++) {

      g_MovementSegments[i][j] = String(buf[j]).toInt();
    }
  }
  g_numMovementSegments = numArgs;

  // Save the function we're using
  g_CurrentUpdateFunctionStr = "cmdTrajectory";
  g_CurrentUpdateFunction = &updateTrajectory;

  // DEBUG
  Serial.print("Read ");
  Serial.print(g_numMovementSegments);
  Serial.println(" parameters.");
}

void updateTrajectory(long elapsedTime) {

  // -----------------------------------
  // Process movement
  // -----------------------------------

  // TODO: Detect stall... If stall detected, go to next segment

  // If Z pos is not correct, move
  if ( abs(g_CurPosZ1 - targetPositionZ) > MAX_X_ERROR ||
       abs(g_CurPosZ2 - targetPositionZ) > MAX_Z_ERROR ) {

    kangarooZ1->p(targetPositionZ / CONVERSION_Z, int(targetVelocityZ / CONVERSION_Z) );
    kangarooZ2->p(targetPositionZ / CONVERSION_Z, int(targetVelocityZ / CONVERSION_Z) );
  }

  // Did we reach our goal for the current segment?
  if ( abs(g_CurPosX - targetPositionX) > MAX_X_ERROR ) {

    // If not, let Kangaroo controllers update system state
    kangarooX->s( int( targetVelocityX / CONVERSION_X ) );

  } else {
    // If so, go to the next segment, or end program
    if (g_curMovementSegment < g_numMovementSegments) {

      if (g_MovementSegments[g_curMovementSegment][0] == MVM_TYPE_WAIT) {

        // TODO: Execute command to quickly slow motor speed. For now use simple delay
        delay( g_MovementSegments[g_curMovementSegment][1] );

      } else if (g_MovementSegments[g_curMovementSegment][0] == MVM_TYPE_LINEAR) {

        targetPositionZ    = g_MovementSegments[g_curMovementSegment][1];
        targetVelocityZ    = g_MovementSegments[g_curMovementSegment][2];
        targetPositionX    = g_MovementSegments[g_curMovementSegment][3];
        targetVelocityX    = g_MovementSegments[g_curMovementSegment][4];
      }
    } else {

      // Finished running segments, so stop updating
      g_CurrentUpdateFunction = NULL;
      g_CurrentUpdateFunctionStr = "";

      // Trigger cortex (to stop)
      // triggerCortex(true);
    }
  }
}

// ================================================================
// ====== COMMAND: Simple positioning
// ================================================================

void cmdZ(char** pArgs, uint8_t numArgs) {

  long speed = 100;
  long z     = 0;

  if (numArgs == 0 || numArgs > 2) {
    Serial.print  ("Invalid parameters (");
    Serial.print  (numArgs);
    Serial.println(" given): 'z <z pos> [<speed>]");
    return;
  }

  if (numArgs >= 1) {
    z     = String(pArgs[0]).toInt();
  }
  if (numArgs == 2) {
    speed = String(pArgs[1]).toInt();
  }

  Serial.print("Moving to Z=");
  Serial.print(z);
  Serial.print(" at speed ");
  Serial.println(speed);

  targetPositionZ = z;
  targetVelocityZ = speed;
}

void cmdX(char** pArgs, uint8_t numArgs) {

  long speed = 100;
  long x     = 0;

  if (numArgs == 0 || numArgs > 2) {
    Serial.print  ("Invalid parameters (");
    Serial.print  (numArgs);
    Serial.println(" given): 'x <x pos> [<speed>]");
    return;
  }

  if (numArgs >= 1) {
    x     = String(pArgs[0]).toInt();
  }
  if (numArgs == 2) {
    speed = String(pArgs[1]).toInt();
  }

  Serial.print("Moving to X=");
  Serial.print(x);
  Serial.print(" at speed ");
  Serial.println(speed);

  targetPositionX = x;
  targetVelocityX = speed;

  g_CurrentUpdateFunction = &updateX;
  g_CurrentUpdateFunctionStr = "cmdX";
}

void updateX(long elapsedTime) {

  if (abs(g_CurPosX - targetPositionX) > MAX_X_ERROR) {
    kangarooX->p(targetPositionX / CONVERSION_X, targetVelocityX / CONVERSION_X);
  } else {
    g_CurrentUpdateFunction = NULL;
    g_CurrentUpdateFunctionStr = "";
  }
}

void cmdSpeedX(char** pArgs, uint8_t numArgs) {

  Serial.println("Bouncing...");
  if (numArgs == 1) {
    long speed = String(pArgs[0]).toInt();
    for (int i = 0; i < 10; i++) {
      kangarooX->s(speed).wait();
      delay(3000);
      kangarooX->s(-speed).wait();
      delay(3000);
    }
  }
}

// ================================================================
// ====== COMMAND: Generate back and forth sequence with delays in
// =====           range [A,B] and absolute speeds in range [C,D],
// =====           and heights in the range [E,F]
// ================================================================

void cmdTrials(char** pArgs, uint8_t numArgs) {
  if (kangarooAvailable()) {
    g_CurrentUpdateFunction = updateTrials;
    g_CurrentUpdateFunctionStr = "cmdTrials";
  } else {
    sendError("Error: Can't start trials, Kangaroo not initialized.");
  }
}

void updateTrials(long elapsedTime) {

  g_TimeUntilTrial -= elapsedTime;
  g_VelocityBoostTimeLeft -= elapsedTime;

  // Are we close to trial start? If so, start cortex
  if (g_TimeUntilTrial < 5 * 1000000) {
    triggerCortex(true);
  }

  // Wait to start trial (so as not to habituate the dF)
  if (g_TimeUntilTrial < 0) {

    // Did we reach our goal for the current segment? (Build in fail-safe, in case tuning is off)
    if ( abs(g_CurPosX - targetPositionX) > MAX_X_ERROR && g_TimeUntilTrial > -8 * 1000000 ) {

      // Determine target velocity (will vary based on current speedups, noise, etc.)
      // Note: This if statement assumes maximum velocity boost duration is shorter than minimum time to traverse segment
      if (g_VelocityBoostTimeLeft <= 0) {
        for (int i = 0; i < NUM_VELOCITY_SEGMENTS; i++) {
          if (g_CurPosX > (i == 0 ? -1000000 : g_VelocitySegmentBounds[i - 1]) && g_CurPosX < (i == NUM_VELOCITY_SEGMENTS - 1 ? 1000000 : g_VelocitySegmentBounds[i])) {
            // After a velocity change, we boost the target velocity
            if (g_VelocityBoostEnabled) {
              g_VelocityBoostDir = g_VelocitySegments[i] - targetVelocityX;
              if (g_VelocityBoostDir != 0) {
                g_VelocityBoostTimeLeft = 50000;
                g_VelocityBoostDir /= abs(g_VelocityBoostDir);
              }
            }
            // update target velocity
            targetVelocityX = g_VelocitySegments[i];
            // Done with loop, found speed
            break;
          }
        }
      }

      // Add noise
      targetVelocityX += currentVelocityNoiseX;
      targetVelocityNoiseTimeLeft -= elapsedTime;
      if (targetVelocityNoiseTimeLeft < 0) {
        // Choose new noise
        currentVelocityNoiseX = (random(0, 2) - 1) * random(0, 1000) * 0.001 * targetVelocityNoiseX;
        targetVelocityNoiseTimeLeft = random(1, 5) * 1000;
      }

      // Add velocity boost
      if (g_VelocityBoostTimeLeft > 0) {
        targetVelocityX += g_VelocityBoostDir * 1000;
      }

      // Enforce maximum velocity to prevent Kangaroo malfunction
      targetVelocityX = max(min(targetVelocityX, 2500), -2500);

      // If not, let Kangaroo controllers update system state
      kangarooX->s( (int) (targetVelocityX + currentVelocityNoiseX) / CONVERSION_X ).wait();

    } else {

      // Wait a little bit longer, then trigger Cortex to stop saving
      delay(2000);
      triggerCortex(false);

      // What direction to move in?
      long xCnt = (g_MinPosX + g_MaxPosX) / 2;

      // Go to the opposite end
      long dst = g_MinPosX;
      if (abs(g_MaxPosX - g_CurPosX) > abs(g_MinPosX - g_CurPosX)) {
        dst = g_MaxPosX;
      }

      long dir   = (dst - g_CurPosX) / abs(dst - g_CurPosX);
      long speed1 = dir > 0 ? random(900, 1100) : random(1200, 1600);
      long speed2 = dir <= 0 ? random(900, 1100) : random(1200, 1600);
      long tWait = random(25, 55) * 1000000;
      long height = random(14, 20) * 25;

      // DEBUG:
      //speed1 = 1000;
      //speed2 = 1600;

      // Set appropriate X speed
      g_VelocitySegments[0] = dir * speed1;
      g_VelocitySegments[1] = dir * (random(0, 100) > 40 ? speed2 : speed1);
      g_VelocitySegmentBounds[0] = (g_MinPosX + g_MaxPosX) * 0.5 + 200;
      targetPositionX = dst;

      // Set appropriate Z position
      targetPositionZ = height;
      targetVelocityZ = 200;

      // Update start time of trial
      g_TimeUntilTrial = tWait;

      // Set noise
      targetVelocityNoiseX = 0;
      if ( false && random(0, 100) > 25 ) {
        targetVelocityNoiseX = random(0, 500);
      }

      // DEBUG:
      //g_TimeUntilTrial = 10000000;
      //height=300;
      //targetVelocityNoiseX = 0;
      //g_VelocitySegments[0] = dir * 900;
      //g_VelocitySegments[0] = dir * 1600;

      //
      Serial.println("Started new trial: ");
      cmdStatus(NULL, NULL);
    }
  }
}

// ================================================================
// ====== COMMAND: Abort (currently targetting Kangaroo)
// ================================================================

void cmdAbort(char** pArgs, uint8_t numArgs) {

  Serial.println("Aborting...");

  // 1.5 sec of no packets to Kangaroo causes a timeout and aborts the program
  delay(1500);

  g_CurrentUpdateFunction = NULL;
  g_CurrentUpdateFunctionStr = "";
}

// ================================================================
// ====== COMMAND: Power down system
// ================================================================

void cmdPowerdown(char** pArgs, uint8_t numArgs) {

  targetPositionZ = 0;
  targetVelocityZ = 20;

  g_CurrentUpdateFunction = updatePowerdown;
  g_CurrentUpdateFunctionStr = "cmdPowerdown";
}

void updatePowerdown(long elapsedTime) {

  if (g_CurPosZ1 < 50 && g_CurPosZ2 < 50) {

    kangarooX->powerDown();
    kangarooZ1->powerDown();
    kangarooZ2->powerDown();
  }
}

void cmdNeutral(char** pArgs, uint8_t numArgs) {

  kangarooX->powerDown();
}

// ================================================================
// ====== COMMAND: Power down system
// ================================================================

void cmdExploreSpace(char** pArgs, uint8_t numArgs) {

  targetPositionZ = 0;
  targetVelocityZ = 100;

  targetPositionX = -500;
  targetVelocityX = 200;
  
  g_CurrentUpdateFunction = updateExploreSpace;
  g_CurrentUpdateFunctionStr = "cmdExploreSpace";
}

void updateExploreSpace(long elapsedTime) {

  /* Print position */
  Serial.println("pos,"+String(g_CurPosX)+","+String(g_CurPosZ1)+","+String(g_CurPosZ2));

  /* Switch to next position if time has elapsed */
  g_TimeUntilNextPosition -= elapsedTime;
  if (g_TimeUntilNextPosition < 0) {
    targetPositionX += 10;
    /* Reached the end of the line? */
    if (targetPositionX > 500) {
      targetPositionX = -500;
      targetPositionZ += 25;
    }
    /* Reached the end of the whole plane? */
    if (targetPositionZ > 550) {
      targetPositionZ = 0;
      targetPositionX = 0;
    
      g_CurrentUpdateFunction = NULL;
      g_CurrentUpdateFunctionStr = "";

      Serial.println("finished exploring space");
    }
  }
}

// ================================================================
// Route serial commands to the right function
// ================================================================

void processSerialCommand(char** pArgs, uint8_t numArgs) {

  String cmd = String( pArgs[0] );
  void (*fun)(char**, uint8_t);

  // Determine function to execute
  Serial.println(cmd);
  if (cmd == "status"     ) {
    fun = &cmdStatus;
  } else if (cmd == "h"          ) {
    fun = &cmdIdentify;
  } else if (cmd == "tune"       ) {
    fun = &cmdTune;
  } else if (cmd == "z"          ) {
    fun = &cmdZ;
  } else if (cmd == "x"          ) {
    fun = &cmdX;
  } else if (cmd == "trials"     ) {
    fun = &cmdTrials;
  } else if (cmd == "kangaroo"   ) {
    fun = &cmdStartKangaroo;
  } else if (cmd == "abort"      ) {
    fun = &cmdAbort;
  } else if (cmd == "powerdown"  ) {
    fun = &cmdPowerdown;
  } else if (cmd == "neutral"    ) {
    fun = &cmdNeutral;
  } else if (cmd == "speedx"     ) {
    fun = &cmdSpeedX;
  } else if (cmd == "trajectory" ) {
    fun = &cmdTrajectory;
  } else if (cmd == "explore") {
    fun = &cmdExploreSpace;
  } else {

    // Command not recognized....
    sendError("Command not recognized.");
    Serial.println(cmd);
    return;
  }

  if (g_CurrentUpdateFunction != NULL && fun != &cmdAbort && fun != &cmdStatus) {
    sendError("Aborting command, other function in progress");
    return;
  }

  // Register time of function start
  g_CurrentCommandStartTime = CurrentTime;

  // Execute selected function
  fun( &(pArgs[1]), numArgs - 1 );
}

// ================================================================
// Loop
// ================================================================

void loop() {

  // -----------------------------------
  // Check that Kangaroos are operating correctly (i.e. actually on)
  // -----------------------------------

  // NOTE: By checking this, the program remains stable when Kangaroos fail / are not turned on yet
  // TODO: Implement automatic re-connect after Kangaroo power turn-off/turn-on sequence... Or is this built-in?

  bool kx = kangarooAvailable('x');
  bool kz = kangarooAvailable('z');

  if ( !kx && !kz ) {
    g_Status = "Unable to connect to either Kangaroo.";
  } else if ( !kx &&  kz ) {
    g_Status = "Unable to connect to X Kangaroo.";
  } else if (  kx && !kz ) {
    g_Status = "Unable to connect to Z Kangaroo.";
  } else {
    g_Status = "OK";
  }

  // -----------------------------------
  // Read the sensor data
  // -----------------------------------

  // IMPORTANT NOTE: These commands matter beyond simply updating sensor values.
  //                 The requests signal to the Kangaroo drivers that we wish the commands
  //                 (e.g. tuning) to continue!

  if (kx) {
    g_CurPosX  = kangarooX->getP().value()  * CONVERSION_X;
    g_CurVelX  = kangarooX->getS().value()  * CONVERSION_X;

    // Get information on bounds
    g_MinPosX = kangarooX->getMax().value() / 2;
    g_MaxPosX = kangarooX->getMin().value() / 2;
  }

  if (kz) {
    g_CurPosZ1 = kangarooZ1->getP().value() * CONVERSION_Z;
    g_CurPosZ2 = kangarooZ2->getP().value() * CONVERSION_Z;
    g_CurVelZ1 = kangarooZ1->getS().value() * CONVERSION_Z;
    g_CurVelZ2 = kangarooZ2->getS().value() * CONVERSION_Z;
  }

  // -----------------------------------
  // Maintain Z
  // -----------------------------------

  if (kz) {
    kangarooZ1->p(targetPositionZ / CONVERSION_Z, targetVelocityZ / CONVERSION_Z);
    kangarooZ2->p(targetPositionZ / CONVERSION_Z, targetVelocityZ / CONVERSION_Z);
  }

  // -----------------------------------
  // Update timing variables
  // -----------------------------------

  DF_check();  // Testing the DF status
  CurrentTime = micros();
  long elapsedTime = CurrentTime - LastTime;

  // -----------------------------------
  // Stop command
  // -----------------------------------

  long STOP = digitalRead(PIN_STOP);

  if ( STOP == 1 ) {

    kangarooZ1->powerDown();
    kangarooZ2->powerDown();
  }

  // -----------------------------------
  // Read PC inputs
  // -----------------------------------

  while ( Serial.available() > 0 )
  {
    char c = Serial.read();

    if ( c != '\n' ) {
      if (g_PCSerialBufferIdx > PC_SERIAL_BUF_LENGTH - 1) {
        // Error: Out of range
        sendError("Error: serial input buffer overflow. Resetting buffer.");
        // Clear buffer
        memset(g_PCSerialBuffer, 0, PC_SERIAL_BUF_LENGTH);
        g_PCSerialBufferIdx = 0;
      } else {
        // Save the new character and continue
        g_PCSerialBuffer[g_PCSerialBufferIdx] = c;
        g_PCSerialBufferIdx += 1;
      }
    } else {
      // Parse arguments
      uint8_t numArgs = parse( (char*) g_PCSerialBuffer,
                               g_PCSerialArgs, PC_SERIAL_MAX_ARGS);
      // Execute command
      processSerialCommand( (char**) g_PCSerialArgs, numArgs);
      // Clear buffer
      memset(g_PCSerialBuffer, 0, PC_SERIAL_BUF_LENGTH);
      g_PCSerialBufferIdx = 0;
    }
  }

  // -----------------------------------
  // Update the currently running function
  // -----------------------------------

  // This depends on what command was executed
  if (g_CurrentUpdateFunction != NULL) {
    g_CurrentUpdateFunction(elapsedTime);
  }

  // Update time
  LastTime = CurrentTime;
}
