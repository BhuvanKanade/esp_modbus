/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "sdkconfig.h"


/*
    Function definination
*/
static void blink_task(void *arg);
void init(void);
static void rx_task(void *arg);
static void tx_task(void *arg);
int sendData(const char* logName, const char* data);




static const int RX_BUF_SIZE = 1024;

/* 
    Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_RATE CONFIG_BLINK_LED_RATE



void app_main(void)
{
    init();
    xTaskCreatePinnedToCore(blink_task, "blink_task", 1024*2, NULL, tskIDLE_PRIORITY, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL, 1);
    xTaskCreatePinnedToCore(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-20, NULL, tskNO_AFFINITY);
}


static void blink_task(void *arg)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    static const char *BLINK_TASK_TAG = "BLINKING_TASK";

    // esp_log_level_set(BLINK_TASK_TAG, ESP_LOG_INFO);
    ESP_LOGI(BLINK_TASK_TAG, "In before superloop");

    while(1) {
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(BLINK_RATE / portTICK_PERIOD_MS);

        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(BLINK_RATE / portTICK_PERIOD_MS);
        
        /* stak monitor */
        uint32_t stack_high_size_free = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGD(BLINK_TASK_TAG, "Stack high Water mark = %d ; running on core = %d", stack_high_size_free, xPortGetCoreID());
    }

    vTaskDelete( NULL );
}


static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    // esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);

            /* stak monitor */
            uint32_t stack_high_size_free = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGD(RX_TASK_TAG, "Stack high Water mark = %d ; running on core = %d", stack_high_size_free, xPortGetCoreID());
        }
    }
    free(data);
    vTaskDelete( NULL );
}


// TODO: work on communciation
int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_0, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    // esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData(TX_TASK_TAG, "Hello world \n");
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        /* stak monitor */
        uint32_t stack_high_size_free = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGD(TX_TASK_TAG, "Stack high Water mark = %d ; running on core = %d", stack_high_size_free, xPortGetCoreID());
    }

    vTaskDelete( NULL );
}


void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}