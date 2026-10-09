// Recreated from git history (commit 665a2d6^): this header was removed but
// main/src/motor/FocusMotor.cpp still includes it via "#include \"../../config.h\"".
// It only defines a handful of legacy macros; the board-specific config lives in
// main/PinConfig.h. Kept so the host Unity test build (and any env compiling
// FocusMotor.cpp) resolves the include.
#define BAUDRATE 115200

#define boolToChar(b) ((b)?"true":"false")

//#define IS_PS3
#define IS_PS4
//#define DEBUG_GAMEPAD
//#define DEBUG_MOTOR

#define DEBUG_MAIN
