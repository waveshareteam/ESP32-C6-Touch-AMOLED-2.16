
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_spiffs.h>

#include "lvgl_bsp.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "codec_bsp.h"
#include "power_bsp.h"
#include "gui_guider.h"

#define TAG "main"
#define APP_NAME "audio"

I2cMasterBus user_i2cbus(7,8,0); //scl,sda,i2c_port
DisplayPort *user_display = NULL;
CodecPort *user_codec = NULL;
lv_ui src_ui;
static uint8_t *rec_data_ptr = NULL;                // malloc 存储mic数据
static esp_codec_dev_handle_t speaker = NULL;       // 播放句柄
static esp_codec_dev_handle_t microphone = NULL;    // 录音句柄
#define REC_DATA_MAX 512                            // 录音存储数据的最大字节
#define REC_SPIFFS_DATA_MAX 192000                  // 总字节数=采样率×声道数×每个采样字节数×录音时长（秒） 存入SPIFFS最大的字节
static EventGroupHandle_t FacTestEventGroup = NULL; // 事件组句柄
static bool is_Music_flag = 0;                     // 正在播放音乐的标志位

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = true,
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    ESP_ERROR_CHECK(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

void fac_audio_default_init() {
    speaker = user_codec->Get_audio_codec_speaker();
    microphone = user_codec->Get_audio_codec_microphone();
}

void fac_audio_run_init() {
    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate = 16000;        
    fs.channel = 2;
    fs.bits_per_sample = 16;
    ESP_ERROR_CHECK(esp_codec_dev_open(speaker, &fs));               //打开播放
    ESP_ERROR_CHECK(esp_codec_dev_open(microphone, &fs));            //打开录音
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(speaker, 100));         //设置90声音大小
    ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(microphone, 35.0));    //设置录音时的增益
    if(rec_data_ptr == NULL) {
        rec_data_ptr = (uint8_t *)heap_caps_malloc(REC_DATA_MAX, MALLOC_CAP_DEFAULT);
        assert(rec_data_ptr);
    }
}

void fac_rec_write_storage(uint8_t *rec_buf,size_t max_size) {
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

void fac_play_read_storage(uint8_t *play_buf) {
    FILE *play_file = fopen("/spiffs/rec.raw", "rb");
    if (!play_file) {
        ESP_LOGE(APP_NAME, "Failed to open file for playback");
        return;
    }
    size_t read_bytes = 0;
    ESP_LOGI(APP_NAME, "Start playback...");
    while ((read_bytes = fread(play_buf, 1, REC_DATA_MAX, play_file)) > 0) {
        esp_codec_dev_write(speaker,play_buf, read_bytes);
    }
    fclose(play_file);
    ESP_LOGI(APP_NAME, "Playback completed");
}

esp_err_t play_pcm_from_spiffs(uint8_t *play_buf, const char *file_path,bool *Music_flag) {
    if(*Music_flag == 0)
    return ESP_OK;
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        ESP_LOGE(APP_NAME, "Failed to open file: %s", file_path);
        return ESP_FAIL;
    }
    size_t read_bytes = 0;
    while (1) {
        read_bytes = fread(play_buf, 1, REC_DATA_MAX, f);
        if (read_bytes > 0) {
            // 写入音频设备
            esp_err_t ret = esp_codec_dev_write(speaker, play_buf, read_bytes);
            if (ret != ESP_OK) {
                ESP_LOGE(APP_NAME, "codec write failed");
                fclose(f);
                return ret;
            }
        }
        // 读完了
        if (read_bytes < REC_DATA_MAX) {
            if (feof(f)) {
                break;
            }
            if (ferror(f)) {
                ESP_LOGE(APP_NAME, "file read error");
                fclose(f);
                return ESP_FAIL;
            }
        }
        if (*Music_flag == 0) {
            fclose(f);
            return ESP_OK;
        }
    }
    fclose(f);
    return ESP_OK;
}

void fac_audio_close_deinit() {
    esp_codec_dev_close(speaker);
    esp_codec_dev_close(microphone);
    if(rec_data_ptr) {
        heap_caps_free(rec_data_ptr);
        rec_data_ptr = NULL;
    }
}

void Test_Audio_Task(void *arg) {
    bool is_recflag = 0;
    while(1) {
        EventBits_t even = xEventGroupWaitBits(FacTestEventGroup,(0x40) | (0x80) | (0x0100),pdFALSE,pdFALSE,portMAX_DELAY);
        if(even & 0x40) {
            Lvgl_lock(pdMS_TO_TICKS(500));
		    lv_label_set_text(src_ui.screen_label_1, "Recording...");                 /*正在录音*/
            Lvgl_unlock();
		    fac_rec_write_storage(rec_data_ptr,REC_SPIFFS_DATA_MAX);
            Lvgl_lock(pdMS_TO_TICKS(500));
		    lv_label_set_text(src_ui.screen_label_1, "Recording done");               /*录音完成*/
            Lvgl_unlock();
            is_recflag = 1;
        } else if(even & 0x80) {
            if(is_recflag) {
                Lvgl_lock(pdMS_TO_TICKS(500));
                lv_label_set_text(src_ui.screen_label_1, "Playing recording...");     /*正在播放录音*/
                Lvgl_unlock();
		        fac_play_read_storage(rec_data_ptr);
                Lvgl_lock(pdMS_TO_TICKS(500));
		        lv_label_set_text(src_ui.screen_label_1, "Playback finished");        /*播放完成*/
                Lvgl_unlock();
            }
        } else if(even & 0x0100) {
            Lvgl_lock(pdMS_TO_TICKS(500));
            lv_label_set_text(src_ui.screen_label_1, "Playing music...");             /*正在播放音乐*/
            Lvgl_unlock();
            play_pcm_from_spiffs(rec_data_ptr,"/spiffs/mood.pcm",&is_Music_flag);      /*必须使用分区,不然ota切换不回来*/
            Lvgl_lock(pdMS_TO_TICKS(500));
            lv_label_set_text(src_ui.screen_label_1, "Playback finished");             /*播放完成*/
            Lvgl_unlock();
        }
        xEventGroupClearBits(FacTestEventGroup,(0x40) | (0x80) | (0x0100));
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

extern "C" void app_main(void)
{
    FacTestEventGroup = xEventGroupCreate(); /* 创建事件组 */
    bsp_spiffs_mount();
    Custom_PmicPortInit(&user_i2cbus,0x34);
    user_display = new DisplayPort(user_i2cbus,480,480);
    user_display->DisplayPort_TouchInit();
    user_codec = new CodecPort(user_i2cbus,"C6_AMOLED_2_16");
    Lvgl_PortInit(*user_display);
    fac_audio_default_init();
    fac_audio_run_init();
    if(Lvgl_lock(-1) == ESP_OK) {
		setup_ui(&src_ui);
    	Lvgl_unlock();
  	}
    xTaskCreatePinnedToCore(Test_Audio_Task, "Test_Audio_Task", 3 * 1024, NULL, 3, NULL,0);
    lv_obj_add_event_cb(src_ui.screen_btn_4, FactoryTestStartCallback , LV_EVENT_ALL, (void *)4);
    lv_obj_add_event_cb(src_ui.screen_btn_5, FactoryTestStartCallback , LV_EVENT_ALL, (void *)5); 
    lv_obj_add_event_cb(src_ui.screen_btn_6, FactoryTestStartCallback , LV_EVENT_ALL, (void *)6);
    lv_obj_add_event_cb(src_ui.screen_btn_7, FactoryTestStartCallback , LV_EVENT_ALL, (void *)7); 
        
}
