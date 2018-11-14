# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/libduer-device/include $(PROJECT_PATH)/components/libduer-device/lightduer/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/libduer-device -L$(PROJECT_PATH)/components/libduer-device/lightduer -lduer-device
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += libduer-device
component-libduer-device-build: 
