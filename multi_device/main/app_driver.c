/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>

#include <app_reset.h>
#include <ws2812_led.h>
#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO         CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0
/* This is the GPIOs on which the power will be set */
#define OUTPUT_GPIO         CONFIG_EXAMPLE_OUTPUT_GPIO
#define LED_GPIO            CONFIG_LED_OUTPUT_GPIO
#define FAN_GPIO            0

/* These values correspoind to H,S,V = 120,100,10 */
#define DEFAULT_RED     0
#define DEFAULT_GREEN   25
#define DEFAULT_BLUE    0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static bool     g_power_state = DEFAULT_SWITCH_POWER;
static bool     g_led_state = DEFAULT_SWITCH_POWER;
static bool     g_fan_state = DEFAULT_SWITCH_POWER;
static float    g_temperature = DEFAULT_TEMPERATURE;
static TimerHandle_t sensor_timer;

static void app_sensor_update(TimerHandle_t handle)
{
    static float delta = 0.5;
    g_temperature += delta;
    if (g_temperature > 99) {
        delta = -0.5;
    } else if (g_temperature < 1) {
        delta = 0.5;
    }
    esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(temp_sensor_device, ESP_RMAKER_PARAM_TEMPERATURE),
                esp_rmaker_float(g_temperature));
}

float app_get_current_temperature()
{
    return g_temperature;
}

esp_err_t app_sensor_init(void)
{
    g_temperature = DEFAULT_TEMPERATURE;
    sensor_timer = xTimerCreate("app_sensor_update_tm", (REPORTING_PERIOD * 1000) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, app_sensor_update);
    if (sensor_timer) {
        xTimerStart(sensor_timer, 0);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void app_indicator_set(bool state)
{
    if (state) {
        ws2812_led_set_rgb(DEFAULT_RED, DEFAULT_GREEN, DEFAULT_BLUE);
    } else {
        ws2812_led_clear();
    }
}

// Initialize indicators GUI
static void app_indicator_init(void)
{
    ws2812_led_init();
    app_indicator_set(g_power_state);
    app_indicator_set(g_led_state);
    app_indicator_set(g_fan_state);
}

// Control toggle buttons
static void push_btn_cb(void *arg)
{
    bool new_state = !g_power_state;
    app_driver_set_state(new_state);
    esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(switch_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(new_state));
}
static void push_btn_led(void *arg)
{
    bool new_state = !g_led_state;
    led_driver_set_state(new_state);
    esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(led_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(new_state));
}
static void push_btn_fan(void *arg)
{
    bool new_state = !g_fan_state;
    fan_driver_set_state(new_state);
    esp_rmaker_param_update_and_report(
        esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
        esp_rmaker_bool(new_state));
}
// GUIO power state
static void set_power_state(bool target)
{
    gpio_set_level(OUTPUT_GPIO, target);
    app_indicator_set(target);
}
static void set_led_state(bool target)
{
    gpio_set_level(LED_GPIO, target);
    app_indicator_set(target);
}
static void set_fan_state(bool target)
{
    gpio_set_level(FAN_GPIO, target);
    app_indicator_set(target);
}

void app_driver_init()
{
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_led, NULL);
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_fan, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    /* Configure GUIO pins */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    uint64_t pin_mask = (((uint64_t)1 << OUTPUT_GPIO) | ((uint64_t)1 << LED_GPIO));
    //io_conf.pin_bit_mask = ((uint64_t)1 << OUTPUT_GPIO);
    //io_conf.pin_bit_mask = ((uint64_t)1 << LED_GPIO);
    io_conf.pin_bit_mask = pin_mask;
    /* Configure the GPIO */
    gpio_config(&io_conf);
    
    app_indicator_init();
    app_sensor_init();
}

int IRAM_ATTR app_driver_set_state(bool state)
{
    if(g_power_state != state) {
        g_power_state = state;
        set_power_state(g_power_state);
    }
    return ESP_OK;
}
int IRAM_ATTR led_driver_set_state(bool state)
{
    if(g_led_state != state) {
        g_led_state = state;
        set_led_state(g_led_state);
    }
    return ESP_OK;
}
int IRAM_ATTR fan_driver_set_state(bool state)
{
    if(g_fan_state != state) {
        g_fan_state = state;
        set_fan_state(g_fan_state);
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_power_state;
}
bool led_driver_get_state(void)
{
    return g_led_state;
}
