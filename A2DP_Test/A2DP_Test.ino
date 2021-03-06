
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

extern "C" {
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
};
#define VS1053_DATA_SPI_SETTING     SPISettings(8000000, MSBFIRST, SPI_MODE0)
#define SCI_WRAMADDR 7
#define SCI_WRAM 6

#define VS1053_RESET   -1     // VS1053 reset pin (not used!)
// Feather M0 or 32u4
#if defined(__AVR__) || defined(ARDUINO_SAMD_FEATHER_M0)
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP8266
#elif defined(ESP8266)
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_FEATHER52)
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin
#endif

#define VS1053_MODE_SM_STREAM 0x0040

Adafruit_VS1053_FilePlayer player =
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

uint8_t data_global[VS1053_DATABUFFERLEN];
bool once = true;
uint8_t counter = 0;
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    switch (event) {
        case 0:{
            log_d("evt %d", event);
            /* set up device name */
            const char *dev_name = "ESP_SPEAKER";
            esp_bt_dev_set_device_name(dev_name);

            /* initialize A2DP sink */
            esp_a2d_register_callback(&bt_app_a2d_cb);
            esp_a2d_register_data_callback(bt_data_cb);
            esp_a2d_sink_init();

            /* initialize AVRCP controller */
            esp_avrc_ct_init();
            esp_avrc_ct_register_callback(bt_app_rc_ct_cb);

            /* set discoverable and connectable mode, wait to be connected */
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
            break;
        }
        default:
            log_e("unhandled evt %d", event);
            break;
    }
}

void on_data_read(uint8_t * data, int readbytes)
{
  for (int i=0; i<readbytes; i++) {
    Serial.print(data[i]);
  }
  Serial.println("");
  Serial.println(readbytes);
}

void bt_data_cb(const uint8_t *data, uint32_t len){
  //for(int i = 0; i < VS1053_DATABUFFERLEN; i + VS1053_DATABUFFERLEN){
  if(player.readyForData()){
    for(int i = 0; i < VS1053_DATABUFFERLEN; i++) { Serial.print(data_global[i]); Serial.print(" "); }
    Serial.println();
    memcpy(data_global, data, VS1053_DATABUFFERLEN);
    uint8_t data_test[8] = {0x53, 0xEF, 0x6E, 0x44, 0x00, 0x00, 0x00, 0x00};
    uint8_t data_test2[8] = { 0x45, 0x78, 0x69, 0x74, 0x00, 0x00, 0x00, 0x00 };
    uint8_t data_test3[32] = {167, 240, 180, 235, 2, 242, 138, 236, 211, 243, 122, 238, 10, 246, 199, 240, 54, 248, 75, 243, 162, 248, 174, 244, 157, 245, 26, 242, 226, 240, 75, 237};
    digitalWrite (VS1053_CS, HIGH );
    digitalWrite(VS1053_DCS, LOW);
    player.spiwrite(data_global, VS1053_DATABUFFERLEN);
    digitalWrite(VS1053_DCS, HIGH);
  }
  if(once){
    player.playFullFile("/track001.mp3", &on_data_read);
    once = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Starting...");
    if (! player.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  SD.begin(CARDCS);
  player.setVolume(0,0);
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
      log_e("initialize controller failed\n");
      return;
  }

  if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
      log_e("enable controller failed\n");
      return;
  }

  if (esp_bluedroid_init() != ESP_OK) {
      log_e("initialize bluedroid failed\n");
      return;
  }

  if (esp_bluedroid_enable() != ESP_OK) {
      log_e("enable bluedroid failed\n");
      return;
  }
  while(!player.readyForData()) { delay(100); }
  
  // Turn on MP3
  player.sciWrite(VS1053_REG_WRAMADDR, 0xC017);
  player.sciWrite(VS1053_REG_WRAM, 3);
  player.sciWrite(VS1053_REG_WRAMADDR, 0xC019);
  player.sciWrite(VS1053_REG_WRAM, 0);

  player.softReset();
  player.sciWrite(VS1053_REG_AUDATA, 44101);
  player.sciWrite(VS1053_REG_CLOCKF, 6 << 12);
  
  player.sciWrite(VS1053_REG_MODE, VS1053_MODE_SM_LINE1 | VS1053_MODE_SM_SDINEW);

  /* create application task */
  bt_app_task_start_up();

  /* Bluetooth device name, connection mode and profile set up */
  bt_app_work_dispatch(bt_av_hdl_stack_evt, 0, NULL, 0, NULL);
  
}
void loop() {
  // put your main code here, to run repeatedly:
}
