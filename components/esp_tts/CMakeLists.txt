# Minimal pinned ESP32-C3 Chinese TTS subset; no recognition models or partitions.
idf_component_register(INCLUDE_DIRS "include")
add_prebuilt_library(esp_tts_chinese "lib/libesp_tts_chinese.a")
add_prebuilt_library(voice_set_xiaole "lib/libvoice_set_xiaole.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE esp_tts_chinese voice_set_xiaole)
