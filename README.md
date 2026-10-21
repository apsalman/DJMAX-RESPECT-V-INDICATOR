# 🎯 SepanViewer Native (C++ Standalone)

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows-blue?logo=windows" alt="Platform" />
  <img src="https://img.shields.io/badge/Language-C++17-00599C?logo=c%2B%2B" alt="Language" />
  <img src="https://img.shields.io/badge/Capture-DXGI_Desktop_Duplication_(DirectX_11)-9cf?logo=directx" alt="Capture" />
  <img src="https://img.shields.io/badge/Overlay-Direct_GDI%2B-brightgreen" alt="Overlay" />
  <img src="https://img.shields.io/badge/Target-DJMAX_RESPECT_V-orange" alt="Target Game" />
  <img src="https://img.shields.io/badge/RAM_Usage-~18MB-success" alt="RAM Usage" />
</p>

<p align="center">
  <b>DJMAX RESPECT V 전용 초경량 독립형 실시간 세부판정 인디케이터 (Hit Error Bar)</b><br>
  OBS나 웹 브라우저, 무거운 파이썬 가상환경 없이 <b>단일 C++ 실행 파일(.exe)</b>만으로 게임 화면 위에 직접 판정 눈금자를 띄웁니다.
</p>

---

## 💡 프로젝트 소개 (About)

