/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "stdio.h"
#include "string.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "sdkconfig.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"



// #define ENABLE_UART_TX_RX

/*
    Function definination
*/
static void blink_task(void *arg);

#ifdef ENABLE_UART_TX_RX
void init(void);
static void rx_task(void *arg);
static void tx_task(void *arg);
#endif

int sendData(const char* logName, const char* data);
static void initialize_console(void);
static void initialize_nvs(void);
#if CONFIG_STORE_HISTORY
static void initialize_filesystem(void);
#endif



static const char* CONSOL_TASK_TAG = "CONSOL_TASK_TAG";
#define PROMPT_STR CONFIG_IDF_TARGET

#ifdef ENABLE_UART_TX_RX
static const int RX_BUF_SIZE = 1024;
#endif

/* 
    Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_RATE CONFIG_BLINK_LED_RATE


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


#ifdef ENABLE_UART_TX_RX

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
#endif


// TODO: work on communciation
int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_0, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}


/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CONSOL_TASK_TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY



#ifdef ENABLE_RX
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
#endif


static void initialize_console(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_REF_TICK,
    };
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );
    ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

    /* Don't return empty lines */
    linenoiseAllowEmpty(false);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif
}

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}



void app_main(void)
{
    initialize_nvs();

#if CONFIG_STORE_HISTORY
    initialize_filesystem();
    ESP_LOGI(CONSOL_TASK_TAG, "Command history enabled");
#else
    ESP_LOGI(CONSOL_TASK_TAG, "Command history disabled");
#endif

    initialize_console();

    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_nvs();

    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    const char* prompt = LOG_COLOR_I PROMPT_STR "> " LOG_RESET_COLOR;

    printf("\n"
           "This is an example of ESP-IDF console component.\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Press Enter or Ctrl+C will terminate the console environment.\n");


    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    if (probe_status) { /* zero indicates success */
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = PROMPT_STR "> ";
#endif //CONFIG_LOG_COLORS
    }


#ifdef ENABLE_UART_TX_RX
    init();    
    xTaskCreatePinnedToCore(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL, 1);
    xTaskCreatePinnedToCore(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-20, NULL, tskNO_AFFINITY);
#endif

    xTaskCreatePinnedToCore(blink_task, "blink_task", 1024*2, NULL, tskIDLE_PRIORITY, NULL, tskNO_AFFINITY);

    
    /* Main loop */
    while(true) {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char* line = linenoise(prompt);
        if (line == NULL) { /* Break on EOF or error */
            break;
        }
        /* Add the command to the history if not empty*/
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
#if CONFIG_STORE_HISTORY
            /* Save command history to filesystem */
            linenoiseHistorySave(HISTORY_PATH);
#endif
        }

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }

    ESP_LOGE(CONSOL_TASK_TAG, "Error or end-of-input, terminating console");
    esp_console_deinit();

}

