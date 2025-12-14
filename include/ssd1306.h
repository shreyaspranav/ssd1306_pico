#pragma once
#include <pico/types.h>

// Global settings --------------------------------------

#define SSD1306_I2C_CLK                1000
#define SSD1306_I2C_ADDR               _u(0x3C)
#define SSD1306_I2C_WRITE_TIMEOUT_MS   100

// ------------------------------------------------------

typedef struct SSD1306 { void* data; } SSD1306;

typedef enum SSD1306_Size {
    SSD1306_128x64,
    SSD1306_128x32
} SSD1306_Size;

// Display functions:
SSD1306* init_display(SSD1306_Size size, uint scl_pin, uint sda_pin);

void clear(SSD1306* inst);

// Max 21 characters per line!
void write_string(SSD1306* inst, const char* str, uint8_t page, uint8_t char_offset);

void display_full_on(SSD1306* inst);
void display_resume_content(SSD1306* inst);

