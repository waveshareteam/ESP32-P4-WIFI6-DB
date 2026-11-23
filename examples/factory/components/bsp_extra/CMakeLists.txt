set(SRCS "")
list(APPEND SRCS
    "src/bsp_board_extra.c"
    "src/bsp_wlan_wifi.c"
)

set(INCLUDE_DIRS "")
list(APPEND INCLUDE_DIRS "include")

foreach(SRC_DIR IN LISTS SRC_DIRS)
    file(GLOB_RECURSE SRC ${SRC_DIR}/*.c)
    list(APPEND SRCS ${SRC})
    list(APPEND INCLUDE_DIRS ${SRC_DIR}/include)
endforeach()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver esp_codec_dev esp_driver_gpio esp_driver_i2s esp_netif
    PRIV_REQUIRES esp_timer fatfs esp_psram esp_mm esp_driver_ledc
                  esp_driver_uart esp_eth esp_event freertos esp_wifi
                  esp_wifi_remote esp_hosted espressif__esp-lib-utils lwip nvs_flash
                  esp_audio_codec efuse esp_hw_support esp_partition esp_security
)

# target_compile_options(
#     ${COMPONENT_LIB} PRIVATE
#         -Wno-format
#         -Wno-int-conversion
#         -Wno-incompatible-pointer-types
#         -Wunused-function
#         -Wno-unused-variable
#         -Wno-unused-function
#         -Wno-overflow
#         -Wno-unused-but-set-variable
#         -Wno-discarded-qualifiers)
