add_library(ssd1306_pico INTERFACE)

target_include_directories(ssd1306_pico INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(ssd1306_pico INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ssd1306.c
)

target_link_libraries(ssd1306_pico INTERFACE
    hardware_i2c
    pico_stdlib
)