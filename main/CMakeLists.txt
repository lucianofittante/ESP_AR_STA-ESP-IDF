idf_component_register(SRCS "ESP_AP_STA.c"
                       INCLUDE_DIRS "C:/Users/lucho/Desktop/ESP_AP_STA/mdns/include")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
