#include "current_input.h"
#include "current_scale.h"
#include <math.h>
#include <stddef.h>
#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t adc;
static bool ready;

esp_err_t current_input_init(void)
{
    if (ready) return ESP_OK;
    esp_err_t result;
    if (adc == NULL) {
        const adc_oneshot_unit_init_cfg_t config = {.unit_id = ADC_UNIT_1};
        result = adc_oneshot_new_unit(&config, &adc);
        if (result != ESP_OK) return result;
    }
    const adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    result = adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &channel);
    if (result != ESP_OK) return result;
    ready = true;
    return ESP_OK;
}
esp_err_t current_input_read(float *out_demo_a)
{
    if (out_demo_a == NULL) return ESP_ERR_INVALID_ARG;
    *out_demo_a = NAN;
    if (!ready) return ESP_ERR_INVALID_STATE;
    unsigned sum = 0U;
    for (unsigned i = 0; i < 8U; ++i) {
        int raw = 0;
        esp_err_t result = adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);
        if (result != ESP_OK) return result;
        if (raw < 0 || raw > 4095) return ESP_ERR_INVALID_RESPONSE;
        sum += (unsigned)raw;
    }
    return current_scale_from_adc((float)sum / 8.0f, out_demo_a)
        ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
