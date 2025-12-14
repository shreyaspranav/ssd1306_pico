# ssd1306_pico
Simple CMake Interface library for common SSD1306 128x64 and 128x32 displays 

# Usage

You need to clone the project inside a seperate folder in your project. In this case, I have cloned inside the `lib/` folder inside my project.

```text
project-root/
├── CMakeLists.txt
├── src/
│   └── main.c
└── lib/
    └── ssd1306_pico/  <- Clone the repo here
        └── CMakeLists.txt
        └── .. rest of the project ..
```

Add the following to the root `CMakeLists.txt` (The one on the root of your project)

```cmake
...
pico_sdk_init()

# Include the ssd1306 library
add_subdirectory(deps/ssd1306_pico)

add_executable(display_test 
    "src/main.c"
)

target_link_libraries(display_test PUBLIC
    ssd1306_pico    # <- Add this
    pico_stdlib
)
```

# Example
There is one file `main.cpp` inside the examples folder. It reads the temperature from the inbuilt temperature sensor in the RP2040 chip and displays it on the SSD1306 screen. Copy the file to your project's own `main.cpp` and build it.