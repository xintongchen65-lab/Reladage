# RehabMotion 7-inch display firmware

PlatformIO project for the VIEWESMART 800×480 ESP32-S3 display.

- `src/main.cpp`: approved dashboard + calibration/training flow.
- `src/product_ui/`: product-page router and 20 product pages covering precheck, calibration, training, result, report, device and sync flows.
- `src/rehab_uart_link.cpp`: framed UART link with ACK/retry and live controller state.
- `platformio.ini`: portable dependency configuration based on the current screen project.

On the development machine the Arduino 3.1.1 library bundle can also be supplied as a
local `framework-arduinoespressif32-libs` package when operating fully offline.
