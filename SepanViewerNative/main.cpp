#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "Config.hpp"
#include "Engine.hpp"
#include "Vision.hpp"
#include "Overlay.hpp"
#include "Capture.hpp"

std::atomic<bool> g_running = true;
std::atomic<int> g_activeMode = 4;
std::atomic<bool> g_saveSnapshot = false;
std::atomic<bool> g_captureFailed = false;
NativeOverlayWindow* g_overlay = nullptr;

double getQpcMs(LARGE_INTEGER origin, LARGE_INTEGER freq) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart - origin.QuadPart) * 1000.0 / freq.QuadPart;
}

std::string findLane(const Config& cfg, int mode, int vk) {
    auto itMode = cfg.keyBindings.find(mode);
    if (itMode == cfg.keyBindings.end()) return "";
    for (const auto& [lane, keyVk] : itMode->second) {
        if (keyVk == vk) return lane;
    }
    return "";
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HINSTANCE hInst = GetModuleHandle(nullptr);
    Config cfg = Config::load("config.ini");

    if (cfg.mode == "4" || cfg.mode == "5" || cfg.mode == "6") {
        g_activeMode = std::stoi(cfg.mode);
    }
    else {
        g_activeMode = 4;
    }

    LARGE_INTEGER freq, origin;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&origin);

    TimingEngine engine(cfg.globalOffset, cfg.maxWindow, cfg.judgeY, cfg.fps);
    NoteDetector detector(cfg.noteThreshold, cfg.maxDetectY);

    NativeOverlayWindow overlay;
    g_overlay = &overlay;
    // [업데이트] maxMarkers, avgSamples 설정값을 함께 전달
    overlay.init(hInst, cfg.overlayX, cfg.overlayY, cfg.maxMarkers, cfg.avgSamples);
    overlay.showAvg = cfg.showAvg;
    overlay.showEarlyLate = cfg.showEarlyLate;
    overlay.capture = cfg.capture;
    SetWindowDisplayAffinity(overlay.hwnd, cfg.capture ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE);

    FastSlowOverlayWindow countOverlay;
    countOverlay.init(hInst, cfg.fastSlowY, cfg.showFastSlowCount, cfg.capture, cfg.fastSlowThresholdMs);
    countOverlay.setMode(g_activeMode.load());
    overlay.perfectRangeR = cfg.perfectRangeR;
    overlay.perfectRangeG = cfg.perfectRangeG;
    overlay.perfectRangeB = cfg.perfectRangeB;

    std::cout << "\n======================================================\n";
    std::cout << "[INFO] SepanViewer Native C++ 가동 완료!\n";
    std::cout << "  - 현재 모드: " << g_activeMode << "B\n";
    std::cout << "  - [모드 변경] 콘솔에 4, 5, 6 입력 후 Enter\n";
    std::cout << "  - [검증 사진 저장] 콘솔에 's' 입력 후 Enter\n";
    std::cout << "  - [위치 이동] Ctrl 키 누른 상태에서 마우스 드래그\n";
    std::cout << "======================================================\n\n";

    // 1. 콘솔 실시간 모드 입력 스레드
    std::thread consoleInputThread([&]() {
        std::string input;
        while (g_running) {
            if (std::cin >> input) {
                if (input == "4" || input == "5" || input == "6") {
                    int m = std::stoi(input);
                    g_activeMode = m;
                    engine.reset(m);
                    overlay.clearHistory();
                    countOverlay.setMode(m);
                    Config::saveMode(input);
                    std::cout << "\n>>> [모드 전환] " << m << "B 로 변경되었습니다!\n\n";
                }
                else if (input == "s" || input == "snap") {
                    g_saveSnapshot = true;
                    std::cout << "\n>>> [스냅샷 요청] 다음 화면 프레임을 캡처하여 사진을 저장합니다...\n";
                }
                else if (input == "q" || input == "quit") {
                    g_running = false;
                    PostQuitMessage(0);
                    break;
                }
            }
        }
        });

    // 2. 오버레이 렌더링 스레드
    std::thread renderThread([&]() {
        while (g_running) {
            bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            overlay.setInteractive(ctrlPressed);
            overlay.render();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        });

    // 3. 화면 캡처 및 비전 스레드
    std::thread captureThread([&]() {
        ScreenCapture cap;
        int roiX = 720, roiY = 0;

        HWND hwndGame = FindWindowW(nullptr, L"DJMAX RESPECT V");
        if (hwndGame) {
            RECT clientRc;
            GetClientRect(hwndGame, &clientRc);
            POINT pt = { 0, 0 };
            ClientToScreen(hwndGame, &pt);

            int winW = clientRc.right;
            roiX = pt.x + (winW * 720 / 1920);
            roiY = pt.y;
            std::cout << "[CAPTURE] DJMAX 게임 화면 감지: (" << pt.x << ", " << pt.y << ") -> 기어 영역: (" << roiX << ", " << roiY << ")\n";
        }
        else {
            std::cout << "[CAPTURE] DJMAX 창 미감지, 기본 좌표(720, 0)에서 캡처합니다.\n";
        }

        if (!cap.init(roiX, roiY, 480, 900)) {
            std::cerr << "[CAPTURE-ERR] DXGI 캡처 초기화 실패. 위의 오류 코드(HRESULT)를 확인하세요.\n";
            g_captureFailed = true;
            g_running = false;
            if (overlay.hwnd) PostMessageW(overlay.hwnd, WM_CLOSE, 0, 0);
            return;
        }

        int targetFps = cfg.fps > 0 ? cfg.fps : 12;
        int frameIntervalMs = 1000 / targetFps;
        cv::Mat roi;

        while (g_running) {
            auto frameStart = std::chrono::steady_clock::now();
            int mode = g_activeMode.load();

            double frameTime = 0.0;
            if (cap.capture(roi, frameTime, origin, freq)) {
                if (mode > 0) {
                    auto detections = detector.detect(roi, mode);
                    engine.onFrame(frameTime, mode, detections);

                    if (g_saveSnapshot.exchange(false)) {
                        try {
                            cv::Mat annotated = roi.clone();
                            cv::line(annotated, cv::Point(0, 750), cv::Point(479, 750), cv::Scalar(0, 255, 255), 2);

                            for (const auto& d : detections) {
                                int x = 240;
                                if (d.lane == "A" || d.lane == "L") x = 120;
                                else if (d.lane == "B" || d.lane == "R") x = 360;
                                else {
                                    try {
                                        int laneNum = std::stoi(d.lane);
                                        x = static_cast<int>(std::round(480.0 * (laneNum - 0.5) / mode));
                                    }
                                    catch (...) {
                                        x = 240;
                                    }
                                }

                                cv::drawMarker(annotated, cv::Point(x, static_cast<int>(d.y)), cv::Scalar(0, 255, 0), cv::MARKER_CROSS, 16, 2);
                                cv::putText(annotated, d.lane + " (" + std::to_string(static_cast<int>(d.y)) + ")",
                                    cv::Point(std::max(0, x - 25), static_cast<int>(d.y) - 8),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 0), 1);
                            }

                            fs::path savePath = Config::getExeDirectory() / "debug_detected.png";
                            cv::imwrite(savePath.string(), annotated);

                            std::cout << "\n[SNAPSHOT 완료] 실제 캡처 이미지가 저장되었습니다!\n";
                            std::cout << "  -> 저장 위치: " << savePath.string() << "\n\n";
                            ShellExecuteW(nullptr, L"open", savePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

                        }
                        catch (const std::exception& e) {
                            std::cout << "[SNAPSHOT-ERR] 저장 중 예외: " << e.what() << "\n";
                        }
                    }
                }
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - frameStart).count();
            if (elapsed < frameIntervalMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(frameIntervalMs - elapsed));
            }
        }
        });

    // 4. Raw Input 등록
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = overlay.hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    // 메인 메시지 루프
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_INPUT) {
            HRAWINPUT hRaw = reinterpret_cast<HRAWINPUT>(msg.lParam);
            UINT size = 0;
            GetRawInputData(hRaw, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
            std::vector<BYTE> buf(size);
            if (GetRawInputData(hRaw, RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
                RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buf.data());
                if (raw->header.dwType == RIM_TYPEKEYBOARD && !(raw->data.keyboard.Flags & RI_KEY_BREAK)) {
                    double t = getQpcMs(origin, freq);
                    int vk = raw->data.keyboard.VKey;
                    int mode = g_activeMode.load();

                    std::string lane = findLane(cfg, mode, vk);
                    if (!lane.empty()) {
                        auto res = engine.onPress(t, lane);
                        if (res && res->error_ms().has_value()) {
                            int err = res->error_ms().value();
                            overlay.addMarker(res->lane, err);
                            countOverlay.add(res->lane, err);
                            std::cout << "[HIT] LANE " << res->lane << " | "
                                << (err < 0 ? "EARLY " : "LATE ") << std::abs(err) << "ms\n";
                        }
                    }
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_running = false;
    if (renderThread.joinable()) renderThread.join();
    if (captureThread.joinable()) captureThread.join();
    if (consoleInputThread.joinable()) consoleInputThread.detach();

    // Keep the console open after DXGI initialization failure so the HRESULT is readable.
    if (g_captureFailed.load()) {
        std::cerr << "\n[DEBUG] 위의 [CAPTURE-ERR] 메시지를 복사해 보내 주세요.\n";
        std::system("pause");
    }

    return 0;
}