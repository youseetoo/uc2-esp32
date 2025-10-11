# LinearEncoderController Improvements and Technical Debt

## Overview
This issue tracks various improvements and technical debt items identified in the `LinearEncoderController.cpp` module. These TODOs range from architectural improvements to bug fixes and performance optimizations.

## 🏗️ Architecture & Design Issues

### 1. Encoder Interface Unification
**Priority:** High  
**File:** `LinearEncoderController.cpp:29`  
**Description:** Move interrupt-based encoder handling to a separate class and unify the interface with PCNTEncoderController
- Current code mixes different encoder interfaces in one class
- Need better separation of concerns
- Would improve maintainability and testing

### 2. Non-blocking Operation Implementation
**Priority:** High  
**File:** `LinearEncoderController.cpp:563`  
**Description:** Replace blocking operations with thread/task-based implementation
- Current precision motion and homing operations are blocking
- Should use FreeRTOS tasks for better system responsiveness
- Would prevent watchdog timer issues

## 🎯 Motion Control Improvements

### 3. Dynamic Motion Timeout Calculation
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:465`  
**Description:** Calculate motion timeout based on distance and maximum speed
- Current 10-second timeout is hardcoded
- Should be computed: `timeout = (distance / max_speed) + safety_margin`
- Would prevent premature timeouts on long moves

### 4. Two-Stage Precision Motion
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:525`  
**Description:** Implement two-stage motion for higher accuracy
- Fast approach to near target position
- Slow final positioning for precision
- Would improve final positioning accuracy

### 5. Stuck Motor Recovery Strategy
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:508`  
**Description:** Implement intelligent stuck motor recovery
- Detect when motor is stuck
- Try reduced speed approach
- Implement retry logic with different parameters
- Abort if multiple attempts fail

## 🔧 Configuration & Persistence

### 6. Motor Direction Persistent Storage
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:207`  
**Description:** Store motor direction settings permanently in preferences
- Current motor direction settings are not persisted
- Should save to flash memory for consistency across reboots

### 7. Axis-Specific Operation Limitation
**Priority:** Low  
**File:** `LinearEncoderController.cpp:167`  
**Description:** Restrict operations to specific axes (e.g., X-axis only)
- Current code processes all axes
- May want to focus on single axis for stability
- Configurable axis selection

## 🐛 Bug Fixes

### 8. Homing State Reset Issue
**Priority:** High  
**File:** `LinearEncoderController.cpp:564`  
**Description:** Fix homing state issue preventing second homing run
- First homing works correctly
- Second homing attempt fails (no motion despite correct speed)
- Need to identify and reset the problematic state

### 9. Motor State Flags Cleanup
**Priority:** Low  
**File:** `LinearEncoderController.cpp:558-559`  
**Description:** Review necessity of `stopped` and `isStop` flags
- Unclear if these flags are necessary in current implementation
- May be legacy code that can be removed

### 10. Incomplete TODO Comment
**Priority:** Low  
**File:** `LinearEncoderController.cpp:622`  
**Description:** Complete or remove incomplete TODO comment
- "TOOD: after one run I cannot" - incomplete thought
- Need to investigate what the issue was and document properly

## 🔍 Data & Positioning

### 11. PCNT Value Integration
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:162`  
**Description:** Retrieve PCNT value for better motion starting point
- Should read current PCNT counter value before starting motion
- Would provide more accurate initial position reference

### 12. Unit Consistency Verification
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:187`  
**Description:** Verify unit consistency in relative positioning
- Check if relative distance calculation uses correct units
- Ensure encoder units match motor step units

### 13. Direction Error Detection
**Priority:** Medium  
**File:** `LinearEncoderController.cpp:494`  
**Description:** Implement motor/encoder direction mismatch detection
- Detect when motor and encoder directions are opposite
- Stop motor or reverse direction automatically
- Prevent runaway motor conditions

## 📈 Performance Optimizations

### 14. Sampling Frequency Optimization
**Priority:** Low  
**File:** `LinearEncoderController.cpp:562`  
**Description:** Review if averaging is still necessary with faster sampling
- High-frequency sampling may eliminate need for rolling averages
- Could simplify code and improve response time

## Implementation Priority

1. **High Priority:**
   - Homing state reset issue (#8)
   - Architecture unification (#1)
   - Non-blocking operations (#2)

2. **Medium Priority:**
   - Dynamic timeout calculation (#3)
   - Two-stage precision motion (#4)
   - Stuck motor recovery (#5)
   - PCNT integration (#11)

3. **Low Priority:**
   - Code cleanup items (#9, #10, #14)
   - Configuration improvements (#6, #7)

## Testing Requirements

Each improvement should include:
- Unit tests for new functionality
- Integration tests with hardware
- Performance regression testing
- Documentation updates

## Related Files

- `LinearEncoderController.h`
- `PCNTEncoderController.cpp`
- `FocusMotor.cpp`
- `MotorEncoderConfig.cpp`
- `BtController.cpp` (for PS4 trackpad integration)

## Future Enhancements

### PS4 Trackpad Integration
- **Touch-based positioning**: Use PS4 controller trackpad for intuitive X/Y positioning
- **Swipe gestures**: Left/right swipes for motor movement, up/down for focus control
- **Multi-touch support**: Two-finger gestures for zoom/rotation control
- **Implementation**: See `PS4_TRACKPAD_README.md` for detailed implementation guide

---

**Labels:** `enhancement`, `technical-debt`, `motion-control`, `encoder`, `ps4-controller`  
**Milestone:** v2.1  
**Estimated Effort:** 2-3 sprint cycles
