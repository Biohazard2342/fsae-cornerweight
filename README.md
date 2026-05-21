# FSAE Corner Weight

다이소 블루투스 체중계 4개를 분해·활용해 FSAE 자작차의 4코너 무게를 실시간으로 측정·표시하는 시스템입니다.

> 두원공과대학교 FSAE 팀 · Designed & built by **이윤재**

![architecture](docs/architecture.png)

## 동작 개요

각 바퀴 아래 분해된 BLE 체중계(ZH-S1402-V02 · Chipsea CST92P23B)가 광고하는 무게 패킷을 Arduino Nano ESP32가 BLE legacy advertisement로 동시에 스캔하고, MAC으로 4코너를 구분해 USB Serial로 JSON 한 줄씩 출력합니다. Raspberry Pi 4에서 돌아가는 FastAPI 서버는 시리얼 브리지가 전달한 데이터를 WebSocket으로 받아, 같은 LAN의 모든 브라우저에 실시간으로 푸시합니다.

```
체중계 FL/FR/RL/RR ──BLE Adv──> ESP32 ──USB Serial(JSON)──> RPi4 ──WebSocket──> Browser UI
                                                              └ FastAPI + uvicorn (systemd)
```

## 하드웨어

| 컴포넌트 | 모델 / 스펙 |
|---|---|
| BLE 체중계 | 다이소 ZH-S1402-V02 · Chipsea CST92P23B-SOP8 BLE RF 모뎀 + COB 메인 MCU · 50kg×4 로드셀 · Max 180kg, d=50g |
| BLE 광고 | Legacy advertisement (BLE 4.x), Manufacturer Specific Data (0xFF), 4ms 광고 간격 |
| 스캐너 | Arduino Nano ESP32 (ESP32-S3, USB-C native) |
| 호스트 | Raspberry Pi 4 (Debian 13 / Bookworm 호환) |
| 통신 | USB Serial 115200 baud (ESP32 ↔ RPi), WebSocket (RPi ↔ Browser) |

## BLE 패킷 구조 (리버싱 결과)

17 bytes의 단일 AD struct (Manufacturer Specific Data):

```
10  FF  C0  [seq]  [w_hi w_lo]  [?? ??]  0A 01 [??]  [MAC × 6]
└┬┘ └┬┘ └┬┘  └─┬─┘  └────┬────┘  └──┬──┘  └────┬────┘  └───┬───┘
 │   │   │    │         │         │          │           └─ 디바이스 MAC (정방향)
 │   │   │    │         │         │          └──────────── 0x0A, 0x01, 부수 카운터
 │   │   │    │         │         └────────────────────── 의미 미확정 (정밀 무게 후보)
 │   │   │    │         └──────────────────────────────── 무게 × 100 (BE 16-bit, 0.01 kg 단위)
 │   │   │    └────────────────────────────────────────── 시퀀스 카운터 (2씩 증가)
 │   │   └─────────────────────────────────────────────── Company ID LSB 추정 (0xC0 고정)
 │   └─────────────────────────────────────────────────── 0xFF = Manufacturer Specific Data
 └─────────────────────────────────────────────────────── AD struct length 16
```

검증: 3 kg 아령 → byte 4-5 = `0x0136` (310) → 3.10 kg.

## 디렉토리 구조

```
.
├── platformio.ini              ESP32 펌웨어 빌드 설정
├── src/main.cpp                ESP32 펌웨어 (NimBLE 스캔 + JSON 시리얼 출력)
├── rpi/
│   ├── server/main.py          FastAPI WebSocket 서버 (/ws/ingest, /ws/display)
│   ├── static/                 정적 UI (index.html, logo.png)
│   ├── bridge/serial_bridge.py /dev/ttyACM0 → /ws/ingest 포워더
│   ├── systemd/                cornerweight-server / -bridge unit
│   ├── run_server.sh           수동 실행용 (개발 중)
│   ├── run_bridge.sh
│   ├── run_dummy.sh            UI 테스트용 더미 publisher
│   └── dummy_publisher.py
└── logo.png                    두원공과대학교 로고
```

## 빌드 & 배포

### ESP32 펌웨어

```bash
# 의존성: PlatformIO Core (pip install platformio) + udev rules
pio run                 # 빌드
pio run -t upload       # /dev/ttyACM0로 DFU 업로드
pio device monitor      # 시리얼 모니터 (115200 baud)
```

`src/main.cpp` 상단의 `kScales[]`에 본인 체중계 MAC + 코너 매핑을 추가하세요.
`#define DUMP_ALL 1`로 빌드하면 주변 모든 BLE 광고를 사람-읽기용 hex로 출력합니다 — MAC 발견용.

### RPi (FastAPI + 브리지)

```bash
# 가상환경 + 의존성
python3 -m venv ~/cornerweight/.venv
~/cornerweight/.venv/bin/pip install fastapi 'uvicorn[standard]' websockets pyserial

# 코드 배치
~/cornerweight/
├── .venv/
├── server/main.py
├── static/{index.html, logo.png}
└── bridge/serial_bridge.py

# systemd 서비스로 자동 시작
sudo install -m 644 rpi/systemd/cornerweight-server.service /etc/systemd/system/
sudo install -m 644 rpi/systemd/cornerweight-bridge.service  /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now cornerweight-server cornerweight-bridge
```

브라우저에서 `http://<rpi-ip>:8000` 접속.

## UI 기능

- 위에서 본 차량 다이어그램 + 4코너 무게 (▲ 진행 방향 표시)
- 한국어 라벨: 앞 왼쪽 / 앞 오른쪽 / 뒤 왼쪽 / 뒤 오른쪽
- 헤더 상태 뱃지: 브라우저 / ESP32 / BLE 체중계 N/4 / 접속자 수
- 요약: 총 무게, 앞 비율, 좌 비율, 크로스(FL+RR)
- 목표값 설정 (앞·좌·크로스 %): localStorage 저장, 자동 보정 1~99%
- 조정 가이드: 크로스 → 좌우 우선순위, 더 가벼운 대각선 코너 추천 ("FR 코너를 약간 높여주세요")
- 용어 설명 (접이식)
- 모바일 반응형 (≤600px에서 차 그림 위로 + 코너 2×2 그리드)

## 운영 명령

```bash
# 상태
sudo systemctl status cornerweight-server cornerweight-bridge

# 로그 실시간
journalctl -u cornerweight-server -f
journalctl -u cornerweight-bridge -f

# 재시작
sudo systemctl restart cornerweight-server cornerweight-bridge
```

## 참고

- HX711 + 로드셀 직결 방식 선례: [luftaquila/a-fa-weight](https://github.com/luftaquila/a-fa-weight)
