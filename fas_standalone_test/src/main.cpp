// ===========================================================================
//  Standalone FastAccelStepper test  -  Seeed XIAO ESP32-S3 + TMC2209
// ---------------------------------------------------------------------------
//  NOTHING from the UC2 firmware runs here: no CANopen, no SerialProcess task,
//  no FocusMotor module loop, no DeviceRouter. Just Arduino setup()/loop(),
//  one TMC2209 over UART, and one FastAccelStepper axis on the RMT backend.
//
//  Goal: reproduce / rule out the "motor stalls at certain accel values"
//  behaviour in isolation, so we can tell whether it is a property of
//  FastAccelStepper + this hardware, or an integration/concurrency problem in
//  the full firmware (CANopen stack + serial task contending with the FAS
//  queue-filler task).
//
//  USAGE (serial console @115200, USB-CDC):
//     ?            show help
//     w <hz>       set speed (Hz)            e.g.  w 10000
//     c <accel>    set acceleration          e.g.  c 100000
//     i <mA>       set TMC run current
//     m <delta>    relative move  (move)     e.g.  m 2000
//     t <abs>      absolute move  (moveTo)   e.g.  t 0
//     p            print live state
//     z            zero the position counter
//     x            force-stop now
//     s            run the full speed x accel SWEEP (auto report)
/*
p
w 10000
c 100000
m 2000

*/
//  The sweep issues one bounded move per (speed,accel) cell and reports for
//  each whether the ramp COMPLETED (rampState->IDLE, queue empty) or STALLED
//  (timed out still busy). A compact PASS/FAIL grid is printed at the end.
// ===========================================================================

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <TMCStepper.h>
#include "esp_system.h"   // esp_reset_reason()

// ----------------------------- Hardware pins -------------------------------
// Copied verbatim from config/UC2_canopen_slave_motor/PinConfig.h
//   MOTOR_X_STEP = GPIO8 (D9)   MOTOR_X_DIR = GPIO7 (D8)   MOTOR_ENABLE = GPIO9 (D10)
//   TMC UART: RX = GPIO44 (D7), TX = GPIO43 (D6)
static const int8_t PIN_STEP   = 8;
static const int8_t PIN_DIR    = 7;
static const int8_t PIN_ENABLE = 9;
static const bool   ENABLE_INVERTED = true;   // active-low driver enable

static const int8_t TMC_RX = 44;   // ESP32 <- TMC (PDN/UART)
static const int8_t TMC_TX = 43;   // ESP32 -> TMC

// ----------------------------- TMC settings --------------------------------
#define ENABLE_TMC      1          // set 0 if you want to drive STEP/DIR only
#define R_SENSE         0.2f
#define DRIVER_ADDRESS  0b00
static const int   TMC_MICROSTEPS  = 16;
static const int   TMC_RMS_CURRENT = 1050;
static const int   TMC_TOFF        = 4;
static const int   TMC_BLANK_TIME  = 24;

#if ENABLE_TMC
TMC2209Stepper driver(&Serial1, R_SENSE, DRIVER_ADDRESS);
#endif

// ----------------------------- FAS objects ---------------------------------
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper      *stepper = nullptr;

// ----------------------------- Defaults ------------------------------------
static int32_t g_speed = 10000;    // Hz
static int32_t g_accel = 100000;   // steps/s^2  (the value that stalls in fw)

// FAS time base on esp32: TICKS_PER_S is a macro (= 16000000L) from the
// FastAccelStepper headers. We reuse it directly as a double in arithmetic.

// First step period from rest depends ONLY on acceleration:
//   t1 = (TICKS_PER_S / sqrt(2)) / sqrt(accel)   [ticks]
static double firstStepTicks(double accel) {
  if (accel < 1) accel = 1;
  return (TICKS_PER_S / 1.41421356237) / sqrt(accel);
}

static const char *rampName(uint8_t rs) {
  switch (rs & RAMP_STATE_MASK) {
    case RAMP_STATE_IDLE:        return "IDLE";
    case RAMP_STATE_COAST:       return "COAST";
    case RAMP_STATE_ACCELERATE:  return "ACCEL";
    case RAMP_STATE_DECELERATE:  return "DECEL";
    default:                     return "MIX";
  }
}

