# Light and Deep Sleep Example

This sample demonstrates the use of the ESP32 light and deep sleep capabilities.
Automatic light sleep is enabled so the system automatically enters light sleep
when FreeRTOS isn't busy doing other things.

On startup a deep sleep timer is started. If this timer expires, GPIO 0 low and
a timer are configured as wakeup sources and deep sleep is entered

An ISR is attached to the falling edge of GPIO 0 which uses the FreeRTOS timer
task to perform deferred interrupt handling of printing a message and resetting
the deep sleep timer.

Each time `app_main` is run, the value of `s_boot_count` is incremented and
printed. This variable is tagged with the property `RTC_DATA_ATTR` which causes
the variable to be placed in a region that is retained during deep sleep. As a
result, this variable will increase each time the device comes out of deep
sleep, but will be reset to `0` if the ESP32 is reset.
