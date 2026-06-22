// TFT_eSPI Pin Configuration for FireBeetle 2 ESP32-E with ZJY-PS130-V2.0 Display

#define ST7789_DRIVER

// Data bus bits
#define TFT_MOSI 16  // SDA pin (GPIO 16 / D11)
#define TFT_SCLK 12  // SCL pin (GPIO 12 / D13)

// Control pins
#define TFT_DC   2   // Data/Command (GPIO 2 / D9)
#define TFT_RST  13  // Reset (GPIO 13 / D7)
// No CS pin used - display communicates via SDA/SCL only

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Rotation
#define TFT_ROTATION 1

// SPI clock speed
#define SPI_FREQUENCY  40000000  // 40 MHz

// Don't use software SPI
#define USE_HSPI_PORT
