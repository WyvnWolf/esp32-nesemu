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

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "driver/touch_pad.h"
#include "driver/gpio.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#include "touchpad.h"
#define TOUCH_THRESH_NO_USE (0)
#define TOUCH_THRESH_PERCENT (80)
static const char *TAG = "Touch pad";
static uint32_t s_pad_init_val[TOUCH_PAD_MAX];
static int last_button;
#define BUTTON0_GPIO GPIO_NUM_0

#if CONFIG_HW_TOUCHPAD_SELECT_REVERSE
#define TP_SELECT_1 4
#define TP_SELECT_2 6
#else
#define TP_SELECT_1 5
#define TP_SELECT_2 7
#endif

static void init_gpio() {
    //Configure button
    gpio_config_t btn_config;
    btn_config.intr_type = GPIO_INTR_ANYEDGE;    //Enable interrupt on both rising and falling edges
    btn_config.mode = GPIO_MODE_INPUT;           //Set as Input
    btn_config.pin_bit_mask = (1 << BUTTON0_GPIO); //Bitmask
    btn_config.pull_up_en = GPIO_PULLUP_DISABLE;    //Disable pullup
    btn_config.pull_down_en = GPIO_PULLDOWN_ENABLE; //Enable pulldown
    gpio_config(&btn_config);
    ESP_LOGI(TAG, "Button0 configured\n");
}

static bool tp_start_pressed(int buttons) {
  if(!gpio_get_level(BUTTON0_GPIO)) {
    return true;
  }
  return false;
}

static bool tp_select_pressed(int buttons) {
  if (((buttons >> TP_SELECT_1) & 1) && ((buttons >> TP_SELECT_2) & 1)) {
    return true;
  }
  return false;
}

static void tp_set_thresholds(void) {
  for (int i = 0; i < TOUCH_PAD_MAX; i++) {
    // init RTC IO and mode for touch pad.
    touch_pad_config(i, TOUCH_THRESH_NO_USE);
  }
  uint16_t touch_value;
  // delay some time in order to make the filter work and get a initial value
  vTaskDelay(500 / portTICK_PERIOD_MS);

  for (int i = 0; i < TOUCH_PAD_MAX; i++) {
    // read filtered value
    touch_pad_read_filtered(i, &touch_value);
    s_pad_init_val[i] = touch_value;
    ESP_LOGI(TAG, "test init touch val: %d\n", touch_value);
    // set interrupt threshold.
    ESP_ERROR_CHECK(touch_pad_set_thresh(i, touch_value * 2 / 3));
  }
}

int tpReadInput() {
  int val = 0;
  // ESP_LOGI(TAG, "last button %d", last_button);
  for (int i = 0; i < TOUCH_PAD_MAX; i++) {
    uint16_t value = 0;
    touch_pad_read_filtered(i, &value);
    if (value < s_pad_init_val[i] * TOUCH_THRESH_PERCENT / 100) {
      // ESP_LOGI(TAG, "T%d activated!", i);
      // ESP_LOGI(TAG, "value: %d; init val: %d\n", value, s_pad_init_val[i]);
      last_button |= 1 << i;
    }
  }
  if (tp_start_pressed(last_button)) {
    last_button |= (1 << 13);
    ESP_LOGI(TAG, "Pressing Start");
  }
  if (last_button != 0) {
    val |= last_button;
    if (tp_select_pressed(last_button)) {
      val = 1;
      ESP_LOGI(TAG, "Pressing Select");
    }
    last_button = 0;
  }
  return 0xFFFF ^ val;
}

void tpcontrollerInit() {
  ESP_LOGI(TAG, "Initializing touch pad");
  last_button = 0;
  touch_pad_init();
  touch_pad_filter_start(10);
  // Set measuring time and sleep time
  // In this case, measurement will sustain 0xffff / 8Mhz = 8.19ms
  // Meanwhile, sleep time between two measurement will be 0x1000 / 150Khz
  // = 27.3 ms
  touch_pad_set_meas_time(0x1000, 0xffff);

  // set reference voltage for charging/discharging
  // In this case, the high reference valtage will be 2.4V - 1.5V = 0.9V
  // The low reference voltage will be 0.8V, so that the procedure of charging
  // and discharging would be very fast.
  touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8,
                        TOUCH_HVOLT_ATTEN_1V5);
  tp_set_thresholds();
  init_gpio();
  ESP_LOGI(TAG, "Enabling hackerboxes badge touch pads");
}
