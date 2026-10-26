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
#include <map>
#include <algorithm>
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
    bool capture = true;

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
        SetWindowDisplayAffinity(hwnd, capture ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE);
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

// Separate, click-through per-lane Fast/Slow visual indicator overlay.
enum class FastSlowType {
    None,
    Fast, // Blue Up Arrow (파란 위 화살표)
    Slow  // Red Down Arrow (빨간 아래 화살표)
};

struct LaneIndicatorState {
    FastSlowType type = FastSlowType::None;
    std::chrono::steady_clock::time_point hitTime{};
};

class FastSlowOverlayWindow {
public:
    HWND hwnd = nullptr;
    int y = 666;
    int width = 0;
    int height = 64;
    bool enabled = true;
    int thresholdMs = 0;
    int mode = 4;

    static constexpr int FADE_DURATION_MS = 380; // Total display & fadeout time in ms

private:
    std::mutex mtx;
    LaneIndicatorState lanes_[6]; // up to 6 lanes (0 to 5)
    bool hasDrawn_ = false;

    HDC hdcScreen_ = nullptr;
    HDC hdcMem_ = nullptr;
    HBITMAP hBmp_ = nullptr;
    HGDIOBJ oldBmp_ = nullptr;
    void* pBits_ = nullptr;

public:
    static LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_DESTROY) return 0;
        return DefWindowProcW(h, msg, wp, lp);
    }

    void init(HINSTANCE inst, int yPos, bool show, bool capture, int threshold = 0) {
        y = yPos;
        enabled = show;
        thresholdMs = (threshold < 0 ? 0 : threshold);
        width = GetSystemMetrics(SM_CXSCREEN);
        height = 64;

        WNDCLASSW wc{};
        wc.lpfnWndProc = Proc;
        wc.hInstance = inst;
        wc.lpszClassName = L"SepanFastSlowIndicatorClass";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);

        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Sepan Fast Slow Indicator", WS_POPUP,
            0, y, width, height, nullptr, nullptr, inst, nullptr);

        if (!hwnd) return;

        SetWindowDisplayAffinity(hwnd, capture ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE);

        // 32-bit PARGB DIB for per-pixel alpha transparency via UpdateLayeredWindow
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = width;
        bi.bmiHeader.biHeight = -height; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        hdcScreen_ = GetDC(nullptr);
        hdcMem_ = CreateCompatibleDC(hdcScreen_);
        hBmp_ = CreateDIBSection(hdcMem_, &bi, DIB_RGB_COLORS, &pBits_, nullptr, 0);
        if (hBmp_ && hdcMem_) {
            oldBmp_ = SelectObject(hdcMem_, hBmp_);
        }

        clearSurface();

        if (enabled) {
            ShowWindow(hwnd, SW_SHOWNA);
        }
    }

    void setMode(int m) {
        std::lock_guard<std::mutex> l(mtx);
        mode = m;
        for (int i = 0; i < 6; ++i) {
            lanes_[i] = {};
        }
        hasDrawn_ = true; // Clear on next render frame
    }

    void add(const std::string& lane, int err) {
        // 5-key mode splits the center lane into 3a/3b bindings; display both under lane 3.
        const std::string displayLane = (lane == "3a" || lane == "3b") ? "3" : lane;
        int laneIdx = -1;
        try {
            laneIdx = std::stoi(displayLane) - 1;
        } catch (...) {
            return;
        }

        if (laneIdx < 0 || laneIdx >= 6) return;

        FastSlowType hitType = FastSlowType::None;
        if (err < 0 && -static_cast<long long>(err) > thresholdMs) {
            hitType = FastSlowType::Fast; // Fast -> Blue Up Arrow
        } else if (err > 0 && err > thresholdMs) {
            hitType = FastSlowType::Slow; // Slow -> Red Down Arrow
        }

        if (hitType != FastSlowType::None) {
            std::lock_guard<std::mutex> l(mtx);
            lanes_[laneIdx].type = hitType;
            lanes_[laneIdx].hitTime = std::chrono::steady_clock::now();
        }
    }

    void render() {
        if (!hwnd || !enabled || !pBits_ || !hdcMem_ || !hdcScreen_) return;

        const auto now = std::chrono::steady_clock::now();
        const int x4[] = { 784, 904, 1024, 1144 };
        const int x5[] = { 769, 864, 960, 1056, 1153 };
        const int x6[] = { 758, 840, 919, 1000, 1079, 1160 };

        bool anyActive = false;
        int nLanes = (mode == 4 || mode == 5 || mode == 6) ? mode : 4;
        const int* xs = (mode == 4 ? x4 : (mode == 5 ? x5 : x6));

        struct ActiveIndicator {
            int cx;
            float curY;
            int alpha;
            FastSlowType type;
        };
        std::vector<ActiveIndicator> activeList;

        {
            std::lock_guard<std::mutex> l(mtx);
            for (int i = 0; i < nLanes; ++i) {
                if (lanes_[i].type == FastSlowType::None) continue;

                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lanes_[i].hitTime).count();
                if (elapsedMs < FADE_DURATION_MS) {
                    anyActive = true;
                    float progress = static_cast<float>(elapsedMs) / static_cast<float>(FADE_DURATION_MS);

                    // Maintain full opacity for the first 80ms, then smoothly fade out to 0
                    float alphaFactor = 1.0f;
                    if (elapsedMs > 80) {
                        alphaFactor = 1.0f - static_cast<float>(elapsedMs - 80) / static_cast<float>(FADE_DURATION_MS - 80);
                    }
                    int alpha = std::clamp(static_cast<int>(255.0f * alphaFactor), 0, 255);

                    // Subtle directional float: Fast floats up (-7px), Slow floats down (+7px)
                    float drift = (lanes_[i].type == FastSlowType::Fast) ? (-7.0f * progress) : (+7.0f * progress);
                    float cy = (height / 2.0f) + drift;

                    activeList.push_back({ xs[i], cy, alpha, lanes_[i].type });
                } else {
                    lanes_[i].type = FastSlowType::None; // Expired
                }
            }
        }

        if (!anyActive) {
            if (hasDrawn_) {
                clearSurface();
                hasDrawn_ = false;
            }
            return;
        }

        hasDrawn_ = true;

        // Render directly into 32-bit PARGB bitmap for per-pixel alpha blending
        Gdiplus::Bitmap bmp(width, height, width * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(pBits_));
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        for (const auto& ind : activeList) {
            if (ind.alpha <= 0) continue;

            Gdiplus::Color fillColor;
            Gdiplus::PointF pts[7];

            if (ind.type == FastSlowType::Fast) {
                // Blue Up Arrow (파란 위 화살표)
                fillColor = Gdiplus::Color(ind.alpha, 45, 160, 255);

                pts[0] = Gdiplus::PointF(static_cast<float>(ind.cx), ind.curY - 13.0f);      // Top tip
                pts[1] = Gdiplus::PointF(static_cast<float>(ind.cx + 14), ind.curY - 1.0f);  // Right wing
                pts[2] = Gdiplus::PointF(static_cast<float>(ind.cx + 6), ind.curY - 1.0f);   // Right inner neck
                pts[3] = Gdiplus::PointF(static_cast<float>(ind.cx + 6), ind.curY + 12.0f);  // Right bottom
                pts[4] = Gdiplus::PointF(static_cast<float>(ind.cx - 6), ind.curY + 12.0f);  // Left bottom
                pts[5] = Gdiplus::PointF(static_cast<float>(ind.cx - 6), ind.curY - 1.0f);   // Left inner neck
                pts[6] = Gdiplus::PointF(static_cast<float>(ind.cx - 14), ind.curY - 1.0f);  // Left wing
            } else {
                // Red Down Arrow (빨간 아래 화살표)
                fillColor = Gdiplus::Color(ind.alpha, 255, 60, 60);

                pts[0] = Gdiplus::PointF(static_cast<float>(ind.cx), ind.curY + 13.0f);      // Bottom tip
                pts[1] = Gdiplus::PointF(static_cast<float>(ind.cx + 14), ind.curY + 1.0f);  // Right wing
                pts[2] = Gdiplus::PointF(static_cast<float>(ind.cx + 6), ind.curY + 1.0f);   // Right inner neck
                pts[3] = Gdiplus::PointF(static_cast<float>(ind.cx + 6), ind.curY - 12.0f);  // Right top
                pts[4] = Gdiplus::PointF(static_cast<float>(ind.cx - 6), ind.curY - 12.0f);  // Left top
                pts[5] = Gdiplus::PointF(static_cast<float>(ind.cx - 6), ind.curY + 1.0f);   // Left inner neck
                pts[6] = Gdiplus::PointF(static_cast<float>(ind.cx - 14), ind.curY + 1.0f);  // Left wing
            }

            Gdiplus::SolidBrush fillBrush(fillColor);
            Gdiplus::Pen outlinePen(Gdiplus::Color(ind.alpha, 0, 0, 0), 2.0f);
            outlinePen.SetLineJoin(Gdiplus::LineJoinRound);

            g.FillPolygon(&fillBrush, pts, 7);
            g.DrawPolygon(&outlinePen, pts, 7);
        }

        POINT ptSrc{ 0, 0 };
        SIZE sizeWnd{ width, height };
        POINT ptDst{ 0, y };
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(hwnd, hdcScreen_, &ptDst, &sizeWnd, hdcMem_, &ptSrc, 0, &bf, ULW_ALPHA);
    }

    void clearSurface() {
        if (!hwnd || !pBits_ || !hdcMem_ || !hdcScreen_) return;
        Gdiplus::Bitmap bmp(width, height, width * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(pBits_));
        Gdiplus::Graphics g(&bmp);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        POINT ptSrc{ 0, 0 };
        SIZE sizeWnd{ width, height };
        POINT ptDst{ 0, y };
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(hwnd, hdcScreen_, &ptDst, &sizeWnd, hdcMem_, &ptSrc, 0, &bf, ULW_ALPHA);
    }

    ~FastSlowOverlayWindow() {
        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
        if (hdcMem_) {
            if (oldBmp_) SelectObject(hdcMem_, oldBmp_);
            DeleteDC(hdcMem_);
            hdcMem_ = nullptr;
        }
        if (hBmp_) {
            DeleteObject(hBmp_);
            hBmp_ = nullptr;
        }
        if (hdcScreen_) {
            ReleaseDC(nullptr, hdcScreen_);
            hdcScreen_ = nullptr;
        }
    }
};
