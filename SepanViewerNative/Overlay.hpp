#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>
#include <deque>
#include <string>
#include <mutex>
#include <chrono>
#include <iostream>
#include <cmath>
#include <vector>
#include "Config.hpp"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

struct Marker {
    std::string lane = "";
    int error = 0;
    std::chrono::steady_clock::time_point timestamp{};
};

class NativeOverlayWindow;
extern NativeOverlayWindow* g_overlay;

class NativeOverlayWindow {
public:
    HWND hwnd = nullptr;
    int posX = 810, posY = 755;
    int width = 301, height = 54;
    std::deque<Marker> markers;
    std::deque<int> errorHistory;
    size_t maxMarkers = 5;
    size_t maxAvgSamples = 40;
    std::mutex mtx;
    bool isInteractive = false;
    bool showAvg = true;
    bool showEarlyLate = true;
    int perfectRangeR = 0;
    int perfectRangeG = 255;
    int perfectRangeB = 120;

    static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_LBUTTONDOWN) {
            ReleaseCapture();
            SendMessage(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);

            if (g_overlay) {
                RECT rc;
                GetWindowRect(h, &rc);
                g_overlay->posX = rc.left;
                g_overlay->posY = rc.top;
                std::cout << "[OVERLAY] 새 좌표 저장 완료: (" << g_overlay->posX << ", " << g_overlay->posY << ")\n";
                Config::saveOverlayPosition(g_overlay->posX, g_overlay->posY);
            }
            return 0;
        }
        else if (msg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(h, msg, wp, lp);
    }

    void init(HINSTANCE hInst, int initX = -1, int initY = -1, int cfgMaxMarkers = 5, int cfgAvgSamples = 40) {
        if (initX >= 0) posX = initX;
        if (initY >= 0) posY = initY;
        maxMarkers = static_cast<size_t>((cfgMaxMarkers > 1) ? cfgMaxMarkers : 1);
        maxAvgSamples = static_cast<size_t>((cfgAvgSamples > 1) ? cfgAvgSamples : 1);

        Gdiplus::GdiplusStartupInput gdiSI;
        ULONG_PTR gdiToken;
        Gdiplus::GdiplusStartup(&gdiToken, &gdiSI, nullptr);

        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"SepanNativeOverlayClass";
        wc.hCursor = LoadCursor(nullptr, IDC_SIZEALL);
        RegisterClass(&wc);

        hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Sepan Overlay", WS_POPUP,
            posX, posY, width, height, nullptr, nullptr, hInst, nullptr
        );

        SetLayeredWindowAttributes(hwnd, RGB(1, 2, 3), 0, LWA_COLORKEY);
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
        ShowWindow(hwnd, SW_SHOW);
    }

    void setInteractive(bool enable) {
        if (!hwnd || isInteractive == enable) return;
        isInteractive = enable;
        LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        if (enable) {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
        }
        else {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
        }
    }

    void addMarker(const std::string& lane, int err) {
        std::lock_guard<std::mutex> lock(mtx);
        markers.push_back({ lane, err, std::chrono::steady_clock::now() });
        if (markers.size() > maxMarkers) markers.pop_front();

        errorHistory.push_back(err);
        if (errorHistory.size() > maxAvgSamples) errorHistory.pop_front();
    }

    void clearHistory() {
        std::lock_guard<std::mutex> lock(mtx);
        markers.clear();
        errorHistory.clear();
    }

    void render() {
        if (!hwnd) return;
        HDC hdc = GetDC(hwnd);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, width, height);
        SelectObject(memDC, memBM);

        Gdiplus::Graphics g(memDC);
        g.Clear(Gdiplus::Color(1, 2, 3));

        int centerX = width / 2;
        int axisY = 36;

        if (isInteractive) {
            Gdiplus::Pen cyanBorder(Gdiplus::Color(255, 0, 255, 200), 2.0f);
            g.DrawRectangle(&cyanBorder, 1, 1, width - 2, height - 2);

            Gdiplus::Font dragFont(L"Consolas", 8, Gdiplus::FontStyleBold);
            Gdiplus::SolidBrush cyanBrush(Gdiplus::Color(255, 0, 255, 200));
            Gdiplus::PointF ptd(centerX - 42.0f, 2.0f);
            g.DrawString(L"[DRAG TO MOVE]", -1, &dragFont, ptd, &cyanBrush);
        }

        // 눈금선
        Gdiplus::Pen grayPen(Gdiplus::Color(255, 100, 100, 100), 1.0f);
        g.DrawLine(&grayPen, 0, axisY, width, axisY);

        for (int x = centerX % 10; x < width; x += 10) {
            g.DrawLine(&grayPen, x, axisY - 3, x, axisY + 4);
        }

        // ±42ms 퍼펙트 범위 경계선: config.ini에서 RGB 색상을 변경할 수 있음
        Gdiplus::Pen perfectRangePen(
            Gdiplus::Color(255, perfectRangeR, perfectRangeG, perfectRangeB), 2.5f);
        const int earlyBoundaryX = centerX - 42;
        const int lateBoundaryX = centerX + 42;
        g.DrawLine(&perfectRangePen, earlyBoundaryX, axisY - 9, earlyBoundaryX, axisY + 9);
        g.DrawLine(&perfectRangePen, lateBoundaryX, axisY - 9, lateBoundaryX, axisY + 9);

        // Just 선 (0ms)
        Gdiplus::Pen blackPen(Gdiplus::Color(255, 0, 0, 0), 3.0f);
        Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 1.0f);
        g.DrawLine(&blackPen, centerX, axisY - 12, centerX, axisY + 12);
        g.DrawLine(&whitePen, centerX, axisY - 12, centerX, axisY + 12);

        std::lock_guard<std::mutex> lock(mtx);
        Gdiplus::Font font(L"Consolas", 9, Gdiplus::FontStyleBold);
        Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::SolidBrush cyanBrush(Gdiplus::Color(255, 0, 255, 255));
        Gdiplus::SolidBrush blueBrush(Gdiplus::Color(255, 80, 160, 255));
        Gdiplus::SolidBrush redBrush(Gdiplus::Color(255, 255, 80, 80));

        // 개별 타격 마커 (사라지기 전 순차적으로 색이 연해지는 단계별 페이드아웃)
        size_t nMarkers = markers.size();
        for (size_t i = 0; i < nMarkers; ++i) {
            const auto& m = markers[i];
            int x = centerX + m.error;
            if (x >= 0 && x < width) {
                // 사라지기 전 남은 순서에 따른 비율 (20% ~ 100%)
                float progress = static_cast<float>(i + 1) / static_cast<float>(nMarkers);
                bool isLatest = (i == nMarkers - 1);

                // 색상 밝기 단계 조절: 65(연한 어두운 회색) ~ 255(선명한 순백색)
                int grayVal = static_cast<int>(65.0f + 190.0f * progress);
                if (grayVal > 255) grayVal = 255;

                // 최신 마커는 굵고 선명하게, 오래된 마커는 얇고 연하게
                float outlineWidth = isLatest ? 4.0f : 3.0f;
                float tickWidth = isLatest ? 2.0f : 1.0f;
                int tickHalfH = isLatest ? 10 : 8;

                // 검정 외곽선
                Gdiplus::Pen mBlack(Gdiplus::Color(255, 0, 0, 0), outlineWidth);
                g.DrawLine(&mBlack, x, axisY - tickHalfH, x, axisY + tickHalfH);

                // 단계별로 연해지는 틱 선
                Gdiplus::Pen mTick(Gdiplus::Color(255, grayVal, grayVal, grayVal), tickWidth);
                g.DrawLine(&mTick, x, axisY - tickHalfH, x, axisY + tickHalfH);
            }
        }

        // 평균 오차 황금 삼각형 및 AVG 텍스트 (config.ini에서 표시 여부 선택)
        if (showAvg && !errorHistory.empty()) {
            double sum = 0;
            for (int e : errorHistory) sum += e;
            double avgError = sum / errorHistory.size();

            int avgX = centerX + static_cast<int>(std::round(avgError));
            if (avgX < 4) avgX = 4;
            if (avgX > width - 5) avgX = width - 5;

            Gdiplus::Point trianglePts[3] = {
                Gdiplus::Point(avgX, axisY + 2),
                Gdiplus::Point(avgX - 4, axisY + 9),
                Gdiplus::Point(avgX + 4, axisY + 9)
            };

            Gdiplus::SolidBrush goldBrush(Gdiplus::Color(255, 255, 215, 0));
            Gdiplus::Pen blackOutline(Gdiplus::Color(255, 0, 0, 0), 1.0f);
            g.FillPolygon(&goldBrush, trianglePts, 3);
            g.DrawPolygon(&blackOutline, trianglePts, 3);

            if (!isInteractive) {
                Gdiplus::Font avgFont(L"Consolas", 8, Gdiplus::FontStyleBold);
                Gdiplus::SolidBrush avgTextBrush(Gdiplus::Color(255, 255, 220, 80));
                wchar_t avgBuf[32];
                swprintf_s(avgBuf, L"AVG %+.1fms", avgError);
                g.DrawString(avgBuf, -1, &avgFont, Gdiplus::PointF(width - 78.0f, 6.0f), &avgTextBrush);
            }
        }

        // 중앙 최신 판정 텍스트
        if (showEarlyLate && !isInteractive && !markers.empty()) {
            const auto& latest = markers.back();
            int diff = latest.error;
            std::wstring str = (diff < 0) ? L"EARLY " + std::to_wstring(std::abs(diff)) + L"ms"
                : (diff > 0) ? L"LATE " + std::to_wstring(diff) + L"ms"
                : L"PERFECT 0ms";

            Gdiplus::PointF pt(centerX - 35.0f, 6.0f);
            Gdiplus::Brush* b = &whiteBrush;
            if (std::abs(diff) <= 22) b = &cyanBrush;
            else if (diff < 0) b = &blueBrush;
            else b = &redBrush;

            g.DrawString(str.c_str(), -1, &font, pt, b);
        }

        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBM);
        DeleteDC(memDC);
        ReleaseDC(hwnd, hdc);
    }
};