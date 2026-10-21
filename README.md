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

기존의 **Python + OBS Studio 브라우저 소스** 방식은 방송 송출 화면을 통해서만 확인할 수 있어 플레이어가 게임 플레이 도중 실시간으로 타점을 확인하기 어렵다는 한계가 있었습니다. 이를 해결하기 위해 **Google AI와의 협업을 통해 완전히 독립적인 C++ 네이티브 프로그램으로 전환**하였으며, 화면 캡처 파이프라인을 **Windows 네이티브 DXGI Desktop Duplication API**로 직접 재설계하여 최상의 반응성과 정밀도를 달성했습니다.

---
