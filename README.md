# Safe Office

[프로젝트에 대한 한 줄 설명을 여기에 작성하세요.]

## 📋 개요

프로젝트의 목적과 전체 구조를 설명합니다.

## 🏗️ 프로젝트 구조

```
Cortex-project/
├── esp/              # ESP8266 WiFi 모듈
│   └── esp.ino      # Arduino 스케치
├── server/          # Python Flask 서버
│   └── server.py    # YOLO 객체 감지 및 이미지 처리
├── stm32/           # STM32F4 임베디드 시스템
│   ├── main.c       # 메인 프로그램
│   ├── device_driver.h
│   ├── Makefile     # 빌드 설정
│   └── [기타 드라이버 파일들]
├── .gitignore
├── LICENSE          # MIT License
└── README.md        # 이 파일
```

## ⚙️ 시스템 요구사항

### Hardware
- STM32F411xE (Cortex-M4)
- ESP8266 WiFi Module
- 라즈베리파이5

### Software
- **ESP**: Arduino IDE with ESP8266 Board Package
- **Server**: Python 3.8+, Flask, OpenCV, YOLOv8
- **STM32**: ARM GCC, Makefile, ST-Link

## 🚀 설치 및 실행

### 1. STM32 빌드 및 플래시

```bash
cd stm32
make
# make flash  # (ST-Link 연결 시)
```

### 2. Server 설정

```bash
cd server
pip install -r requirements.txt
python server.py
```

Server는 `http://0.0.0.0:5000` 에서 실행됩니다.

**Endpoints:**
- `/capture` - 이미지 캡처 및 객체 감지
- `/count` - 현재 객체 개수 조회

### 3. ESP8266 설정

1. Arduino IDE에서 `esp/esp.ino` 열기
2. WiFi SSID, Password, Server URL 설정
3. Board: ESP8266, Port 선택 후 업로드

## 📱 주요 기능

- **자동 감지**: YOLO 기반 실시간 객체 감지
- **WiFi 통신**: ESP8266을 통한 무선 데이터 전송
- **임베디드 제어**: STM32로 물리적 장치 제어
  - 문 열기/닫기
  - 가스 모드
  - 화염 감지
  - LED 상태 표시

## 📡 통신 프로토콜

### ESP8266 → STM32
```
HELLO    → 응답 대기
CAPTURE  → 카메라 신호 요청
```

### ESP8266 → Server
```
GET /capture  → 감지된 객체 개수 반환
GET /count    → 마지막 감지 결과 반환
```

## 🔧 주요 설정값

**STM32 (stm32/main.c)**
```c
#define MOVEMENT_ADC_THRESHOLD     2000
#define DOOR_HOLD_MS              3000
#define GAS_MODE_TRIGGER_ADC      1200
```

**ESP8266 (esp/esp.ino)**
```cpp
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* serverUrl = "http://SERVER_IP:5000/capture";
```

**Server (server/server.py)**
```python
MODEL_PATH = "best.pt"  # YOLOv8 가중치 파일
```

## 📝 라이선스

이 프로젝트는 [MIT License](LICENSE) 하에 배포됩니다.



[이메일 또는 연락처]

---

**최종 업데이트**: 2026년 4월
