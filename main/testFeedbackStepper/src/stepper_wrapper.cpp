#include <Arduino.h>
#include <FastAccelStepper.h>
#include <TMCStepper.h>

// ---- Pins from your pinConfig ------------------
static constexpr int STEP_PIN = 8;    // D9  -> GPIO8
static constexpr int DIR_PIN  = 7;    // D8  -> GPIO7
static constexpr int EN_PIN   = 9;    // D10 -> GPIO9 (active LOW)

static constexpr int TMC_RX   = 44;   // D7  -> GPIO44
static constexpr int TMC_TX   = 43;   // D6  -> GPIO43
static constexpr float R_SENSE = 0.11f; // adjust to your board

// ---- Motion limits -----------------------------
static constexpr uint32_t MAX_SPEED_HZ   = 25000;   // 25 ksteps/s
static constexpr uint32_t ACCEL_HZ_S     = 100000;  // 100 ksteps/s²

// ---- Globals -----------------------------------
static FastAccelStepperEngine engine;
static FastAccelStepper* stepper = nullptr;

HardwareSerial TMCSerial(1);
TMC2209Stepper driver(&TMCSerial, R_SENSE, 0b00);  // addr 0b00 – adapt if needed

void tmc_init() {
    TMCSerial.begin(115200, SERIAL_8N1, TMC_RX, TMC_TX);

    driver.begin();
    driver.toff(4);
    driver.blank_time(24);
    driver.rms_current(1050); // from your pinConfig
    driver.microsteps(16);

    driver.TCOOLTHRS(0xFFFFF);
    driver.semin(5);
    driver.semax(2);
    driver.sedn(0b01);
    //driver.sgt(100); // stall threshold - tune!
    driver.microsteps(16);
}

void stepper_init() {
    engine.init();

    stepper = engine.stepperConnectToPin(STEP_PIN);
    if (!stepper) {
        Serial.println("FATAL: stepperConnectToPin() failed");
        while (true) { delay(1000); }
    }

    stepper->setDirectionPin(DIR_PIN, false);
    stepper->setEnablePin(EN_PIN);
    stepper->setAutoEnable(false);
    stepper->enableOutputs();

    stepper->setSpeedInHz(MAX_SPEED_HZ);
    stepper->setAcceleration(ACCEL_HZ_S);

    tmc_init();
}

void stepper_poll() {
}

void stepper_move_to(int32_t pos) {
    if (!stepper) return;
    stepper->moveTo(pos);
}

void stepper_set_velocity(float steps_per_sec) {
    if (!stepper) return;
    
    // Clamp velocity to safe limits
    float clamped = constrain(steps_per_sec, -(float)MAX_SPEED_HZ, (float)MAX_SPEED_HZ);
    
    if (fabsf(clamped) < 10.0f) {
        // Below minimum speed threshold - stop motor
        stepper->stopMove();
        return;
    }
    
    // Set speed and run in appropriate direction
    stepper->setSpeedInHz((uint32_t)fabsf(clamped));
    if (clamped > 0) {
        stepper->runForward();
    } else {
        stepper->runBackward();
    }
}

void stepper_stop() {
    if (!stepper) return;
    stepper->forceStop();
}

int32_t stepper_get_pos() {
    return stepper ? stepper->getCurrentPosition() : 0;
}

int32_t stepper_get_target() {
    return stepper ? stepper->targetPos() : 0;
}

bool stepper_is_busy() {
    return stepper && stepper->isRunning();
}
