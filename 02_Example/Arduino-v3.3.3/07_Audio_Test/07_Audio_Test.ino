#include <Arduino.h>
#include <esp_spiffs.h>
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "bsp_lvgl_port.h"
#include "./src/port_bsp/i2c_bsp.h"
#include "./src/port_bsp/axp2101_bsp.h"
#include "ui.h"
#include "./src/port_bsp/codec_bsp.h"
#include "mood_pcm.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);
static lv_audio_ui src_ui;
CodecPort *CodecPort_ = NULL;
static uint8_t *rec_data_ptr = NULL;                 // malloc 存储mic数据
static esp_codec_dev_handle_t speaker = NULL;        // 播放句柄
static esp_codec_dev_handle_t microphone = NULL;     // 录音句柄
#define REC_DATA_MAX 512                             // 录音存储数据的最大字节
#define REC_SPIFFS_DATA_MAX 192000                   // 总字节数=采样率×声道数×每个采样字节数×录音时长（秒） 存入SPIFFS最大的字节
static EventGroupHandle_t FacTestEventGroup = NULL;  // 事件组句柄
static bool is_Music_flag = 0;                       // 正在播放音乐的标志位


esp_err_t bsp_spiffs_mount(void) {
  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = "spiffs",
    .max_files = 2,
    .format_if_mount_failed = true,
  };

  esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

  ESP_ERROR_CHECK(ret_val);

  size_t total = 0, used = 0;
  ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret_val != ESP_OK) {
    Serial.printf("Failed to get SPIFFS partition information (%s)\n", esp_err_to_name(ret_val));
  } else {
    Serial.printf("Partition size: total: %d, used: %d\n", total, used);
  }
  return ret_val;
}

void spiffs_rec_write_spk(uint8_t *rec_buf, size_t max_size) {
  FILE *rec_file = fopen("/spiffs/rec.raw", "wb");
  if (!rec_file) {
    ESP_LOGE(APP_NAME, "Failed to open file for recording");
    return;
  }
  size_t total_written = 0;
  ESP_LOGI(APP_NAME, "Start recording...");
  while (total_written < max_size) {
    int err = esp_codec_dev_read(microphone, rec_buf, REC_DATA_MAX);
    if (err == ESP_CODEC_DEV_OK) {
      fwrite(rec_buf, 1, REC_DATA_MAX, rec_file);
      total_written += REC_DATA_MAX;
    } else {
      ESP_LOGE(APP_NAME, "Failed to rec data");
    }
  }
  fclose(rec_file);
  ESP_LOGI(APP_NAME, "Recording completed, total %d bytes written", total_written);
}

void spiffs_play_mic(uint8_t *play_buf) {
  FILE *play_file = fopen("/spiffs/rec.raw", "rb");
  if (!play_file) {
    ESP_LOGE(APP_NAME, "Failed to open file for playback");
    return;
  }
  size_t read_bytes = 0;
  ESP_LOGI(APP_NAME, "Start playback...");
  while ((read_bytes = fread(play_buf, 1, REC_DATA_MAX, play_file)) > 0) {
    esp_codec_dev_write(speaker, play_buf, read_bytes);
  }
  fclose(play_file);
  ESP_LOGI(APP_NAME, "Playback completed");
}

static void Audio_start(void) {
  speaker = CodecPort_->Get_audio_codec_speaker();
  microphone = CodecPort_->Get_audio_codec_microphone();
  esp_codec_dev_sample_info_t fs = {};
  fs.sample_rate = 16000;
  fs.channel = 2;
  fs.bits_per_sample = 16;
  ESP_ERROR_CHECK(esp_codec_dev_open(speaker, &fs));             //打开播放
  ESP_ERROR_CHECK(esp_codec_dev_open(microphone, &fs));          //打开录音
  ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(speaker, 100));      //设置100声音大小
  ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(microphone, 35.0));  //设置录音时的增益
  if (rec_data_ptr == NULL) {
    rec_data_ptr = (uint8_t *)heap_caps_malloc(REC_DATA_MAX, MALLOC_CAP_DEFAULT);
    assert(rec_data_ptr);
  }
}

