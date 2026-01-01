#include <Arduino.h>
#include "stepper_wrapper.h"
#include "encoder_wrapper.h"

// Mechanical conversion: steps per encoder count
// Adjust to your mechanics (e.g., 200*16 steps/rev / 1000 enc counts/rev = 3.2)
static constexpr float STEPS_PER_ENC = 1.0f;

// ============= PID Controller Parameters =============
// Two-stage controller: aggressive when far, precise when near

// FAR mode: aggressive parameters for fast approach (|error| > NEAR_THRESHOLD)
static constexpr float KP_FAR = 80.0f;     // Higher P for fast response
static constexpr float KI_FAR = 0.0f;      // No integral during fast approach
static constexpr float KD_FAR = 8.0f;      // Some damping to prevent overshoot

// NEAR mode: precise parameters for final positioning (|error| <= NEAR_THRESHOLD)
static constexpr float KP_NEAR = 100.0f;    // High P for strong final correction
static constexpr float KI_NEAR = 5.0f;     // Integral to eliminate steady-state error
static constexpr float KD_NEAR = 8.0f;     // Higher D to prevent oscillation

// Threshold to switch between modes (in encoder counts)
static constexpr int32_t NEAR_THRESHOLD = 50;

static constexpr float MAX_VELOCITY = 15000.0f;   // Max commanded velocity (steps/sec)
static constexpr float MIN_VELOCITY = 80.0f;      // Min velocity before stopping (increased)
static constexpr int32_t POSITION_TOLERANCE = 1;  // Encoder counts tolerance for "arrived"
static constexpr float INTEGRAL_LIMIT = 500.0f;   // Anti-windup limit for integral term

// Control loop timing
static constexpr uint32_t CONTROL_INTERVAL_US = 1000; // 1ms = 1kHz control loop

// ============= Stall Detection Parameters =============
static constexpr int32_t STALL_NOISE_THRESHOLD = 3;     // Position change below this = not moving
static constexpr int32_t STALL_ERROR_THRESHOLD = 20;    // Only detect stall if error > this
static constexpr uint32_t STALL_TIME_MS = 150;          // Time without movement to detect stall
static constexpr uint8_t STALL_MAX_RETRIES = 3;         // Max retry attempts before error
static constexpr float STALL_VELOCITY_REDUCTION = 0.5f; // Reduce velocity by this factor on retry

// ============= Control State =============
static int32_t enc_setpoint = 0;
static float integral_error = 0.0f;
static int32_t last_error = 0;
static uint32_t last_control_time_us = 0;
static bool position_reached = true;

// Stall detection state
static int32_t stall_reference_pos = 0;      // Position when stall check started
static uint32_t stall_check_start_ms = 0;    // Time when stall check started
static uint8_t stall_retry_count = 0;        // Current retry attempt
static float velocity_limit_factor = 1.0f;   // Current velocity limit factor (reduced on stall)
static bool stall_error_state = false;       // True if motor is in error state

String input_line;

void handle_serial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;

        if (c == '\n') {
            if (input_line.length() > 0) {
                long val = input_line.toInt();
                enc_setpoint = val;

                // Reset PID state for new target
                integral_error = 0.0f;
                last_error = enc_setpoint - encoder_get();
                position_reached = false;
                
                // Reset stall detection state
                stall_retry_count = 0;
                velocity_limit_factor = 1.0f;
                stall_error_state = false;
                stall_reference_pos = encoder_get();
                stall_check_start_ms = millis();
                
                Serial.print("New targetEncPos: ");
                Serial.println(enc_setpoint);
            }
            input_line = "";
        } else {
            input_line += c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Closed-loop XIAO ESP32S3 demo");

    stepper_init();
    encoder_init();
    encoder_set_zero();
}

