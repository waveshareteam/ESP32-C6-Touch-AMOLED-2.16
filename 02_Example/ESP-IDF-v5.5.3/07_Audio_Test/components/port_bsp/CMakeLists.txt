idf_component_register(
    SRCS 
    "i2c_bsp.cpp" 
    "display_bsp.cpp" 
    "codec_bsp.cpp"
    PRIV_REQUIRES 
    driver
    pmicpower
    espressif__esp_lcd_sh8601
    waveshare__esp_lcd_touch_cst9217
    esp_timer
    codec_board 
    REQUIRES
    esp_driver_sdmmc
    fatfs
    INCLUDE_DIRS 
    "./" 
    EMBED_FILES "./pcm/canon.pcm")
