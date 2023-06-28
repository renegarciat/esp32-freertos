/* MOTOR CONTROL CODE */
////// IGNORE BY NOW ///////
//DAC GPIO25
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls.h"
#include "esp_system.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "freertos/timers.h"
#include "driver/dac_oneshot.h"
#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";
uint32_t firebase_buffer[2] = {0};

// For LED!////////
#define BLINK_GPIO 2
int CONFIG_BLINK_PERIOD = 250;

////FOR RELAYS////
#define FORWARD 33
#define STOP 26
#define REVERSE 13
int d = 0;

////FOR SPEED SENSOR////
#define CONFIG_LED_PIN 2
#define ESP_INTR_FLAG_DEFAULT 0
#define CONFIG_BUTTON_PIN 15
#define ROTARY_ENCODER_PIN 15
#define TIMER_PERIOD_MS 5000


TaskHandle_t ISR = NULL;
TaskHandle_t led_TaskHandle = NULL;
static volatile int pulseCount = 0;
TimerHandle_t xTimers;
int timerId = 1;
float speed = 0;
time_t seconds;

////FOR DAC////////
// #define EXAMPLE_DAC_CHAN0_ADC_CHAN ADC_CHANNEL_8 // GPIO25, same as DAC channel 0
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data)
            {
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len)
                {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            }
            else
            {
                const int buffer_len = esp_http_client_get_content_length(evt->client);
                if (output_buffer == NULL)
                {
                    output_buffer = (char *)malloc(buffer_len);
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (buffer_len - output_len));
                if (copy_len)
                {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

static void http_rest_with_url(uint32_t *v_d)
{
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {};
    esp_http_client_config_t config = {
        .url = "https://esp32motorcontrol-95da1-default-rtdb.firebaseio.com/Mediciones/Velocidad.json",
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer, // Pass address of local buffer to get response
        .disable_auto_redirect = true};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (v_d == NULL) { // IT IS A PATCH REQUEST!
        // FIRST PATCH
        char value[50] = {};
        char timestamp[50] = {"{\""};
        seconds = time(NULL);
        char temp[80] = {0};
        sprintf(temp, "%lld", seconds);
        strcat(timestamp, temp);
        strcat(timestamp, "\":");
        snprintf(value, 50, "%.2f", speed);
        strcat(timestamp, value);
        strcat(timestamp, "}");
        //printf("RESULTING STRING:%s\n", timestamp);
        const char *post_data2 = timestamp;
        esp_http_client_set_post_field(client, post_data2, strlen(post_data2));
        esp_http_client_set_method(client, HTTP_METHOD_PATCH);
        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK) ESP_LOGE(TAG, "HTTP PATCH request failed: %s", esp_err_to_name(err));
        //SECOND PATCH
        char data[50]= "{\"Velocidad_actual\":";
        strcat(data, value);
        strcat(data,"}");
        const char *post_data3 = data;
        esp_http_client_set_url(client, "https://esp32motorcontrol-95da1-default-rtdb.firebaseio.com/Mediciones.json");
        esp_http_client_set_post_field(client, post_data3, strlen(post_data3));
        err = esp_http_client_perform(client);
        //printf("%s\n",local_response_buffer);
    }
    else
    {
        // GET
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        esp_http_client_set_url(client, "https://esp32motorcontrol-95da1-default-rtdb.firebaseio.com/Comandos/Direccion.json");
        esp_err_t err = esp_http_client_perform(client);
        v_d[0] = atoi(local_response_buffer);
        esp_http_client_set_url(client, "https://esp32motorcontrol-95da1-default-rtdb.firebaseio.com/Comandos/Velocidad.json");
        esp_http_client_perform(client);
        v_d[1] = atoi(local_response_buffer);
        if (err != ESP_OK) ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void vTimerCallback(TimerHandle_t pxTimer)
{
    float pulses = pulseCount;
    speed = 60.0 * (pulses / 7.0); // Calculate speed in turns per second
    //printf("Speed: %.2f rpm\n", speed);
    pulseCount = 0; // Reset pulse count for the next interval
}

esp_err_t set_timer(void)
{
    printf("Timer init config.\n");
    xTimers = xTimerCreate("Speedimer",                      // Just a text name, not used by the kernel.
                           (pdMS_TO_TICKS(TIMER_PERIOD_MS)), // The timer period in ticks.
                           pdTRUE,                           // The timers will auto-reload themselves when they expire.
                           (void *)timerId,                  // Assign each timer a unique id equal to its array index.
                           vTimerCallback                    // Each timer calls the same callback when it expires.
    );

    if (xTimers == NULL)
    {
        printf("The timer was not created.\n");
    }
    else
    {
        if (xTimerStart(xTimers, 0) != pdPASS)
        {
            printf("The timer could not be set into the Active state.\n"); //
        }
    }

    return ESP_OK;
}

// interrupt service routine, called when the button is pressed
void IRAM_ATTR button_isr_handler(void *arg)
{
    xTaskResumeFromISR(ISR);
    // portYIELD_FROM_ISR(  );
}

void interruptInit(void)
{
    gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
    // enable interrupt on falling (1->0) edge for button pin
    gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_POSEDGE);
    // Install the driver’s GPIO ISR handler service, which allows per-pin GPIO interrupt handlers.
    //  install ISR service with default configuration
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // attach the interrupt service routine
    gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);
    return;
}

/// TASKS DEFINITION////
static void http_get_task(void *pvParameters)
{
    while (1)
    {
        http_rest_with_url(firebase_buffer);
        printf("command direction is = %ld\n", firebase_buffer[0]);
        printf("speed desired is = %ld\n", firebase_buffer[1]);
        ESP_LOGI(TAG, "GET Done!\n");
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void http_patch_task(void *pvParameters)
{
    while (1)
    {
        http_rest_with_url(NULL);
        ESP_LOGI(TAG, "Patch Done!\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void blink_led(void *pvParameters)
{
    int s_led_state = 0;
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    for (;;)
    {
        s_led_state = !s_led_state;
        gpio_set_level(BLINK_GPIO, s_led_state);
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

static void direction(void *pvParameters){
    gpio_set_direction(STOP, GPIO_MODE_OUTPUT);
    gpio_set_direction(FORWARD, GPIO_MODE_OUTPUT);
    gpio_set_direction(REVERSE, GPIO_MODE_OUTPUT);
    gpio_set_level(STOP, 1);
    gpio_set_level(FORWARD, 1);
    gpio_set_level(REVERSE, 1);

    int old_d = 0;
    while (1)
    {
        int d_loc = firebase_buffer[0];
        if (d_loc != old_d)
        {
            old_d = d_loc;
            switch (d_loc)
            {
            case 2:
                gpio_set_level(STOP, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                gpio_set_level(STOP, 1);
                vTaskDelay(pdMS_TO_TICKS(250));
                gpio_set_level(REVERSE, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
            case 0:
                gpio_set_level(STOP, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
            case 1:
                gpio_set_level(STOP, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                gpio_set_level(STOP, 1);
                vTaskDelay(pdMS_TO_TICKS(250));
                gpio_set_level(FORWARD, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
            default:
                break;
            }
            gpio_set_level(STOP, 1);
            gpio_set_level(FORWARD, 1);
            gpio_set_level(REVERSE, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// task that will react to button clicks
void button_task(void *arg)
{
    while (1)
    {
        vTaskSuspend(NULL);
        pulseCount++;
    }
}

static void dac_output_task(void *args)
{
    /* DAC oneshot init */
    dac_oneshot_handle_t chan0_handle;
    dac_oneshot_config_t chan0_cfg = {
        .chan_id = DAC_CHAN_0,
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&chan0_cfg, &chan0_handle));
    while (1)
    {
        /* Set the voltage every 1000 ms */
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(chan0_handle, firebase_buffer[1]));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/////END OF TASKS DEFINITION///////////

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    interruptInit();
    xTaskCreate(&blink_led, "Wifi Led", 1024, NULL, 3, &led_TaskHandle);
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    CONFIG_BLINK_PERIOD = 2000;

    ESP_LOGI(TAG, "Connected to AP\n");
    xTaskCreate(&http_get_task, "http_get_task", 8192, NULL, 5, NULL);
    xTaskCreate(&http_patch_task, "http_patch_task", 8192, NULL, 10, NULL);
    xTaskCreate(button_task, "button_task", 4096, NULL, 25, &ISR);
    xTaskCreate(&direction, "direction", 2048, NULL, 5, NULL);
    xTaskCreate(dac_output_task, "dac_chan0_output_task", 4096, NULL, 7, NULL);
    set_timer();
}
