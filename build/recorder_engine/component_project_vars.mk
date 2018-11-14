# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/components/recorder_engine/. $(PROJECT_PATH)/components/recorder_engine/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/recorder_engine -lrecorder_engine -L$(PROJECT_PATH)/components/recorder_engine/lib -lvad -lesp_wakenet -lnn_model_hilexin -lrecorder_eng 
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += recorder_engine
component-recorder_engine-build: 
