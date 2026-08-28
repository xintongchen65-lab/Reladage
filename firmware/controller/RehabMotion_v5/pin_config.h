#pragma once

// =====================================================
// RehabMotion integrated main-controller pin configuration
// 7-inch VIEWE display is now the ONLY visual UI.
// The old ILI9488 runtime is disabled; GPIO8/9 are reused for UART.
// =====================================================

// =============================
// 7-inch screen UART link
// Main TX -> screen RX, Main RX <- screen TX
// =============================
#define PIN_SCREEN_TX   8
#define PIN_SCREEN_RX   9

// =============================
// Offline voice-board UART link (UART2)
// Main TX -> voice GPIO44 (RX), Main RX <- voice GPIO43 (TX)
// GPIO10/11 cannot be used here: they are legacy TFT CS and SD MOSI.
// =============================
#define PIN_VOICE_TX   17
#define PIN_VOICE_RX   18

// Legacy ILI9488 CS is kept HIGH only because the mature SD logger still
// contains shared-SPI safety code. RESET/DC are no longer driven at runtime.
#define PIN_TFT_CS     10

// =============================
// Micro SD Storage Board (main controller)
// =============================
#define PIN_SPI_MOSI   11
#define PIN_SPI_SCK    12
#define PIN_SPI_MISO   13
#define PIN_SD_CS      16

// =============================
// Active buzzer (HIGH trigger)
// =============================
#define PIN_BUZZER     21
