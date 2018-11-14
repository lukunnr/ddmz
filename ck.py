#-*- coding: utf-8 -*-
import requests
import time
import hashlib
import base64

URL = "https://openapi.xfyun.cn/v2/aiui"
APPID = "5b976677"
API_KEY = "aa13a4a1ea884af9bcd275a2a7d62451"
AUDIO_PATH = "audio_test.wav"

def getHeader(aue, engineType, scene):
    curTime = str(int(time.time()))
    param = "{\"result_level\":\"complete\",\"aue\":\"raw\",\"auth_id\":\"6649a9975b8245ccacc8f062097a6577\",\"data_type\":\"audio\",\"sample_rate\":\"16000\",\"scene\":\"main\",\"lat\":\"39.26\",\"lng\":\"115.14\"}"
    paramBase64 = base64.b64encode(param)

    m2 = hashlib.md5()
    m2.update(API_KEY + curTime + paramBase64)
    checkSum = m2.hexdigest()

    header = {
        'X-CurTime': curTime,
        'X-Param': paramBase64,
        'X-Appid': APPID,
        'X-CheckSum': checkSum,
    }
    print header
    return header

def getBody(filepath):
    binfile = open(filepath, 'rb')
    data = binfile.read()
    return data

r = requests.post(URL, headers=getHeader(aue, engineType, scene), data=getBody(AUDIO_PATH))
print(r.content)
