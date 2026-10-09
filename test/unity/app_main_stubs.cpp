// Stub app_main for ESP-IDF framework when test_build_src = false
// This prevents linker errors when the main application is not compiled

#ifdef UNIT_TEST
extern "C" void app_main(void) {
    // Stub - no app main needed for unit tests
}
#endif