void Test_Audio_Task(void *arg) {
  bool is_recflag = 0;
  while (1) {
    EventBits_t even = xEventGroupWaitBits(FacTestEventGroup, (0x40) | (0x80) | (0x0100), pdFALSE, pdFALSE, portMAX_DELAY);
    if (even & 0x40) {
      bsp_lvgl_lock(pdMS_TO_TICKS(500));
      lv_label_set_text(src_ui.screen_label_1, "Recording..."); /*正在录音*/
      bsp_lvgl_unlock();
      spiffs_rec_write_spk(rec_data_ptr, REC_SPIFFS_DATA_MAX);
      bsp_lvgl_lock(pdMS_TO_TICKS(500));
      lv_label_set_text(src_ui.screen_label_1, "Recording done"); /*录音完成*/
      bsp_lvgl_unlock();
      is_recflag = 1;
    } else if (even & 0x80) {
      if (is_recflag) {
        bsp_lvgl_lock(pdMS_TO_TICKS(500));
        lv_label_set_text(src_ui.screen_label_1, "Playing recording..."); /*正在播放录音*/
        bsp_lvgl_unlock();
        spiffs_play_mic(rec_data_ptr);
        bsp_lvgl_lock(pdMS_TO_TICKS(500));
        lv_label_set_text(src_ui.screen_label_1, "Playback finished"); /*播放完成*/
        bsp_lvgl_unlock();
      }
    } else if (even & 0x0100) {
      bsp_lvgl_lock(pdMS_TO_TICKS(500));
      lv_label_set_text(src_ui.screen_label_1, "Playing music..."); /*正在播放音乐*/
      bsp_lvgl_unlock();
      /**/
      size_t bytes_write = 0;
      uint8_t *data_ptr = (uint8_t *)mood_pcm;
      while ((bytes_write < mood_pcm_len) && is_Music_flag) {
        esp_codec_dev_write(speaker, data_ptr, 256);
        bytes_write += 256;
        data_ptr += 256;
      }
      /**/
      bsp_lvgl_lock(pdMS_TO_TICKS(500));
      lv_label_set_text(src_ui.screen_label_1, "Playback finished"); /*播放完成*/
      bsp_lvgl_unlock();
    }
    xEventGroupClearBits(FacTestEventGroup, (0x40) | (0x80) | (0x0100));
  }
}

void FactoryTestStartCallback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  int id = (int)lv_event_get_user_data(e);
  if (code == LV_EVENT_CLICKED && id == 4) {
    xEventGroupSetBits(FacTestEventGroup, 0x40); /* 录音开始 */
  }
  if (code == LV_EVENT_CLICKED && id == 5) {
    xEventGroupSetBits(FacTestEventGroup, 0x80); /* 播放录音开始 */
  }
  if (code == LV_EVENT_CLICKED && id == 6) {
    is_Music_flag = 1;
    xEventGroupSetBits(FacTestEventGroup, 0x0100); /* 播放音乐开始 */
  }
  if (code == LV_EVENT_CLICKED && id == 7) {
    is_Music_flag = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("start");
  FacTestEventGroup = xEventGroupCreate(); /* 创建事件组 */
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);

  bsp_spiffs_mount();
  bsp_lvgl_init(I2cMasterBus_);

  CodecPort_ = new CodecPort(I2cMasterBus_, "C6_AMOLED_2_16");
  Audio_start();

  if (bsp_lvgl_lock(0)) {
    Serial.println("start ui");
    setup_audio_ui(&src_ui);
    bsp_lvgl_unlock();
  }
  xTaskCreatePinnedToCore(Test_Audio_Task, "Test_Audio_Task", 3 * 1024, NULL, 3, NULL, 0);
  lv_obj_add_event_cb(src_ui.screen_btn_4, FactoryTestStartCallback, LV_EVENT_ALL, (void *)4);
  lv_obj_add_event_cb(src_ui.screen_btn_5, FactoryTestStartCallback, LV_EVENT_ALL, (void *)5);
  lv_obj_add_event_cb(src_ui.screen_btn_6, FactoryTestStartCallback, LV_EVENT_ALL, (void *)6);
  lv_obj_add_event_cb(src_ui.screen_btn_7, FactoryTestStartCallback, LV_EVENT_ALL, (void *)7);
}



void loop() {
}
