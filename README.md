# ESP-Audio Project

ESP-Audio project is an official development framework for the [ESP32](https://espressif.com/en) chip.


This project is a fully integrated solution which supports multi-source (e.g. Http / Flash / TF-Card / BT) and multi-format (e.g. aac / m4a / mp3 / ogg / opus / amr) music playing with some simple APIs.   
Along with some music-playing related features such as music-tone (can automatically interrupt and resume the music), playlist, DLNA / AIRPLAY protocol, Duer OS, re-sampling (e.g. 16K sample rate to 48K) and smartconfig, it will be very easy for users to develop an audio project.

## Feature List

### Hardware
- BT & BLE
- Wi-Fi
- TF-Card
- External RAM
- Auxiliary / Button / LED

### Software
- Music (TF-card / Flash / Http)
    - playing (OPUS / AMR / WAV / AAC / M4A / MP3 / OGG)
    - recording (OPUS / AMR / WAV)
- Protocol
    - DLNA
    - AIRPLAY (under developing)
- Cloud
    - Duer OS
    - Tuling
- Playlist (integrated with shuffle mode)
- Tone (automatically interrupt and resume the music)
- OTA (Over-The-Air software upgrade)
- Re-sampling (e.g. 16K sample rate to 48K)
- Smartconfig & Blufi
- Shell (uart terminal)

### Restriction
- Can not play 24bits music directly, but can be elevated to 32 bits by filling the last 8 bits with zero.
- BT Audio and Wi-Fi Audio can not coexist without PSRAM (external RAM) because of the RAM inefficiency. 

> Refer to [ESP-Audio Design Guideline](./ESP-Audio_Design_Guideline.md "ESP-Audio Design Guideline") to learn more restrictions.

## Directory Structure

- components
    - Bluetooth (BT Audio Demo)
    - DeviceController
        - AuxInManager (Code of Line-in mode)
        - SDCardManager 
        - TouchManager (Touch pad and button driver)
        - WifiManager (Wi-Fi and smartconfig)
    - dlna-lib (Library of DLNA)
    - DlnaService (DLNA Demo)
    - EspAppService (Phone-app demo)
    - esp-codec-lib (Decoder library)
    - httpd (Http server demo)
    - led (LED demo)
    - MediaHal (Hardware abstraction layer)
    - OtaService (Over the air upgrade demo)
    - player (Player library, managing all the player API)
    - SystemSal (System software abstraction layer)
    - TerminalService (Uart command shell)
    - TouchService (Touch pad and button demo)
    - userconfig (System configuration)
    - utils (functions and methods)
- docs (documents)
- main (app_main.c)
- tools
    - audio-esp.bin (flash tone binary, should be downloaded according to [partitions_esp_audio.csv](./partitions_esp_audio.csv "ESP-Audio partition table"))
    - bt.defaults (default BT audio sdkconfig)
    - wifi.defaults (default Wi-Fi audio sdkconfig)
    - mk_audio_bin.py (a python script for generating tone binary, refer to "ESP-Audio Tone" section in [ESP-Audio User Guide](docs/ESP-Audio_User_Guide.md "Audio User Guide"))

## Quick Usage Example
```c
//initialize the player
EspAudioInit(&playerConfig, &player);
//support mp3 music
EspAudioCodecLibAdd(CODEC_DECODER, 10 * 1024, "mp3", MP3Open, MP3Process, MP3Close, MP3TriggerStop);
//initialize Wi-Fi and connect
deviceController->enableWifi(deviceController);
//Play the music
EspAudioPlay("http://iot.espressif.com/file/example.mp3");
```
With these simple APIs, users can develop their own audio project on any ESP32 based hardware.

***

## Getting Started

### Preparation
ESP-IDF and related environment should be set up to build the ESP-Audio project and to generate the binaries as well as to `make flash`.  

Documents and hardware guides are also available by following links.
1. Refer to the [ESP-IDF README](https://github.com/espressif/esp-idf "ESP-IDF doc") on github to set up the environment.
2. Refer to the [ESP-IDF Programming Guide](http://esp-idf.readthedocs.io/en/latest/index.html "ESP-32 IDF system") for information about the ESP-IDF system (including hardware and software).  

    > Please pay attention to [Partition Tables](http://esp-idf.readthedocs.io/en/latest/api-guides/partition-tables.html "Partition Tables Section") section.  
    > This is where Flash Tone Flash Playlist as well as OTA binaries are stored.

3. Refer to [ESP32 documents](http://espressif.com/en/support/download/documents?keys=&field_type_tid%5B%5D=13 "ESP32 documents") for datasheet and technical references, etc.

## Developing ESP-Audio Project
Refer to [ESP-Audio User Guide](docs/ESP-Audio_User_Guide.md "Audio User Guide").

[Build](http://esp-idf.readthedocs.io/en/latest/get-started/index.html#build-and-flash "build section in 'ESP-IDF Programming Guide'") this project and generate binary files to be downloaded.

## Downloading Firmware
Once the binary files are generated, these files can be downloaded into the hardware.  
>Check out the schematic in */esp-audio-app/docs* directory if any demo board is in use  

When using *ESP32_LyraXX* demo board, please follow these following steps to download the firmware:

1. Press the *BOOT* button and the *RST* button simultaneously.
2. Release the *RST* button.
3. Release the *BOOT* button.

***

## Resources

- ESP32 Home Page: https://espressif.com/en
- ESP-IDF: https://github.com/espressif/esp-idf
- ESP-IDF Programming Guide: http://esp-idf.readthedocs.io/en/latest/index.html
- Espressif Documents: https://espressif.com/en/support/download/documents

### Audiopedia
This cyclopedia is about the general ideas and structure of ESP-Audio SDK for users who are interested in the general implementation of the ESP-Audio system. 

#### [Audiopedia Link](docs/Audiopedia/Audiopedia.md "Audiopedia")

