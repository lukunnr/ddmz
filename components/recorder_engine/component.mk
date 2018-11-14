#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
COMPONENT_ADD_INCLUDEDIRS := . include

# COMPONENT_SRCDIRS :=  .
ifdef CONFIG_SR_TYPE_ESPRESSIF
    ifdef CONFIG_WAKEUP_WORD_NAME_HI_LEXIN
        LIBS := vad esp_wakenet nn_model_hilexin recorder_eng
    endif
    ifdef CONFIG_WAKEUP_WORD_NAME_ALEXA
        LIBS := vad esp_wakenet nn_model_alexa recorder_eng
    endif
    ifdef CONFIG_WAKEUP_WORD_NAME_HAOGE
        LIBS := vad esp_wakenet nn_model_haoge recorder_eng
    endif
endif



ifdef CONFIG_SR_TYPE_SNOWBOY
LIBS := vad snowboy
endif


COMPONENT_ADD_LDFLAGS +=  -L$(COMPONENT_PATH)/lib \
                           $(addprefix -l,$(LIBS)) \

ALL_LIB_FILES += $(patsubst %,$(COMPONENT_PATH)/lib/lib%.a,$(LIBS))
