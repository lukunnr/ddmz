deps_config := \
	/home/lvkun/work/esp32/esp/idf_0.93/components/app_trace/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/aws_iot/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/bt/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/esp32/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/esp_adc_cal/Kconfig \
	/home/lvkun/work/esp32/esp/project/ddmz2/components/esp_http_client/Kconfig \
	/home/lvkun/work/esp32/esp/project/ddmz2/components/espmqtt/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/ethernet/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/fatfs/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/freertos/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/heap/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/libsodium/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/log/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/lwip/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/mbedtls/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/openssl/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/pthread/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/spi_flash/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/spiffs/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/tcpip_adapter/Kconfig \
	/home/lvkun/work/esp32/esp/idf_0.93/components/wear_levelling/Kconfig \
	/home/lvkun/work/esp32/esp/project/ddmz2/components/MediaHal/Kconfig.projbuild \
	/home/lvkun/work/esp32/esp/idf_0.93/components/bootloader/Kconfig.projbuild \
	/home/lvkun/work/esp32/esp/idf_0.93/components/esptool_py/Kconfig.projbuild \
	/home/lvkun/work/esp32/esp/idf_0.93/components/partition_table/Kconfig.projbuild \
	/home/lvkun/work/esp32/esp/project/ddmz2/components/recorder_engine/Kconfig.projbuild \
	/home/lvkun/work/esp32/esp/project/ddmz2/components/userconfig/Kconfig.projbuild \
	/home/lvkun/work/esp32/esp/idf_0.93/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
