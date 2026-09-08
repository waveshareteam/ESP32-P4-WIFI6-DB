set(PROJ_SRC ${CMAKE_CURRENT_LIST_DIR})

set(PROJ_SRCS_C
    "assets/img_app_xiaozhi.c"
    "board/bsp_extra_xiaozhi_identity.c"
    "board/bsp_extra_xiaozhi.c"
    "services/xiaozhi_activation.c"
    "services/xiaozhi_audio_processor.c"
    "services/xiaozhi_services.c"
)

set(PROJ_SRCS_CPP
    "XiaozhiActivationClient.cpp"
    "XiaozhiActivationPrompt.cpp"
    "XiaozhiApp.cpp"
    "XiaozhiAudioProcessor.cpp"
    "XiaozhiClient.cpp"
    "XiaozhiLocalization.cpp"
    "XiaozhiUi.cpp"
    "prompt/ogg_demuxer.cc"
)

idf_component_register(
    SRCS ${PROJ_SRCS_C} ${PROJ_SRCS_CPP}
    INCLUDE_DIRS ${PROJ_SRC} "services/include"
    REQUIRES brookesia_core esp32_p4_wifi6_db xiaozhi-fonts bsp_extra audio_focus
             esp_audio_codec esp_ringbuf
    PRIV_REQUIRES app_update cjson esp_app_format esp_event esp_http_client
                  efuse esp_partition esp_hw_support esp_security esp_system
                  esp_timer nvs_flash
                  espressif__esp-lib-utils esp_audio_effects esp_xiaozhi
                  esp-sr heap mbedtls mcp-c-sdk spi_flash
)

set_source_files_properties(
    ${PROJ_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)

