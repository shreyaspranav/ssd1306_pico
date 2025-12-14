#include "ssd1306.h"
#include "ssd1306_font.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"


typedef struct {
    uint8_t width, height;

    i2c_inst_t* disp_i2c_inst;

    uint8_t* frame_buf;
    uint frame_buf_size;
} _SSD1306;

// Commands ------------------------------------------------------------
// Refer https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf

// Fundamental Commands -------------------------------
#define SSD1306_SET_CONTRAST                _u(0x81)   // Set contrast level (next byte = 0–255)
#define SSD1306_DISPLAY_ALL_ON_RESUME       _u(0xA4)   // Resume RAM content display
#define SSD1306_DISPLAY_ALL_ON              _u(0xA5)   // Force all pixels ON
#define SSD1306_NORMAL_DISPLAY              _u(0xA6)   // RAM bit 0=OFF, 1=ON (normal)
#define SSD1306_INVERT_DISPLAY              _u(0xA7)   // RAM bit 0=ON, 1=OFF (inverse)
#define SSD1306_DISPLAY_OFF                 _u(0xAE)   // Set display OFF (sleep mode)
#define SSD1306_DISPLAY_ON                  _u(0xAF)   // Set display ON (normal operation)

// Scrolling Commands ---------------------------------
#define SSD1306_RIGHT_HORIZONTAL_SCROLL     _u(0x26)   // Start horizontal scroll to the right
#define SSD1306_LEFT_HORIZONTAL_SCROLL      _u(0x27)   // Start horizontal scroll to the left
#define SSD1306_VERTICAL_AND_RIGHT_SCROLL   _u(0x29)   // Vertical + right horizontal scroll
#define SSD1306_VERTICAL_AND_LEFT_SCROLL    _u(0x2A)   // Vertical + left horizontal scroll
#define SSD1306_DEACTIVATE_SCROLL           _u(0x2E)   // Stop scrolling
#define SSD1306_ACTIVATE_SCROLL             _u(0x2F)   // Start scrolling
#define SSD1306_SET_VERTICAL_SCROLL_AREA    _u(0xA3)   // Set top area + number of rows for vertical scroll

// Addressing Setting Commands ------------------------
#define SSD1306_SET_LOWER_COLUMN            _u(0x00)   // Low  nibble of column (0x00–0x0F)
#define SSD1306_SET_HIGHER_COLUMN           _u(0x10)   // High nibble of column (0x10–0x1F)
#define SSD1306_SET_MEMORY_MODE             _u(0x20)   // Set memory addressing mode (next byte)
#define SSD1306_COLUMN_ADDR                 _u(0x21)   // Set column start & end (2 bytes)
#define SSD1306_PAGE_ADDR                   _u(0x22)   // Set page   start & end (2 bytes)

// Hardware Configuration Commands --------------------
#define SSD1306_SET_START_LINE              _u(0x40)   // Set display start line (OR with 0–63)
#define SSD1306_SET_SEGMENT_REMAP_0         _u(0xA0)   // Column 0 -> SEG0
#define SSD1306_SET_SEGMENT_REMAP_127       _u(0xA1)   // Column 0 -> SEG127 (mirror horizontally)
#define SSD1306_SET_MULTIPLEX_RATIO         _u(0xA8)   // Set multiplex ratio (next byte = 0x1F–0x3F)
#define SSD1306_SET_COM_SCAN_DIR_NORMAL     _u(0xC0)   // Scan COM0 -> COM[N] (normal vertical order)
#define SSD1306_SET_COM_SCAN_DIR_REMAPPED   _u(0xC8)   // Scan COM[N] -> COM0 (vertical flip)
#define SSD1306_SET_DISPLAY_OFFSET          _u(0xD3)   // Vertical display shift (next byte)
#define SSD1306_SET_COM_PINS                _u(0xDA)   // COM pins configuration (next byte)

// Timing & Driving Scheme Commands -------------------
#define SSD1306_SET_CLOCK_DIV               _u(0xD5)   // Set display clock divide ratio (next byte)
#define SSD1306_SET_PRECHARGE               _u(0xD9)   // Pre-charge period (next byte)
#define SSD1306_SET_VCOM_DETECT             _u(0xDB)   // VCOMH deselect level (next byte)
#define SSD1306_NOP                         _u(0xE3)   // No operation

// Charge Pump ----------------------------------------
#define SSD1306_CHARGE_PUMP                 _u(0x8D)   // Enable/disable charge pump (next byte)

// Graphic acceleration (legacy compatibility) --------
#define SSD1306_SCROLL_SETUP                _u(0x26)   // (Alias of right horizontal scroll)

// Internal API --------------------------------------------------------

void _send_single_cmd(_SSD1306* display_inst, uint8_t cmd)
{
    // When sending a single command, the control byte is supposed to be 0x80
    // i.e, Co = 1, D/C = 0 and the rest of the bits 0
    // and the byte followed by the command is again a control byte
    
    uint8_t data[] = { 0x80, cmd };
    int status = i2c_write_timeout_us(display_inst->disp_i2c_inst, SSD1306_I2C_ADDR, data, 2, false, SSD1306_I2C_WRITE_TIMEOUT_MS * 1000);
}