static void printRefThresholds() {
  Serial.println();
  Serial.println(F("--- FAS step-period reference (esp32 RMT, 16 MHz) -------------"));
  Serial.println(F("  first-step period  t1 = 11.31e6 / sqrt(accel)  [ticks]"));
  Serial.println(F("  single RMT word ceiling : 32767 ticks  (~488 Hz)"));
  Serial.println(F("  16-bit queue-entry ceiling: 65535 ticks (~244 Hz)"));
  Serial.println(F("  below the 65535 ceiling FAS must emit pause-commands"));
  Serial.println(F("  examples:"));
  const int32_t a[] = {1000, 10000, 100000, 200000, 1000000};
  for (uint8_t i = 0; i < 5; i++) {
    double t1 = firstStepTicks(a[i]);
    Serial.printf("    accel=%-8ld  t1=%8.0f ticks  (%6.1f Hz)  %s\n",
                  (long)a[i], t1, TICKS_PER_S / t1,
                  (t1 > 65535.0) ? "> 65535 ceiling" :
                  (t1 > 32767.0) ? "> 1-word, needs 2 words" : "fits 1 word");
  }
  Serial.println(F("---------------------------------------------------------------"));
}

// ----------------------------- TMC setup -----------------------------------
static void setupTMC() {
#if ENABLE_TMC
  Serial1.begin(115200, SERIAL_8N1, TMC_RX, TMC_TX);
  delay(50);
  driver.begin();
  driver.pdn_disable(true);        // use UART, not standalone pins
  driver.mstep_reg_select(1);      // microsteps from UART register
  driver.intpol(true);
  driver.toff(TMC_TOFF);
  driver.blank_time(TMC_BLANK_TIME);
  driver.rms_current(TMC_RMS_CURRENT);
  driver.microsteps(TMC_MICROSTEPS);
  delay(10);

  uint8_t conn = driver.test_connection();   // 0 = OK
  Serial.printf("[TMC] test_connection = %u (%s)\n", conn,
                conn == 0 ? "OK" : "NO REPLY - check wiring/power");
  Serial.printf("[TMC] microsteps=%u  rms_current=%u mA  toff=%u\n",
                driver.microsteps(), driver.rms_current(), TMC_TOFF);
#else
  Serial.println("[TMC] disabled (ENABLE_TMC=0) - driving STEP/DIR only");
#endif
}

// ----------------------------- FAS setup -----------------------------------
// Backend selectable at build time so we can A/B the RMT backend against the
// MCPWM+PCNT backend on the SAME hardware. On ESP32-S3 + IDF4 FAS supports
// BOTH (QUEUES_MCPWM_PCNT=4, QUEUES_RMT=4). The RMT backend strands the final
// 1-12 steps of a move at low instantaneous step rates (isRunning stuck true,
// position a few steps short); MCPWM+PCNT is the original, battle-tested path.
// Build the MCPWM variant with:  -DFAS_DRIVER=DRIVER_MCPWM_PCNT
#ifndef FAS_DRIVER
#define FAS_DRIVER DRIVER_RMT
#endif
#if (FAS_DRIVER == DRIVER_MCPWM_PCNT)
#define FAS_DRIVER_NAME "DRIVER_MCPWM_PCNT"
#else
#define FAS_DRIVER_NAME "DRIVER_RMT"
#endif
static void setupFAS() {
  engine.init();
  stepper = engine.stepperConnectToPin(PIN_STEP, FAS_DRIVER);
  if (stepper == nullptr) {
    Serial.println("[FAS] stepperConnectToPin(" FAS_DRIVER_NAME ") FAILED - halting");
    while (true) delay(1000);
  }
  stepper->setDirectionPin(PIN_DIR);
  stepper->setEnablePin(PIN_ENABLE, ENABLE_INVERTED);
  stepper->setAutoEnable(false);
  stepper->enableOutputs();
  stepper->setCurrentPosition(0);
  stepper->setSpeedInHz(g_speed);
  stepper->setAcceleration(g_accel);
  Serial.println("[FAS] engine + stepper ready (" FAS_DRIVER_NAME ")");
}

// ----------------------------- Move + watch --------------------------------
enum MoveResult { MR_COMPLETE, MR_STALL, MR_REJECTED };

