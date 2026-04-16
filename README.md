# 🛡️🏢 Safe Office

ESP8266, STM32F4, 그리고 YOLOv8 객체 감지를 활용한 자동화된 사무실 환경 모니터링 및 제어 시스템입니다.

## 📋 개요

YOLO 기반의 실시간 객체 감지로 인가된 사용자를 인식하고, STM32를 통해 센서, 모터, LED 등을 자동으로 제어합니다. ESP8266은 WiFi 통신을 담당합니다.

---

## 🛠 기술 스택

| 기술 | 설명 |
|------|------|
| Python | 학습코드 작성 및 추론결과 전송 |
| YOLOv8 | person객체 추론 |
| STM32-F411xE(Cortex-M4) | 메인보드로 사용 |
| Raspberry Pi5 | 추론 및 ESP8266와 통신 (서버역할) |
| ESP8266 | STM32-F411xE 보드와 통신 |
| PILOMAX USB 웹캠| person객체 인식 |
| SG90 | 서보모터로 사용 |
| 28BYJ-48 | 스텝모터로 사용 |
| HC-SR501 | PIR모션감지센서로 사용 |
| MQ-2 Gas Sensor | 가스센서로 사용 |
| CP2102 | 외부전압공급으로 사용 |
| L293D Dual Motor Driver IC | DC모터 드라이버로 사용 |






---

## 📝 프로젝트 정보

- **프로젝트 기간:** 2026.04.03 ~ 2026.04.13
- **프로젝트 목표:** IoT 기반 사무실 안전 관리 시스템 구현

---

## 🧠 시스템 구성도

![시스템 구성도](docs/images/시스템구성도.png)


---

##  📷  프로토타입 사진

<div>
 <img src="docs/images/prototype.png" width="580" height="500" align="top">
 <img src="docs/images/추론.png" width="250" height="480" align="top">
</div>

---


## 🔌 회로도

![System Architecture](./docs/images/system.png)


---


## 🔧 하드웨어 핀맵
시스템에 사용된 센서, 모터, LED 및 통신 핀의 GPIO 매핑 정보이다.
![map](./docs/images/매핑.PNG)




---

## 🔄 플로우차트

![System Architecture](./docs/images/flow1.png)

![System Architecture](./docs/images/flow2.png)

![System Architecture](./docs/images/flow3.png)

![System Architecture](./docs/images/flow_idle.png)

![System Architecture](./docs/images/flow_door.png)

![System Architecture](./docs/images/flow_em.png)

---

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

---

## ⚙️ 시스템 요구사항

### Hardware
- STM32F411xE (Cortex-M4)
- ESP8266 WiFi Module
- 라즈베리파이5 + 웹캠

### Software
- **ESP**: Arduino IDE with ESP8266 Board Package
- **Server**: Python 3.8+, Flask, OpenCV, YOLOv8
- **STM32**: ARM GCC, Makefile, ST-Link

---

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

---

## 📱 주요 기능

- **자동 감지**: YOLO 기반 실시간 객체 감지
- **WiFi 통신**: ESP8266을 통한 무선 데이터 전송
- **임베디드 제어**: STM32로 물리적 장치 제어
  - 문 열기/닫기
  - 가스 모드
  - 화염 감지
  - LED 상태 표시

---

## 📡 통신 프로토콜

### STM32 <-> ESP8266
- 물리/인터페이스: UART 시리얼 통신
- 통신 방식: 비동기 직렬 통신
- 설정: 115200 baud, 8 data bits, no parity, 1 stop bit(8N1)
- 역할: STM32가 명령 전송, ESP8266이 서버 처리 결과를 다시 STM32로 반환

### ESP8266 <-> Server
- 네트워크 방식: Wi-Fi(IEEE 802.11 b/g/n, 2.4GHz)
- 전송 프로토콜: TCP/IP 기반 HTTP 통신
- 역할: ESP8266이 서버에 HTTP 요청을 보내고, 서버가 판별 결과를 응답

---

## 🔧 주요 설정값

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

---

## 🎬 프로토타입 및 시연 영상

https://youtu.be/J2NhM7mqCp0

---

## ⚠️ 문제 해결 과정 (Trouble Shooting)

### 📡 전송 오류 문제


![트러블슈팅](docs/images/trouble1.png)


- **문제:** ESP8266 <-> STM32 간 UART 통신 시 전송 오류 문제  
- **해결:** ESP8266의 UOT, UOR 대신 새로운 GPIO 할당 

### 📉 비정상 센서 값 출력

![트러블슈팅2](docs/images/trouble2.png)

- **문제:** 보드 첫 전원 인가 시 비정상적인 센서 값 출력
- **해결:** 초기 상태 확립 이후 시스템 활성화  

### 👤 사람을 인식하지 못하는 문제

![트러블슈팅3](docs/images/trouble3.png)

- **문제:** 모형 통합 이후 센서에서 의도하지 않은 값이 출력  
- **해결:** 케이블 연결 부분 절연처리, 재정리 이후 합선 문제 해결


---

## 👥 팀원 소개

<table>
  <tr>
    <td align="center">
      <a href="https://github.com/kkyle9104">
        <img src="https://github.com/kkyle9104.png" width="120px;" alt=""/>
        <br />
        <sub><b>kkyle9104</b></sub>
      </a>
      <br />
      Kim Byunghyun
    </td>
    <td align="center">
      <a href="https://github.com/ksi076">
        <img src="https://github.com/ksi076.png" width="120px;" alt=""/>
        <br />
        <sub><b>ksi076</b></sub>
      </a>
      <br />
      Kim Seil
    </td>
    <td align="center">
      <a href="https://github.com/qkrrkdtj">
        <img src="https://github.com/qkrrkdtj.png" width="120px;" alt=""/>
        <br />
        <sub><b>qkrrkdtj</b></sub>
      </a>
      <br />
      Park Kangseo
    </td>
    <td align="center">
      <a href="https://github.com/miggh2">
        <img src="https://github.com/miggh2.png" width="120px;" alt=""/>
        <br />
        <sub><b>miggh2</b></sub>
      </a>
      <br />
      Lee Gyeonghwan
    </td>
  </tr>
</table>

---

## 📝 라이선스

이 프로젝트는 [MIT License](LICENSE) 하에 배포됩니다.


**최종 업데이트**: 2026년 4월 14일 
