# Safe Office - IoT 기반 사무실 안전 관리 시스템

ESP8266, STM32F4, 그리고 YOLOv8 객체 감지를 활용한 자동화된 사무실 환경 모니터링 및 제어 시스템입니다.

## 📋 개요

YOLO 기반의 실시간 객체 감지로 사무실 환경을 모니터링하고, STM32 미니컨트롤러를 통해 문, 가스 밸브, LED 등을 자동으로 제어합니다. ESP8266은 WiFi 통신을 담당합니다.

## 🏗️ 프로젝트 구조

```
Safe_Office/
├── esp/              # WiFi 모듈 (ESP8266)
│   └── esp.ino
├── server/           # YOLO 객체 감지 서버
│   ├── server.py
│   └── requirements.txt
├── stm32/            # 임베디드 제어 (STM32F411xE)
│   ├── main.c
│   ├── device_driver.h
│   ├── Makefile
│   ├── firewall_stepper.c/h
│   ├── gas_fan.c/h
│   ├── servo.c/h
│   └── [기타 드라이버 파일]
├── training/         # YOLOv8 모델 훈련
│   └── yolo_train.py
├── LICENSE
└── README.md
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
```YOLOv8 모델 훈련

`training/yolo_train.py`에서 커스텀 YOLO 모델을 훈련할 수 있습니다.

```bash
cd training
python yolo_train.py
```

**훈련 설정:**
- Model: YOLOv8s
- Epochs: 500
- Image Size: 320x320
- Batch Size: 32
- Optimizer: AdamW
- 증강: Flip, Rotation, Mosaic, Mixup

**출력:**
- 훈련된 모델: `runs/detect/train/weights/best.pt`
- 검증 메트릭: Precision, Recall, mAP50, mAP50-95

## 🔧 
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


**최종 업데이트**: 2026년 4월