// Arms speed/accel, issues a move, then polls rampState/position until the
// ramp returns to IDLE (complete) or the timeout elapses (stall).
static MoveResult runMove(int32_t target, bool absolute, uint32_t timeout_ms,
                          bool verbose) {
  // make sure we start from rest with a clean queue
  if (stepper->isRunning() || !stepper->isQueueEmpty()) {
    stepper->forceStopAndNewPosition(stepper->getCurrentPosition());
    uint32_t t = millis();
    while ((stepper->isRunning() || !stepper->isQueueEmpty()) &&
           millis() - t < 200) delay(1);
  }

  int8_t sr = stepper->setSpeedInHz(g_speed);
  int8_t ar = stepper->setAcceleration(g_accel);
  if (sr != 0 || ar != 0)
    Serial.printf("  [warn] setSpeed rc=%d  setAccel rc=%d\n", sr, ar);

  const int32_t startPos = stepper->getCurrentPosition();
  const double  t1 = firstStepTicks(g_accel);

  if (verbose)
    Serial.printf("MOVE %s%ld  speed=%ld accel=%ld | t1=%.0f ticks (%.0f Hz)\n",
                  absolute ? "->" : "+", (long)target,
                  (long)g_speed, (long)g_accel, t1, TICKS_PER_S / t1);

  const int32_t targetAbs = absolute ? target : startPos + target;

  int8_t mres = absolute ? stepper->moveTo(target, false)
                         : stepper->move(target, false);
  if (mres != MOVE_OK) {
    Serial.printf("  -> REJECTED (rc=%d)\n", mres);
    return MR_REJECTED;
  }

  // Zero-distance move: nothing to do.
  if (targetAbs == startPos) {
    if (verbose) Serial.println("  -> COMPLETE (zero distance)");
    return MR_COMPLETE;
  }

  const uint32_t t0 = millis();
  int32_t  lastPos      = startPos;
  uint32_t lastProgress = t0;
  uint8_t  rampSeen     = 0;
  bool     everRan      = false;
  bool     started      = false;   // becomes true once the move is observed to begin

  while (millis() - t0 < timeout_ms) {
    const uint8_t  rs  = stepper->rampState() & RAMP_STATE_MASK;
    const int32_t  pos = stepper->getCurrentPosition();
    const bool     qE  = stepper->isQueueEmpty();
    const bool     run = stepper->isRunning();
    rampSeen |= rs;
    if (run) everRan = true;

    // The FAS StepperTask refills the queue every ~4 ms, so for the first few
    // ms after moveTo() the ramp is still IDLE/empty. Only treat IDLE+empty as
    // "complete" AFTER we have actually seen the move start (motion, a non-IDLE
    // ramp state, a non-empty queue, or isRunning()).
    if (run || rs != RAMP_STATE_IDLE || !qE || pos != startPos) started = true;

    if (started && rs == RAMP_STATE_IDLE && qE && !run) {
      const int32_t endPos = stepper->getCurrentPosition();
      if (verbose)
        Serial.printf("  -> COMPLETE in %lu ms  pos=%ld (moved %ld, target %ld)\n",
                      (unsigned long)(millis() - t0), (long)endPos,
                      (long)(endPos - startPos), (long)targetAbs);
      return MR_COMPLETE;
    }
    if (pos != lastPos) { lastPos = pos; lastProgress = millis(); }
    delay(2);
  }

  // ---- timed out: STALL (or never started) ----
  const int32_t endPos = stepper->getCurrentPosition();
  const uint8_t rs     = stepper->rampState();
  Serial.printf("  -> *** %s *** %lums  rampState=0x%02x(%s)  pos %ld->%ld "
                "(moved %ld, target %ld)  queueEmpty=%d isRunning=%d everRan=%d "
                "rampSeen=0x%02x  stuckFor=%lums\n",
                started ? "STALL" : "NEVER-STARTED",
                (unsigned long)(millis() - t0), rs, rampName(rs),
                (long)startPos, (long)endPos, (long)(endPos - startPos),
                (long)targetAbs,
                stepper->isQueueEmpty(), stepper->isRunning(), everRan,
                rampSeen, (unsigned long)(millis() - lastProgress));
  // leave it stopped for the next test
  stepper->forceStopAndNewPosition(endPos);
  return MR_STALL;
}

