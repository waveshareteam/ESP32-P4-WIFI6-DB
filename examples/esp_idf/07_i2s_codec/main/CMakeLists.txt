idf_component_register(SRCS "i2s_es8311_example.c"
                    PRIV_REQUIRES driver esp_driver_i2s esp_driver_gpio esp_driver_i2c bsp_extra esp32_p4_wifi6_db
                    EMBED_FILES "canon.pcm")
