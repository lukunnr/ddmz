# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/esp-dlna/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp-dlna $(PROJECT_PATH)/components/esp-dlna/lib/libesp-dlna.a
COMPONENT_LINKER_DEPS += $(PROJECT_PATH)/components/esp-dlna/lib/libesp-dlna.a
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp-dlna
component-esp-dlna-build: 
