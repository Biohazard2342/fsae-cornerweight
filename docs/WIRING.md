# 코너 모듈 결선 가이드

다이소 ZH-S1402-V02 케이스 + 로드셀 4개 + HX711 + Arduino Nano ESP32 1코너 시제품 결선.

---

## 부품

| 항목 | 갯수 | 비고 |
|---|---|---|
| 다이소 ZH-S1402-V02 체중계 | 1대 | 케이스·로드셀 4개만 활용 (BLE PCB 폐기) |
| HX711 모듈 | 1개 | E+ E− B+ B− A+ A− 6핀 + VCC GND DT SCK 4핀 |
| Arduino Nano ESP32 | 1개 | ESP32-S3, USB-C |
| 점퍼 와이어 / 인두 / 작은 만능기판 | — | 12선 모으는 결선 노드용 |

## 두 가지 방법

### Method A — 다이소 어댑터 보드 활용 (단순, 권장)

다이소 케이스 안에 작은 `ZH-S1402-V02` 어댑터 보드가 4 로드셀을 휘트스톤으로 묶어놓은 상태. 그 보드의 4단자(E+/E−/S+/S−)만 빼서 쓰면 됨.

| 어댑터 보드 | HX711 |
|---|---|
| E+ | E+ |
| E− | E− |
| S+ | A+ |
| S− | A− |

(B+/B−, US+/US−, VDD, PT1, PT2 같은 다른 단자는 사용 안 함 — 메인 PCB용)

### Method B — 어댑터 보드 빼고 직접 결선 (현재 가는 길)

다이소 로드셀 4개 각각 3선 (Red / Black / White). 4개를 풀 Wheatstone bridge로 직접 묶음.

| HX711 핀 | 연결 |
|---|---|
| **E+** | Cell1·2·3·4 의 Red 4개 모두 한 점에 묶음 |
| **E−** | Cell1·2·3·4 의 Black 4개 모두 한 점에 묶음 |
| **A+** | 대각선 쌍 1의 White (예: Cell1 + Cell3 의 White) |
| **A−** | 대각선 쌍 2의 White (예: Cell2 + Cell4 의 White) |
| **B+** | 사용 안 함 |
| **B−** | 사용 안 함 |

> 대각선 쌍 = 케이스에서 마주 보는 두 셀. 인접한 두 셀 White끼리 묶으면 신호 cancel out 되어 측정 안 됨.

---

## HX711 → ESP32

반대쪽 4핀 (보통 VCC SCK DT GND 라벨):

| HX711 | ESP32 (Arduino Nano ESP32) |
|---|---|
| VCC | 5V (또는 VIN) |
| GND | GND |
| DT (DOUT) | **D4** (보드 실크 라벨 — Arduino 코어가 ESP32-S3 GPIO 7로 매핑) |
| SCK | **D5** (보드 실크 라벨 — GPIO 8로 매핑) |

> ⚠️ 펌웨어에는 **보드 라벨 매크로 `D4`/`D5`**로 코딩되어 있음 (`src/main.cpp`).
> 만약 raw 숫자 `4`/`5`로 적으면 그건 실리콘 GPIO 4·5에 해당해 다른 핀이 됨 — 주의.
> 즉, 보드의 D4·D5 실크프린트 라벨에 그대로 연결하면 됩니다.

---

## 작업 순서

### 1. 케이스에 로드셀 위치 표시
각 모서리(좌상/우상/좌하/우하)를 펜이나 마스킹테이프로 표시. 분해/결선 중 헷갈리지 않도록.

### 2. 솔더 전 멀티미터 검증
각 로드셀에서:
- Red ↔ Black: 약 **1 kΩ** (full coil)
- Red ↔ White: 약 **500 Ω**
- Black ↔ White: 약 **500 Ω**

위 셋이 비슷한 비율 나오면 정상 half-bridge. 한 셀이라도 값이 크게 다르면 셀 손상 의심.

### 3. 결선 노드 만들기
작은 만능기판이나 헤더핀 + 점퍼 다발로 12선 → 4선:
- 노드 1 = Red×4 → E+
- 노드 2 = Black×4 → E−
- 노드 3 = White1 + White3 → A+
- 노드 4 = White2 + White4 → A−

납땜 후 절연 테이프 또는 열수축 튜브로 마무리.

### 4. HX711 ↔ ESP32 4선 연결
표 그대로 (VCC/GND/DT/SCK → 5V/GND/D4/D5).

### 5. 펌웨어 업로드
ESP32 USB로 RPi에 꽂으면 `/dev/ttyACM0`로 잡힘.
```bash
ssh yunjae@192.168.0.158
cd ~/cornerweight/firmware
~/cornerweight/.venv/bin/pio run -t upload
```

### 6. 시리얼 모니터로 검증
```bash
~/cornerweight/.venv/bin/pio device monitor -b 115200
```
또는 `screen /dev/ttyACM0 115200`.

### 7. 대각선 매칭 확인
시리얼에서 `info` 입력하면 raw 값 출력. 다음 동작으로 검증:

| 동작 | 기대 결과 |
|---|---|
| 빈 체중계 | raw 안정 (수십 카운트 내 변동) |
| 한 셀만 손가락 누름 | raw 큰 변동 (수만~수십만) |
| **대각선 두 셀 동시 누름** | 같은 방향으로 더 큰 변동 ✓ |
| **인접한 두 셀 동시 누름** | 변동이 거의 없음 ⚠ → A+/A− White 짝 잘못. 바꿔 묶기 |

### 8. 캘리브레이션
시리얼 명령:
```
tare           # 빈 상태에서 영점
cal 3.00       # 3kg 아령 올린 후 (정확한 무게 입력)
info           # cal factor / tare 확인
```

이후 JSON에 `weight_kg` 자동 출력. RPi의 `cornerweight-bridge` 서비스가 받아서 웹 UI로 흘림.

---

## 시리얼 명령 요약

| 명령 | 설명 |
|---|---|
| `tare` | 현재 상태를 영점으로 |
| `cal <kg>` | 알려진 무게로 캘리브레이션 (예: `cal 3.10`) |
| `corner <FL\|FR\|RL\|RR>` | 코너 ID 설정 (NVS 저장) |
| `info` | 현재 cal / tare / raw 값 |
| `help` | 명령 목록 |

모두 NVS에 영구 저장되어 재부팅에도 유지.

---

## 트러블슈팅

| 증상 | 원인 후보 | 대처 |
|---|---|---|
| raw 값 0 또는 −1 고정 | HX711 not ready | VCC/GND/DT/SCK 결선 확인 |
| raw 값이 너무 빨리 떨림 (수십만 이상 노이즈) | E+/E− 연결 불량 또는 전원 부족 | E+ 단자에 5V 정확히 들어가는지 |
| 인접 셀 눌렀을 때 신호 안 변함 | A+/A− 대각선 매칭 정상 | 정상 (인접은 cancel 됨) |
| 대각선 셀 눌렀을 때 신호 안 변함 | A+/A− 매칭 잘못 | White 짝짓기 바꾸기 |
| weight_kg 출력 안 됨 | 캘리브레이션 안 됐음 | `cal <kg>` 실행 |
| 무게가 음수 | E+/E− 또는 A+/A− 극성 반대 | 그대로 `cal` 하면 자동 부호 처리됨 |
