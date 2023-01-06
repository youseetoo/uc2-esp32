#include <string>

// This will switch between configs at runtime (e.g. pindef_UC2_2.h)
class pindefConfigurator {
  public:
    int PIN_DEF_MOTOR_DIR_A;
    int PIN_DEF_MOTOR_DIR_X; 
    int PIN_DEF_MOTOR_DIR_Y;
    int PIN_DEF_MOTOR_DIR_Z;
    int PIN_DEF_MOTOR_STP_A;
    int PIN_DEF_MOTOR_STP_X;
    int PIN_DEF_MOTOR_STP_Y;
    int PIN_DEF_MOTOR_STP_Z;
    int PIN_DEF_MOTOR_EN_A;
    int PIN_DEF_MOTOR_EN_X;
    int PIN_DEF_MOTOR_EN_Y;
    int PIN_DEF_MOTOR_EN_Z;
    bool PIN_DEF_MOTOR_EN_A_INVERTED;
    bool PIN_DEF_MOTOR_EN_X_INVERTED;
    bool PIN_DEF_MOTOR_EN_Y_INVERTED;
    bool PIN_DEF_MOTOR_EN_Z_INVERTED;

    int PIN_DEF_LASER_1;
    int PIN_DEF_LASER_2;
    int PIN_DEF_LASER_3;

    int PIN_DEF_LED;
    int PIN_DEF_LED_NUM;

    int PIN_DEF_END_X;
    int PIN_DEF_END_Y;
    int PIN_DEF_END_Z; 
    
    //string PIN_PS4_MAC_DEF;
    int PIN_PS4_ENUM_DEF;

};