void _send_cmd_list(_SSD1306* display_inst, uint8_t* cmd_list, size_t len)
{
    // When sending a multiple commands at once, the control byte is supposed to be 0x00
    // i.e, Co = 0, D/C = 0 and the rest of the bits 0

    uint8_t* temp_buf = malloc(len + 1);
    temp_buf[0] = 0x00;

    memcpy(temp_buf + 1, cmd_list, len);
    int status = i2c_write_timeout_us(display_inst->disp_i2c_inst, SSD1306_I2C_ADDR, temp_buf, len + 1, false, SSD1306_I2C_WRITE_TIMEOUT_MS * 1000);

    free(temp_buf);
}

void _send_buf_gddram(_SSD1306* display_inst, uint8_t* buf, size_t len)
{
    // When sending a buffer to GDDRAM, Co = 0, D/C = 1 as the data byte is to be stored in the GDDRAM

    uint8_t* temp_buf = malloc(len + 1);
    temp_buf[0] = 0x40;

    memcpy(temp_buf + 1, buf, len);
    int status = i2c_write_timeout_us(display_inst->disp_i2c_inst, SSD1306_I2C_ADDR, temp_buf, len + 1, false, SSD1306_I2C_WRITE_TIMEOUT_MS * 1000);

    free(temp_buf);
}

void _display_init(_SSD1306* display_inst)
{
    // Send the initialization sequence
    // Refer page 64

    uint8_t init_cmds[] = {
        SSD1306_SET_MULTIPLEX_RATIO, 
        0x3F,
        SSD1306_SET_DISPLAY_OFFSET, 
        0x00,
        SSD1306_SET_MEMORY_MODE,        // Wasn't present in the documentation, I added it. 
        0x00,                           // Horizontal addressing
        SSD1306_SET_START_LINE, 
        SSD1306_SET_SEGMENT_REMAP_0, 
        SSD1306_SET_COM_SCAN_DIR_NORMAL,
        SSD1306_SET_COM_PINS, 
        0x02,
        SSD1306_SET_CONTRAST, 
        0x7F,
        SSD1306_DISPLAY_ALL_ON_RESUME,
        SSD1306_NORMAL_DISPLAY,
        SSD1306_SET_CLOCK_DIV, 
        0x80,
        SSD1306_CHARGE_PUMP, 
        0x14,
        SSD1306_DISPLAY_ON,
    };

    if(display_inst->height == 64)
        init_cmds[10] = 0x12;

    _send_cmd_list(display_inst, init_cmds, count_of(init_cmds));
}

void _render_buf(_SSD1306* display_inst, uint8_t start_page, uint8_t end_page, uint8_t start_seg, uint8_t end_seg)
{
    uint8_t cmds[] = 
    {
        SSD1306_COLUMN_ADDR,
        start_seg, end_seg,
        SSD1306_PAGE_ADDR,
        start_page, end_page
    };

    _send_cmd_list(display_inst, cmds, count_of(cmds));
    _send_buf_gddram(display_inst, display_inst->frame_buf, display_inst->frame_buf_size);
}

// Public API ----------------------------------------------------------
SSD1306* init_display(SSD1306_Size size, uint scl_pin, uint sda_pin)
{
    _SSD1306* inst = malloc(sizeof(_SSD1306));
    inst->width = 128;

    switch (size)
    {
    case SSD1306_128x32:
        inst->height = 32;
        break;
    case SSD1306_128x64:
        inst->height = 64;
        break;
    }

    inst->frame_buf_size = (inst->width * inst->height) / 8;
    inst->frame_buf = malloc(inst->frame_buf_size);

    // Init the I2C peripheral + Set the clock
    inst->disp_i2c_inst = i2c0;
    i2c_init(inst->disp_i2c_inst, SSD1306_I2C_CLK * 1000);

    // Set the GPIO pins for I2C comminucation, pull them up
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_pull_up(scl_pin);
    gpio_pull_up(sda_pin);

    _display_init(inst);
    return (SSD1306*)inst;
}

void write_string(SSD1306* inst, const char* str, uint8_t page, uint8_t char_offset)
{
    _SSD1306* disp = (_SSD1306*)inst;
    size_t string_len = strlen(str);
    printf("String length: %d", string_len);

    for (int i = 0; i < string_len; i++)
    {
        size_t offset = (128 * page) + (6 * (char_offset + i));
        printf("Offset: %d", offset);
        memcpy(disp->frame_buf + offset, font[str[i] - ' '], 6);
    }
    _render_buf(disp, 0, (disp->height / 8) - 1, 0, disp->width - 1);
}

void clear(SSD1306* inst)
{
    _SSD1306* disp = (_SSD1306*)inst;
    memset(disp->frame_buf, 0, disp->frame_buf_size);
    _render_buf(disp, 0, (disp->height / 8) - 1, 0, disp->width - 1);
}

void display_full_on(SSD1306* inst)
{
    uint8_t cmd = SSD1306_DISPLAY_ALL_ON;
    _send_single_cmd((_SSD1306*)inst, cmd);
}

void display_resume_content(SSD1306* inst)
{
    uint8_t cmd = SSD1306_DISPLAY_ALL_ON_RESUME;
    _send_single_cmd((_SSD1306*)inst, cmd);
}