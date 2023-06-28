/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/dac_oneshot.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"

#define EXAMPLE_DAC_CHAN0_ADC_CHAN          ADC_CHANNEL_8   // GPIO25, same as DAC channel 0
#define EXAMPLE_ADC_WIDTH                   ADC_WIDTH_BIT_12
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_11

static void adc_monitor_task(void *args)
{
    adc_oneshot_unit_handle_t adc2_handle = (adc_oneshot_unit_handle_t) args;
    int chan0_val = 0;
    while (1) {
        /* Read the DAC output voltage */
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, EXAMPLE_DAC_CHAN0_ADC_CHAN, &chan0_val));
        printf("DAC channel 0 value: %4d\n", chan0_val);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void dac_output_task(void *args)
{
    dac_oneshot_handle_t handle = (dac_oneshot_handle_t)args;
    uint32_t val = 0;
    for(;;) {
        /* Set the voltage every 100 ms */
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(handle, val));
        val += 10;
        val %= 250;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    /* DAC oneshot init */
    dac_oneshot_handle_t chan0_handle;
    dac_oneshot_config_t chan0_cfg = {
        .chan_id = DAC_CHAN_0,
    };
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&chan0_cfg, &chan0_handle));

    /* DAC oneshot outputting threads */
    xTaskCreate(dac_output_task, "dac_chan0_output_task", 4096, chan0_handle, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(500)); // To differential the output of two channels

    /* ADC init, these channels are connected to the DAC channels internally */
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = false,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc2_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, EXAMPLE_DAC_CHAN0_ADC_CHAN, &chan_cfg));
    /* ADC monitor thread */
    xTaskCreate(adc_monitor_task, "adc_monitor_task", 4096, adc2_handle, 5, NULL);
}
