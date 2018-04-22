#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nofrendo.h"
#include "nvs_flash.h"
#include "tft.h"
#include "tftspi.h"
#include "touchpad.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <time.h>

// ==========================================================
// Define which spi bus to use TFT_VSPI_HOST or TFT_HSPI_HOST
#define SPI_BUS TFT_HSPI_HOST

//---------------------------------
static void disp_header(char *info) {
  TFT_fillScreen(TFT_BLACK);
  TFT_resetclipwin();

  _fg = TFT_ORANGE;
  _bg = (color_t){64, 64, 64};

  if (_width < 240)
    TFT_setFont(DEFAULT_FONT, NULL);
  else
    TFT_setFont(DEJAVU18_FONT, NULL);
  TFT_fillRect(0, 0, _width - 1, TFT_getfontheight() + 8, _bg);
  TFT_drawRect(0, 0, _width - 1, TFT_getfontheight() + 8, TFT_CYAN);

  TFT_fillRect(0, _height - TFT_getfontheight() - 9, _width - 1,
               TFT_getfontheight() + 8, _bg);
  TFT_drawRect(0, _height - TFT_getfontheight() - 9, _width - 1,
               TFT_getfontheight() + 8, TFT_CYAN);

  TFT_print(info, CENTER, 4);

  _bg = TFT_BLACK;
  TFT_setclipwin(0, TFT_getfontheight() + 9, _width - 1,
                 _height - TFT_getfontheight() - 10);
}

int nes_partition = 0x40;
#define max_sel 6
char *name[max_sel] = {"1 - One", "2 - Two",
                       "3 - Three", "4 - Four",
                       "5 - Five", "6 - Six"};
void display_roms(int selection) {
  for (int count = 0; count < max_sel; count++) {
    if (count == selection) {
      _fg = TFT_YELLOW;
    } else {
      _fg = TFT_CYAN;
    }
    TFT_print(name[count], 20, 60 + (count * 20));
  }
}

void choose_rom() {
  TFT_resetclipwin();
  TFT_setRotation(LANDSCAPE_FLIP);
  disp_header("ESP32 NES");
  TFT_setFont(COMIC24_FONT, NULL);

  _fg = TFT_WHITE;
  TFT_print("Select Game", CENTER, 20);
  TFT_setFont(DEJAVU18_FONT, NULL);

  display_roms(0);
  int selection = 0;
  int old_selection = 0;
  while (1) {
    vTaskDelay(30 / portTICK_RATE_MS);
    int val = tpReadInput();
    int chg = val ^ 0xffff;

    if ((chg & 8) == 8) {
      break;
    }
    if ((chg & 16) == 16) {
      selection = (selection == (max_sel - 1) ? (max_sel - 1) : selection + 1);
    }
    if ((chg & 64) == 64) {
      selection = (selection == 0 ? 0 : selection - 1);
    }
    if (selection != old_selection) {
      display_roms(selection);
      vTaskDelay(300 / portTICK_RATE_MS);
    }
    old_selection = selection;
  }
  nes_partition += selection;
}

char *osd_getromdata() {
  char *romdata;
  const esp_partition_t *part;
  spi_flash_mmap_handle_t hrom;
  esp_err_t err;
  nvs_flash_init();
  part = esp_partition_find_first(nes_partition, 1, NULL);
  if (part == 0)
    printf("Couldn't find rom part!\n");
  err = esp_partition_mmap(part, 0, 0x50000, SPI_FLASH_MMAP_DATA,
                           (const void **)&romdata, &hrom);
  if (err != ESP_OK)
    printf("Couldn't map rom part!\n");
  printf("Initialized. ROM@%p\n", romdata);
  return (char *)romdata;
}

esp_err_t event_handler(void *ctx, system_event_t *event) { return ESP_OK; }

int nofrendo_app_main(void) {
  printf("NoFrendo start!\n");
  nofrendo_main(0, NULL);
  printf("NoFrendo died? WtF?\n");
  asm("break.n 1");
  return 0;
}

void app_main() {
  esp_err_t ret;

  // === SET GLOBAL VARIABLES ==========================
  // ==== Set display type                         =====
  tft_disp_type = DEFAULT_DISP_TYPE;
  _width = DEFAULT_TFT_DISPLAY_WIDTH;   // smaller dimension
  _height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension

  // ===================================================
  // ==== Set maximum spi clock for display read    ====
  //      operations, function 'find_rd_speed()'    ====
  //      can be used after display initialization  ====
  max_rdclock = 8000000;
  // ===================================================

  // ====================================================================
  // === Pins MUST be initialized before SPI interface initialization ===
  // ====================================================================
  TFT_PinsInit();

  // ====  CONFIGURE SPI DEVICES(s)
  // ====================================================================================
  spi_lobo_device_handle_t spi;

  spi_lobo_bus_config_t buscfg = {
      .miso_io_num = PIN_NUM_MISO, // set SPI MISO pin
      .mosi_io_num = PIN_NUM_MOSI, // set SPI MOSI pin
      .sclk_io_num = PIN_NUM_CLK,  // set SPI CLK pin
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 6 * 1024,
  };
  spi_lobo_device_interface_config_t devcfg = {
      .clock_speed_hz = 8000000,         // Initial clock out at 8 MHz
      .mode = 0,                         // SPI mode 0
      .spics_io_num = -1,                // we will use external CS pin
      .spics_ext_io_num = PIN_NUM_CS,    // external CS pin
      .flags = LB_SPI_DEVICE_HALFDUPLEX, // ALWAYS SET  to HALF DUPLEX MODE!!
                                         // for display spi
  };

  // ==================================================================
  // ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

  ret = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
  assert(ret == ESP_OK);
  printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
  disp_spi = spi;

  // ==== Test select/deselect ====
  ret = spi_lobo_device_select(spi, 1);
  assert(ret == ESP_OK);
  ret = spi_lobo_device_deselect(spi);
  assert(ret == ESP_OK);

  printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
  printf("SPI: bus uses native pins: %s\r\n",
         spi_lobo_uses_native_pins(spi) ? "true" : "false");

  // ================================
  // ==== Initialize the Display ====

  printf("SPI: display init...\r\n");
  TFT_display_init();

  // ---- Detect maximum read speed ----
  max_rdclock = find_rd_speed();
  printf("SPI: Max rd speed = %u\r\n", max_rdclock);

  // ==== Set SPI clock used for display operations ====
  spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
  printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

  font_rotate = 0;
  text_wrap = 0;
  font_transparent = 0;
  font_forceFixed = 0;
  gray_scale = 0;
  TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
  TFT_setFont(DEFAULT_FONT, NULL);
  TFT_resetclipwin();

  tpcontrollerInit();
  choose_rom();
  nofrendo_app_main();
}
