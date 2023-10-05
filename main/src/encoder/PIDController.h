class PIDController {
public:
    float kp, ki, kd;
    float integral, previous_error;

    PIDController(float kp, float ki, float kd) : kp(kp), ki(ki), kd(kd), integral(0), previous_error(0) {}

    float compute(float setpoint, float actual_position) {
        float error = setpoint - actual_position;
        integral = integral + error;
        float derivative = error - previous_error;

        float output = kp * error + ki * integral + kd * derivative;
        previous_error = error;
        return output;
    }
};