// ----------------------------- The sweep -----------------------------------
static const int32_t SWEEP_SPEEDS[] = {1000, 2000, 5000, 10000, 20000, 40000, 80000};
static const int32_t SWEEP_ACCELS[] = {1000, 2000, 5000, 10000, 20000, 50000,
                                       100000, 200000, 500000, 1000000};
static const uint8_t NS = sizeof(SWEEP_SPEEDS) / sizeof(SWEEP_SPEEDS[0]);
static const uint8_t NA = sizeof(SWEEP_ACCELS) / sizeof(SWEEP_ACCELS[0]);

static const int32_t  SWEEP_DIST       = 1600;   // steps per cell
static const uint32_t SWEEP_TIMEOUT_MS = 4000;

static void runSweep() {
  Serial.println();
  Serial.println(F("################  SWEEP START  ################"));
  Serial.printf ("dist=%ld steps/cell, timeout=%lu ms\n",
                 (long)SWEEP_DIST, (unsigned long)SWEEP_TIMEOUT_MS);
  printRefThresholds();

  static char grid[NA][NS];   // 'O'=ok 'S'=stall 'r'=rejected
  uint16_t nOk = 0, nStall = 0, nRej = 0;

  for (uint8_t ia = 0; ia < NA; ia++) {
    for (uint8_t is = 0; is < NS; is++) {
      g_speed = SWEEP_SPEEDS[is];
      g_accel = SWEEP_ACCELS[ia];

      // alternate direction so the stage oscillates near origin
      stepper->setCurrentPosition(0);
      const int32_t target = ((ia * NS + is) & 1) ? -SWEEP_DIST : SWEEP_DIST;

      Serial.printf("[a=%-7ld s=%-6ld] ", (long)g_accel, (long)g_speed);
      MoveResult r = runMove(target, /*absolute=*/true, SWEEP_TIMEOUT_MS,
                             /*verbose=*/true);
      grid[ia][is] = (r == MR_COMPLETE) ? 'O' : (r == MR_STALL) ? 'S' : 'r';
      if (r == MR_COMPLETE) nOk++; else if (r == MR_STALL) nStall++; else nRej++;
      delay(60);   // let things settle between cells
    }
  }

  // ---- summary grid ----
  Serial.println();
  Serial.println(F("######## SWEEP RESULT  (O=ok  S=stall  r=rejected) ########"));
  Serial.print(F("accel \\ speed |"));
  for (uint8_t is = 0; is < NS; is++) Serial.printf("%7ld", (long)SWEEP_SPEEDS[is]);
  Serial.println();
  Serial.print(F("--------------+"));
  for (uint8_t is = 0; is < NS; is++) Serial.print(F("-------"));
  Serial.println();
  for (uint8_t ia = 0; ia < NA; ia++) {
    Serial.printf("%13ld |", (long)SWEEP_ACCELS[ia]);
    for (uint8_t is = 0; is < NS; is++) Serial.printf("%7c", grid[ia][is]);
    Serial.println();
  }
  Serial.printf("\nTotals: OK=%u  STALL=%u  REJECTED=%u  (of %u)\n",
                nOk, nStall, nRej, (unsigned)(NA * NS));
  Serial.println(F("################   SWEEP END   ################\n"));

  // restore defaults
  g_speed = 10000; g_accel = 100000;
  stepper->setCurrentPosition(0);
  stepper->setSpeedInHz(g_speed);
  stepper->setAcceleration(g_accel);
}

// ----------------------------- State print ---------------------------------
static void printState() {
  Serial.printf("[state] speed=%ld accel=%ld | pos=%ld qEnd=%ld rampState=0x%02x(%s) "
                "isRunning=%d queueEmpty=%d\n",
                (long)g_speed, (long)g_accel,
                (long)stepper->getCurrentPosition(),
                (long)stepper->getPositionAfterCommandsCompleted(),
                stepper->rampState(), rampName(stepper->rampState()),
                stepper->isRunning(), stepper->isQueueEmpty());
}

