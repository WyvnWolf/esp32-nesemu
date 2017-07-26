// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/touch_pad.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#include "touchpad.h"

static const char* TAG = "Touch pad";
static bool s_pad_activated[TOUCH_PAD_MAX];
static int last_button;

#if CONFIG_HW_TOUCHPAD_START_SELECT_REVERSE
	#define TP_START_1 5
	#define TP_START_2 7
	#define TP_SELECT_1 4
	#define TP_SELECT_2 6
#else
	#define TP_START_1 4
	#define TP_START_2 6
	#define TP_SELECT_1 5
	#define TP_SELECT_2 7
#endif

static bool tp_start_pressed(int buttons)
{
		if( ((buttons >> TP_START_1) & 1) && ((buttons >> TP_START_2) & 1)) {
			return true;
		}
		return false;
}

static bool tp_select_pressed(int buttons)
{
		if( ((buttons >> TP_SELECT_1) & 1) && ((buttons >> TP_SELECT_2) & 1)) {
			return true;
		}
		return false;
}

static void tp_set_thresholds(void)
{
    uint16_t touch_value;
    for (int i=0; i<TOUCH_PAD_MAX; i++) {
        ESP_ERROR_CHECK(touch_pad_read(i, &touch_value));
        ESP_ERROR_CHECK(touch_pad_config(i, touch_value/2));
    }
}

/*
  Check if any of touch pads has been activated
  by reading a table updated by rtc_intr()
  If so, then print it out on a serial monitor.
  Clear related entry in the table afterwards
 */
static void tp_read_task(void *pvParameter)
{
    while (1) {
        for (int i=0; i<TOUCH_PAD_MAX; i++) {
            if (s_pad_activated[i] == true) {
                //ESP_LOGI(TAG, "T%d activated!", i);
								last_button |= 1 << i;
                // Wait a while for the pad being released
                vTaskDelay(10/ portTICK_PERIOD_MS);
                s_pad_activated[i] = false;
            }
        }
        // try and slow down the input when pressing Start/Select combo buttons
				if(tp_start_pressed(last_button) || tp_select_pressed(last_button)) {
        	vTaskDelay(3000 / portTICK_PERIOD_MS);
				} else {
					vTaskDelay(10 / portTICK_PERIOD_MS);
			  }
    }
}

/*
  Handle an interrupt triggered when a pad is touched.
  Recognize what pad has been touched and save it in a table.
 */
static void tp_rtc_intr(void * arg)
{
    uint32_t pad_intr = READ_PERI_REG(SENS_SAR_TOUCH_CTRL2_REG) & 0x3ff;
    uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);
    //clear interrupt
    WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);

    if (rtc_intr & RTC_CNTL_TOUCH_INT_ST) {
        for (int i = 0; i < TOUCH_PAD_MAX; i++) {
            if ((pad_intr >> i) & 0x01) {
                s_pad_activated[i] = true;
            }
        }
    }
}

int tpReadInput() {
	int val = 0x0;
  if(last_button != 0) {
		val |= last_button;
		if(tp_select_pressed(last_button)) {
		        val = 1;
		        ESP_LOGI(TAG,"Pressing Select");
		}

		if(tp_start_pressed(last_button)) {
						val = (1 << 13);
		        ESP_LOGI(TAG,"Pressing Start");
		}
		last_button = 0;
	}
	return val;
}

void tpcontrollerInit() {
	last_button = 0;
	touch_pad_init();
	tp_set_thresholds();
	touch_pad_isr_handler_register(tp_rtc_intr, NULL, 0, NULL);
	// Start a task to show what pads have been touched
	xTaskCreate(&tp_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
	ESP_LOGI(TAG, "Enabling hackerboxes badge touch pads");
}