본 프로젝트는 **[Sobrem 님의 예시 영상](https://www.youtube.com/watch?v=be2ie5e_9og)**으로 알려진 [DJMAX RESPECT V](https://store.steampowered.com/app/960170/DJMAX_RESPECT_V) 세부판정 뷰어(SepanViewer)의 아이디어에서 출발하였습니다.

기존의 **Python + OBS Studio 브라우저 소스** 방식은 방송 송출 화면을 통해서만 확인할 수 있어 플레이어가 게임 플레이 도중 실시간으로 타점을 확인하기 어렵다는 한계가 있었습니다. 이를 해결하기 위해 **Google AI와의 협업을 통해 완전히 독립적인 C++ 네이티브 프로그램으로 전환**하였으며, 화면 캡처 파이프라인을 **Windows 네이티브 DXGI Desktop Duplication API**로 재설계하여 최상의 반응성과 정밀도를 달성했습니다.

---

## ⚡ 기술적 특징 (Technical Highlights)

* **🚀 Windows DXGI Desktop Duplication (DirectX 11) 네이티브 캡처**
  * DWM(데스크톱 창 관리자) 합성 지연을 우회하고 **GPU VRAM에서 기어 영역을 직접 초고속 복사**.
  * `DXGI_OUTDUPL_FRAME_INFO.LastPresentTime`을 활용하여 **"GPU가 실제로 프레임을 화면에 출력한 절대 시각"**을 1ms 미만의 오차로 획득, 캡처 파이프라인 지연을 근본적으로 상쇄.
* **📉 극적인 시스템 자원 절감**
  * 파이썬 런타임 및 멀티프로세싱 오버헤드를 완전히 제거하여 **메모리(RAM) 사용량을 약 18MB 수준으로 압축**.
  * CPU/GPU 점유율이 0%에 수렴하여 인게임 프레임 드랍을 일절 유발하지 않음.
* **📏 BMS 스타일 가로형 히트 에러 바**
  * 기어 옆에 세로로 뜨던 기존 방식 대신, 노트가 떨어지는 **판정선 바로 위/아래에 가로형 슬림 인디케이터**를 배치하여 시선 이동 없이 즉각적인 타이밍 조절 가능.
* **🟡 실시간 평균 타점 화살표 (▲) & AVG 수치 표기**
  * 최근 타격 오차의 이동 평균을 실시간 계산하여 눈금자 아래 **황금색 삼각형(▲)**과 **`AVG -3.5ms`** 텍스트로 표시.
  * 내가 평균적으로 서둘러서 치는지(Fast/Early), 늦게 치는지(Slow/Late) 한눈에 직관적으로 파악.
* **🎨 단계별 잔상 페이드아웃 (Fade-out)**
  * 눈금자에 남는 마커가 오래될수록 **순백색(100%) → 연회색 → 어두운 회색(20%)**으로 서서히 어두워지며 자연스럽게 소멸.
* **🖱️ Ctrl + 마우스 드래그로 자유로운 위치 이동**
  * 평소에는 게임 클릭을 방해하지 않는 완전 클릭 관통(`WS_EX_TRANSPARENT`) 상태 유지.
  * **`Ctrl` 키를 누른 채 마우스로 드래그**하면 원하는 위치로 즉시 이동되며, 마우스를 놓으면 `config.ini`에 **좌표 자동 영구 저장**.
* **⌨️ 콘솔 실시간 모드 전환**
  * 프로그램을 껐다 켤 필요 없이, 콘솔 창에 **`4`**, **`5`**, **`6`**을 입력하고 Enter를 치면 4B / 5B / 6B 키 모드가 즉각 전환 (설정 파일 자동 반영).
* **🛡️ 100% 안티치트 안전 (Passive Capture)**
  * 게임 프로세스 메모리를 건드리지 않는 외부 캡처 방식(XIGNCODE / EAC 밴 위험 0%).
  * Windows `WDA_EXCLUDEFROMCAPTURE` 기술을 적용하여, **오버레이 눈금자가 게임 화면을 캡처할 때는 투명인간처럼 제외**되므로 눈금자 뒤의 노트를 절대 가리지 않음.

---

## 🎮 필수 게임 설정 (In-Game Settings)

정확한 노트 템플릿 매칭 및 캡처를 위해 게임 내 옵션을 반드시 아래와 같이 맞춰주세요.

| 옵션 항목 | 권장 설정값 | 이유 |
|---|---|---|
| **화면 모드** | **전체 창 모드** (Borderless) | 전체화면 시 오버레이가 가려지는 현상 방지 |
| **기어 위치** | **센터 기어 (Center)** | 중앙 캡처 ROI 영역(x: 720~1200) 고정 |
| **레인 불투명도** | **100%** | BGA 영상 비침으로 인한 템플릿 인식 실패 방지 |
| **노트 스킨** | **GROOVE COASTER (그루브 코스터)** | 템플릿 매칭에 사용되는 원형 캡 규격 스킨 |
| **트리거/사이드** | **기본 색상** | HSV 색상 인식 범위 표준 |

---

## 🛠️ 조작 및 단축키 (Controls)

* **실시간 모드 변경**: 콘솔 창에 `4`, `5`, `6` 입력 후 **Enter**
* **오버레이 위치 이동**: **`Ctrl` 키를 누른 상태**에서 오버레이 눈금자를 마우스 왼쪽 버튼으로 잡고 드래그 (놓으면 `config.ini`에 자동 저장)
* **인식 검증 사진 저장 (스냅샷)**: 콘솔 창에 **`s`** 입력 후 **Enter**
  * 현재 프로그램이 보고 있는 화면과 노트에 초록색 십자가(`+`)가 찍힌 `debug_detected.png` 사진이 생성되며 윈도우 사진 뷰어로 즉시 열립니다.
* **프로그램 종료**: 콘솔 창에 **`q`** 입력 후 **Enter**

---

## ⚙️ 설정 파일 (`config.ini`)

실행 파일(`.exe`)과 같은 경로에 있는 `config.ini`를 메모장으로 열어 컴파일 없이 언제든지 세부 수치를 조절할 수 있습니다.

```ini
[general]
fps=24                      # 화면 캡처 빈도 (컴터가 좋으면 높일수록 판정 정확해지긴 함)
globalOffset=-30            # 지연 보정 오프셋 (판정 오프셋 설정하는건데 42ms 넘어서 얼리,레이트가 떴는데도 디맥에서 100% 면 알아서 맞춰서 보정하십쇼)
maxWindow=150               # 판정 허용 범위 (ms)
mode=4                      # 기본 시작 모드 (4, 5, 6)
judgeY=750                  # 디맥 1080p 기준 판정선 Y좌표
noteThreshold=0.75          # 노트 인식 민감도 (기본 0.75, 낮출수록 관대함. 너무 낮으면 프로그램이 헛 것을 보고 노트가 없는데 판정을 할 수도 있음.)
maxDetectY=610              # 판정 이펙트 간섭을 피하기 위한 최대 검출 높이
maxMarkers=30               # 눈금자에 동시에 남길 최근 판정 마커 잔상 개수
avgSamples=60               # 황금 삼각형(평균 오차) 산출에 반영할 최근 타격 횟수
triggerColor=default        # 트리거 키 색상 (색맹, 색약으로 변경된 색상은 RGB 색상 추출해서 변경하십쇼)
sideColor=default           # 사이드 키 색상 (마찬가지)
showAVG=false               # 평균 ms 우측 상단에 표시 true 면 표시 false 면 미표시
showEarlyLate=true          # 위에 Early 0ms Late 0ms 표시
perfectRangeColor=0,255,120 # 퍼펙트 판정 범위 색상 지금 이건 초록색 일 듯 아마도
overlayX=809                # 눈금자 띄울 x좌표
overlayY=199                # 눈금자 띄울 y좌표

[4LANE]
1=s
2=d
3=l
4=;
L=Space
R=RAlt
A=LShift
B=RShift

# 5LANE, 6LANE 키 매핑도 동일하게 지정 가능 (세미콜론 ';' 및 따옴표 '\'' 지원)
```


# * 100% 의 정확한 판정을 보장하지 않습니다. 보조적인 참고용으로만 사용하시기 바랍니다.