// ----------------------------- Serial console ------------------------------
static void printHelp() {
  Serial.println();
  Serial.println(F("=========== FAS standalone test ==========="));
  Serial.println(F(" ?            help"));
  Serial.println(F(" w <hz>       set speed       (e.g. w 10000)"));
  Serial.println(F(" c <accel>    set accel       (e.g. c 100000)"));
  Serial.println(F(" i <mA>       set TMC current"));
  Serial.println(F(" m <delta>    relative move   (e.g. m 2000)"));
  Serial.println(F(" t <abs>      absolute move   (e.g. t 0)"));
  Serial.println(F(" p            print state"));
  Serial.println(F(" z            zero position"));
  Serial.println(F(" x            force stop"));
  Serial.println(F(" s            run full sweep"));
  Serial.println(F("==========================================="));
  Serial.printf ("current: speed=%ld accel=%ld\n", (long)g_speed, (long)g_accel);
}

static void handleLine(char *line) {
  // trim leading spaces
  while (*line == ' ' || *line == '\t') line++;
  if (*line == 0) return;
  const char cmd = *line;
  long arg = 0;
  const char *p = line + 1;
  while (*p == ' ') p++;
  bool hasArg = (*p != 0);
  if (hasArg) arg = strtol(p, nullptr, 10);

  switch (cmd) {
    case '?': case 'h': printHelp(); break;
    case 'w': if (hasArg) { g_speed = arg; stepper->setSpeedInHz(g_speed);
                Serial.printf("speed=%ld\n", (long)g_speed); } break;
    case 'c': if (hasArg) { g_accel = arg; stepper->setAcceleration(g_accel);
                Serial.printf("accel=%ld (t1=%.0f ticks, %.0f Hz)\n", (long)g_accel,
                              firstStepTicks(g_accel), TICKS_PER_S / firstStepTicks(g_accel)); } break;
    case 'i': if (hasArg) {
#if ENABLE_TMC
                driver.rms_current(arg);
                Serial.printf("TMC current=%ld mA (readback %u)\n", arg, driver.rms_current());
#else
                Serial.println("TMC disabled");
#endif
              } break;
    case 'm': if (hasArg) runMove(arg, /*absolute=*/false, 8000, true); break;
    case 't': if (hasArg) runMove(arg, /*absolute=*/true,  8000, true); break;
    case 'p': printState(); break;
    case 'z': stepper->setCurrentPosition(0); Serial.println("pos=0"); break;
    case 'x': stepper->forceStopAndNewPosition(stepper->getCurrentPosition());
              Serial.println("force-stopped"); break;
    case 's': runSweep(); break;
    default:  Serial.printf("unknown '%c' - type ? for help\n", cmd); break;
  }
}

static char   s_buf[64];
static uint8_t s_len = 0;

static void pollSerial() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') { s_buf[s_len] = 0; handleLine(s_buf); s_len = 0; }
    else if (s_len < sizeof(s_buf) - 1) s_buf[s_len++] = ch;
  }
}

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_SW:       return "SW";
    case ESP_RST_PANIC:    return "PANIC (crash!)";
    case ESP_RST_INT_WDT:  return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT:      return "WDT";
    case ESP_RST_BROWNOUT: return "BROWNOUT (power!)";
    case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
    default:               return "OTHER";
  }
}

// staged boot marker: prints and flushes so output drains BEFORE any fault
static void stage(const char *s) { Serial.println(s); Serial.flush(); delay(20); }

// ----------------------------- Arduino entry -------------------------------
void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 3000) delay(10);   // wait for USB-CDC
  delay(400);

  esp_reset_reason_t rr = esp_reset_reason();
  Serial.println();
  Serial.println(F("##############################################"));
  Serial.println(F("#  Standalone FastAccelStepper test (XIAO S3) #"));
  Serial.println(F("#  STEP=GPIO8  DIR=GPIO7  EN=GPIO9 (act-low)   #"));
  Serial.println(F("##############################################"));
  Serial.printf("[boot] reset reason = %d (%s)   free heap = %u\n",
                (int)rr, resetReasonName(rr), (unsigned)ESP.getFreeHeap());
  Serial.flush();

  stage("[boot] -> setupTMC");
  setupTMC();
  stage("[boot] -> setupFAS");
  setupFAS();
  stage("[boot] -> ready");
  printRefThresholds();
  printHelp();
  Serial.println(F("\nType 's' to run the sweep, or drive moves manually.\n"));
}

void loop() {
  pollSerial();
  delay(2);
}
