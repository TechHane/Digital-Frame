#pragma once

// ESP32-WROOM + ILI9341 SPI. Touch input is not used.
constexpr int TFT_CS_PIN = 5;
constexpr int TFT_DC_PIN = 2;
constexpr int TFT_RST_PIN = -1;
constexpr int TFT_SCK_PIN = 18;
constexpr int TFT_MOSI_PIN = 23;
constexpr int TFT_MISO_PIN = -1;
constexpr uint32_t TFT_SPEED = 10000000;
constexpr int PHOTO_WIDTH = 240;
constexpr int PHOTO_HEIGHT = 240;
constexpr int HEADER_HEIGHT = 18;
constexpr int PHOTO_TOP = HEADER_HEIGHT + (320 - HEADER_HEIGHT - PHOTO_HEIGHT) / 2;
constexpr int SD_SCK_PIN = 25;
constexpr int SD_MOSI_PIN = 26;
constexpr int SD_MISO_PIN = 32;
constexpr int SD_CS_PIN = 33;
constexpr int INTERNAL_PHOTO_LIMIT = 5;
constexpr int SD_PHOTO_LIMIT = 65535;
constexpr int ALBUM_PAGE_SIZE = 12;
constexpr size_t PHOTO_BYTES = PHOTO_WIDTH * PHOTO_HEIGHT * 2;
constexpr int MAX_SOURCE_EDGE = 640;
constexpr size_t MAX_SOURCE_PIXELS = 120000;
constexpr size_t MAX_PHOTO_BYTES = 8 + MAX_SOURCE_PIXELS * 2;
constexpr char AP_NAME[] = "TechHane_Cerceve";
constexpr char AP_PASSWORD[] = "Techhane123";
constexpr char VERSION[] = "0.3.4";
