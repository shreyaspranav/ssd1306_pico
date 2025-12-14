#include <stdio.h>
#include "pico/stdlib.h"

#include "ssd1306.h"

#include "hardware/adc.h"

#define DISP_SCL_PIN 17
#define DISP_SDA_PIN 16

int main()
{
    stdio_init_all();

    SSD1306* display = init_display(SSD1306_128x64, DISP_SCL_PIN, DISP_SDA_PIN);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    const float Vref = 3.3f;

    clear(display);
    while (true)
    {
        uint16_t raw = adc_read();  // 12-bit value (0–4095)
        float voltage = raw * Vref / 4096.0f;

        float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;

        char str[25];
        sprintf(str, "Chip Temp: %.3f *C", temperature);
        write_string(display, str, 0, 0);

        sleep_ms(300);
    }
}