// PID velocity controller - runs at high frequency for smooth control
void run_pid_controller() {
    uint32_t now_us = micros();
    uint32_t dt_us = now_us - last_control_time_us;
    
    // Only run at specified interval
    if (dt_us < CONTROL_INTERVAL_US) return;
    last_control_time_us = now_us;
    
    float dt = dt_us * 1e-6f; // Convert to seconds
    
    // Get current position error
    int32_t enc_pos = encoder_get();
    int32_t error = enc_setpoint - enc_pos;
    
    // === Stall Detection ===
    if (stall_error_state) {
        // Motor is in error state - do nothing until new setpoint
        stepper_stop();
        return;
    }
    
    // Only check for stall if we have significant error (not near target)
    if (abs(error) > STALL_ERROR_THRESHOLD) {
        uint32_t now_ms = millis();
        int32_t pos_change = abs(enc_pos - stall_reference_pos);
        
        if (pos_change > STALL_NOISE_THRESHOLD) {
            // Position is changing - reset stall detection
            stall_reference_pos = enc_pos;
            stall_check_start_ms = now_ms;
        } else if (now_ms - stall_check_start_ms > STALL_TIME_MS) {
            // Stall detected! Position hasn't changed enough
            stall_retry_count++;
            
            if (stall_retry_count >= STALL_MAX_RETRIES) {
                // Max retries exceeded - enter error state
                stall_error_state = true;
                stepper_stop();
                Serial.println("ERROR: Stall detected - max retries exceeded!");
                Serial.print("  Position: ");
                Serial.print(enc_pos);
                Serial.print("  Target: ");
                Serial.println(enc_setpoint);
                return;
            }
            
            // Reduce velocity and retry
            velocity_limit_factor *= STALL_VELOCITY_REDUCTION;
            stall_reference_pos = enc_pos;
            stall_check_start_ms = now_ms;
            integral_error = 0.0f; // Reset integral
            
            Serial.print("STALL: Retry ");
            Serial.print(stall_retry_count);
            Serial.print("/");
            Serial.print(STALL_MAX_RETRIES);
            Serial.print(" - reducing velocity to ");
            Serial.print(velocity_limit_factor * 100.0f);
            Serial.println("%");
        }
    } else {
        // Near target - reset stall detection
        stall_reference_pos = enc_pos;
        stall_check_start_ms = millis();
    }
    
    // Check if we've reached the target
    if (abs(error) <= POSITION_TOLERANCE) {
        if (!position_reached) {
            stepper_stop();
            integral_error = 0.0f; // Reset integral when target reached
            position_reached = true;
        }
        last_error = error;
        return;
    }
    
    position_reached = false;
    
    // === Two-Stage PID: Select gains based on distance to target ===
    float kp, ki, kd;
    if (abs(error) > NEAR_THRESHOLD) {
        // FAR mode: aggressive approach
        kp = KP_FAR;
        ki = KI_FAR;
        kd = KD_FAR;
        // Reset integral when in far mode to prevent windup
        integral_error = 0.0f;
    } else {
        // NEAR mode: precise positioning
        kp = KP_NEAR;
        ki = KI_NEAR;
        kd = KD_NEAR;
    }
    
    // === PID Calculation ===
    
    // Proportional term
    float p_term = kp * error;
    
    // Integral term with anti-windup
    integral_error += error * dt;
    integral_error = constrain(integral_error, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);
    float i_term = ki * integral_error;
    
    // Derivative term (on error, with filtering)
    float d_term = 0.0f;
    if (dt > 0) {
        d_term = kd * (error - last_error) / dt;
    }
    last_error = error;
    
    // Calculate velocity command (in encoder counts per second)
    float velocity_cmd = p_term + i_term + d_term;
    
    // Convert to step velocity
    float step_velocity = velocity_cmd * STEPS_PER_ENC;
    
    // Apply velocity limits (with stall reduction factor)
    float current_max_velocity = MAX_VELOCITY * velocity_limit_factor;
    step_velocity = constrain(step_velocity, -current_max_velocity, current_max_velocity);
    
    // Apply minimum velocity threshold to prevent stalling
    if (fabsf(step_velocity) < MIN_VELOCITY && abs(error) > POSITION_TOLERANCE) {
        step_velocity = (error > 0) ? MIN_VELOCITY : -MIN_VELOCITY;
    }
    
    // Command the stepper
    stepper_set_velocity(step_velocity);
}

void loop() {
    handle_serial();
    stepper_poll();
    
    // Run high-frequency PID controller
    run_pid_controller();

    // Periodic debug output for serial plotter
    static uint32_t last_print = 0;
    static float last_v_cmd = 0.0f;
    uint32_t now = millis();
    
    if (now - last_print >= 20) { // 50 Hz output
        last_print = now;
        
        int32_t enc_pos = encoder_get();
        int32_t step_tgt = stepper_get_target();
        int32_t error = enc_setpoint - enc_pos;
        
        // Calculate current velocity command for display
        float v_cmd = (abs(error) > NEAR_THRESHOLD ? KP_FAR : KP_NEAR) * error * STEPS_PER_ENC;
        v_cmd = constrain(v_cmd, -MAX_VELOCITY, MAX_VELOCITY);
        last_v_cmd = v_cmd;
        
        // Format for Serial Plotter: setpoint, enc, step_target
        Serial.print("New:");
        Serial.print(enc_pos);
        Serial.print(",setpoint:");
        Serial.print(enc_setpoint);
        Serial.print(",enc=");
        Serial.print(enc_pos);
        Serial.print("  target=");
        Serial.print(enc_setpoint);
        Serial.print("  err=");
        Serial.print(error);
        Serial.print("  v_cmd=");
        Serial.println(v_cmd);
    }
}
