/* Light and Deep Sleep Example

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


// Mark this variable to be placed in the RTC_DATA_ATTR which will cause its value to be retained
// even while in deep sleep.
static RTC_DATA_ATTR unsigned int s_boot_count = 0;

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

static void push_button_deferred_handler(void *p1, uint32_t p2)
{
    TimerHandle_t deep_sleep_timer = p1;
    xTimerReset(deep_sleep_timer, portMAX_DELAY);
    printf(
        "push button pressed - wakeup cause: %s\n",
        wakeup_cause_to_string(esp_sleep_get_wakeup_cause()));
}

static void push_button_isr_handler(void *context)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    configASSERT(xTimerPendFunctionCallFromISR(
        push_button_deferred_handler, context, 0, &higher_priority_task_woken) == pdPASS);
    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void setup_push_button(TimerHandle_t deep_sleep_timer)
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
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_0, push_button_isr_handler, deep_sleep_timer));
}

static void timer_handler(TimerHandle_t deep_sleep_timer)
{
    printf("Deep sleep timer expired - requesting deep sleep\n");

    // Disable any wakeup sources that were previously configured. In testing it was found that
    // leftover GPIO wakeup sources (which aren't compatible with deep sleep) cause a reboot when
    // deep sleep is requested.
    ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));

    // Wake up if GPIO0 goes low
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);

    // Wake up from deep sleep after 30s
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(30 * 1000 * 1000));

    esp_deep_sleep_start();
}

void app_main(void)
{
    s_boot_count++;
    printf(
        "Boot count: %u, wakeup cause: %s\n",
        s_boot_count,
        wakeup_cause_to_string(esp_sleep_get_wakeup_cause()));
    TimerHandle_t deep_sleep_timer =
        xTimerCreate("deep sleep", pdMS_TO_TICKS(20 * 1000), pdFALSE, NULL, timer_handler);
    setup_power_management();
    setup_sleep();
    setup_push_button(deep_sleep_timer);
    configASSERT(deep_sleep_timer != NULL);
    configASSERT(xTimerStart(deep_sleep_timer, portMAX_DELAY) == pdPASS);
    for (int i = 1; ; i++) {
        // The system will automatically enter light sleep during this delay
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        printf(
            "In 5s delay loop - count = %d, wakeup cause = %s\n",
            i,
            wakeup_cause_to_string(esp_sleep_get_wakeup_cause()));
    }
}
