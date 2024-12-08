// st7789.hpp
#pragma once

#include <string>
#include <cstdint>
#include <gpiod.hpp>

class ST7789 {
public:
    // Display dimensions
    static constexpr int WIDTH = 240;
    static constexpr int HEIGHT = 280;

    // Basic color definitions (16-bit RGB565)
    enum Colors : uint16_t {
        BLACK = 0x0000,
        WHITE = 0xFFFF,
        RED   = 0xF800,
        GREEN = 0x07E0,
        BLUE  = 0x001F
    };
    // Font dimensions
    static constexpr int16_t font_width=8;
    static constexpr int16_t font_height=16;

    struct Config {
        std::string spiDevice;   // e.g., "/dev/spidev0.0"
        unsigned int speedHz;    // SPI speed in Hz
        std::string gpioChip;    // e.g., "gpiochip0"
        unsigned int dcPin;      // Data/Command pin
        unsigned int resetPin;   // Reset pin
    };

    explicit ST7789(const Config& config);
    ~ST7789();

    // Prevent copying
    ST7789(const ST7789&) = delete;
    ST7789& operator=(const ST7789&) = delete;

    void clearScreen(uint16_t color = BLACK);
    void showLogo();
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg);
    void drawString(int16_t x, int16_t y, const std::string& str, uint16_t color, uint16_t bg);

private:
    void reset();
    void display_init();
    void writeReg(uint8_t cmd);
    void writeDataByte(uint8_t data);
    void writeDataWord(uint16_t data);
    void spiWrite8(uint8_t data);
    void spiWrite16(uint16_t data);
    void selectArea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    Config config;
    gpiod::chip chip;
    gpiod::line dcLine;
    gpiod::line resetLine;
    int spifd;
    static const uint8_t font_bitmap[] ;
    static std::vector<uint16_t> image_data ;
};
