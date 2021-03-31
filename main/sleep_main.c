/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/gpio.h"


static const char* wakeup_cause_to_string(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "undefined";

        case ESP_SLEEP_WAKEUP_EXT0:
            return "ext0";

        case ESP_SLEEP_WAKEUP_EXT1:
            return "ext1";

        case ESP_SLEEP_WAKEUP_TIMER:
            return "timer";

        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "touchpad";

        case ESP_SLEEP_WAKEUP_ULP:
            return "ulp";

        case ESP_SLEEP_WAKEUP_GPIO:
            return "gpio";

        case ESP_SLEEP_WAKEUP_UART:
            return "uart";

        case ESP_SLEEP_WAKEUP_ALL:
        default:
            return "invalid";
    }
}

static void setup_power_management(void)
{
    const esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

static void setup_sleep(void)
{
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    // Enable wakeup when the button is pushed
    ESP_ERROR_CHECK(gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL));
}

void push_button_deferred_handler(void *p1, uint32_t p2)
{
    printf(
        "push button pressed - wakeup cause: %s\n",
        wakeup_cause_to_string(esp_sleep_get_wakeup_cause()));
}

void push_button_isr_handler(void *context)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    configASSERT(xTimerPendFunctionCallFromISR(
                     push_button_deferred_handler, NULL, 0, &higher_priority_task_woken) == pdPASS);
    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void setup_push_button(void)
{
    // Use the centralized GPIO service
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    const gpio_config_t button_config = {
        .pin_bit_mask = (1ull << GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_0, push_button_isr_handler, NULL));
}

void app_main(void)
{
    setup_power_management();
    setup_sleep();
    setup_push_button();
    while (true) {
        printf(
            "timer expired - level = %d, wakeup cause: %s\n",
            gpio_get_level(GPIO_NUM_0),
            wakeup_cause_to_string(esp_sleep_get_wakeup_cause()));
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
