class PIDController {
public:
    float kp, ki, kd;
    float integral, previous_error;
    float output_min, output_max;
    float integral_min, integral_max;
    unsigned long last_time;
    bool first_run;

    PIDController(float kp, float ki, float kd) : 
        kp(kp), ki(ki), kd(kd), 
        integral(0), previous_error(0),
        output_min(-10000), output_max(10000),
        integral_min(-1000), integral_max(1000),
        last_time(0), first_run(true) {}

    void setOutputLimits(float min_output, float max_output) {
        output_min = min_output;
        output_max = max_output;
        // Also set integral limits to prevent windup
        integral_min = min_output / (ki != 0 ? ki : 1);
        integral_max = max_output / (ki != 0 ? ki : 1);
    }

    void reset() {
        integral = 0;
        previous_error = 0;
        first_run = true;
    }

    float compute(float setpoint, float actual_position) {
        unsigned long now = millis();
        float error = setpoint - actual_position;
        
        // Skip first run to avoid derivative kick
        if (first_run) {
            previous_error = error;
            last_time = now;
            first_run = false;
            return 0;
        }
        
        // Calculate time delta for proper derivative calculation
        float dt = (now - last_time) / 1000.0; // Convert to seconds
        if (dt <= 0) dt = 0.001; // Prevent division by zero
        
        // Proportional term
        float p_term = kp * error;
        
        // Integral term with windup protection
        integral += error * dt;
        if (integral > integral_max) integral = integral_max;
        else if (integral < integral_min) integral = integral_min;
        float i_term = ki * integral;
        
        // Derivative term (derivative on measurement to avoid derivative kick)
        float derivative = (error - previous_error) / dt;
        float d_term = kd * derivative;
        
        // Calculate output
        float output = p_term + i_term + d_term;
        
        // Apply output limits
        if (output > output_max) output = output_max;
        else if (output < output_min) output = output_min;
        
        // Store for next iteration
        previous_error = error;
        last_time = now;
        
        return output;
    }
};
