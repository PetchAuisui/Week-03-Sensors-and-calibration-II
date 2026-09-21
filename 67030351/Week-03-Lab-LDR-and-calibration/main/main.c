#include <stdio.h>
#include <math.h> // จำเป็นต้องใช้สำหรับฟังก์ชันคณิตศาสตร์ exp()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// กำหนดแชนเนลและระดับการทนแรงดันให้เหมาะสมกับวงจร LDR
#define ADC_CHANNEL     ADC_CHANNEL_6     // ขา GPIO34 (ตรงกับ ADC1 Channel 6)
#define ADC_ATTEN       ADC_ATTEN_DB_12   // ระดับการทนแรงดันสำหรับช่วง ~3.3V ใน IDF v6
#define ADC_UNIT        ADC_UNIT_1

void app_main(void)
{
    // -----------------------------------------------------------------
    // 1) ตั้งค่า ADC Oneshot Unit
    // -----------------------------------------------------------------
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // -----------------------------------------------------------------
    // 2) ตั้งค่า Channel Configuration (ความละเอียดเริ่มต้นจะเป็นค่าสูงสุด เช่น 12-bit)
    // -----------------------------------------------------------------
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    // -----------------------------------------------------------------
    // 3) ระบบทำ Calibration (ดึงค่าปรับแต่งจากโรงงานที่บันทึกอยู่ในตัวชิปมาใช้)
    // -----------------------------------------------------------------
    adc_cali_handle_t cali_handle = NULL;
    bool cali_enabled = false;

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    
    esp_err_t cali_ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
    if (cali_ret == ESP_OK) {
        cali_enabled = true;
    }

    // -----------------------------------------------------------------
    // 4) Loop อ่านค่า LDR และแปลงเป็น Lux ด้วยสมการ Exponential จาก Excel
    // -----------------------------------------------------------------
    while (1) {
        int raw = 0;
        int voltage = 0;

        // อ่านค่าดิบดิจิทัลจากขา ADC
        esp_err_t r = adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw);
        if (r == ESP_OK) {
            if (cali_enabled) {
                // แปลงค่า Raw เป็นแรงดันไฟฟ้า (หน่วย mV) เพื่อแสดงผลเปรียบเทียบ
                adc_cali_raw_to_voltage(cali_handle, raw, &voltage);
                
                // คำนวณค่า Lux จากสมการความสัมพันธ์ในกราฟ Excel: y = 3.0326 * e^(0.0014 * x)
                // โดยที่แกน X (ตัวแปร x ในโค้ด) คือค่าดิจิทัลดิจิทัลดิบ (raw)
                double lux = 3.0326 * exp(0.0014 * (double)raw);

                // พิมพ์ผลลัพธ์ออกทาง Serial Monitor
                printf("ADC Raw = %d, Voltage = %d mV, Light = %.2f Lux\n", raw, voltage, lux);
            } else {
                printf("ADC Raw = %d (Calibration not enabled)\n", raw);
            }
        } else {
            printf("Error: Failed to read ADC value\n");
        }

        // หน่วงเวลาทุกๆ 500 มิลลิวินาที
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}