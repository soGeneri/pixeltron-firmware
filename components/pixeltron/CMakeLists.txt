# Pixeltron firmware logic: knob gestures, brightness, setup QR and status
# screens. The sources are plain C with no ESP-IDF dependencies, so the same
# files also build for the host (host/Makefile) for tests and the simulator.
idf_component_register(
    SRCS "src/gesture.c"
         "src/brightness.c"
         "src/fb.c"
         "src/screens.c"
         "src/codeword.c"
         "third_party/qrcodegen/qrcodegen.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src" "third_party/qrcodegen")
