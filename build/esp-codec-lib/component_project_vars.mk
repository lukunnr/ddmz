# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp-codec-lib/include/esp-opus $(PROJECT_PATH)/components/esp-codec-lib/include/esp-amr $(PROJECT_PATH)/components/esp-codec-lib/include/esp-amrwbenc $(PROJECT_PATH)/components/esp-codec-lib/include/esp-fdk $(PROJECT_PATH)/components/esp-codec-lib/include/esp-flac $(PROJECT_PATH)/components/esp-codec-lib/include/esp-aac $(PROJECT_PATH)/components/esp-codec-lib/include/esp-ogg $(PROJECT_PATH)/components/esp-codec-lib/include/esp-share $(PROJECT_PATH)/components/esp-codec-lib/include/esp-stagefright $(PROJECT_PATH)/components/esp-codec-lib/include/esp-tremor $(PROJECT_PATH)/components/esp-codec-lib/include/audio_signal_process
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp-codec-lib -L$(PROJECT_PATH)/components/esp-codec-lib/lib -lesp-opus -lesp-mp3 -lesp-flac -lesp-tremor -lesp-aaac -lesp-ogg-container -lesp-amr -lesp-share -lesp-amrwbenc -lesp-aac -laudio_signal_process 
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp-codec-lib
component-esp-codec-lib-build: 
