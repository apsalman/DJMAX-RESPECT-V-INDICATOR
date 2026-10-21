#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#pragma warning(push, 0)
#include <opencv2/opencv.hpp>
#pragma warning(pop)
#include <iostream>
#include <algorithm>
#include <cstdint>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// DXGI Desktop Duplication capture with the actual frame presentation timestamp.
// DXCam-CPP's documented C++ API returns cv::Mat but does not expose the DXGI
// LastPresentTime timestamp, which is important for timing measurements.
class ScreenCapture {
public:
    int x = 720, y = 0, width = 480, height = 900;

    bool init(int targetX = 720, int targetY = 0, int w = 480, int h = 900) {
        x = targetX; y = targetY; width = w; height = h;
        shutdown();

        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory_));
        if (FAILED(hr)) return fail("CreateDXGIFactory1", hr);

        // Select the DXGI output containing the center of the requested ROI.
        const LONG cx = x + width / 2;
        const LONG cy = y + height / 2;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> selectedAdapter;
        Microsoft::WRL::ComPtr<IDXGIOutput1> selectedOutput;
        RECT selectedDesktop{};
        bool found = false;

        for (UINT ai = 0; !found; ++ai) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            if (factory_->EnumAdapters1(ai, &adapter) == DXGI_ERROR_NOT_FOUND) break;
            for (UINT oi = 0; ; ++oi) {
                Microsoft::WRL::ComPtr<IDXGIOutput> output;
                if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND) break;
                DXGI_OUTPUT_DESC desc{};
                if (FAILED(output->GetDesc(&desc))) continue;
                if (cx >= desc.DesktopCoordinates.left && cx < desc.DesktopCoordinates.right &&
                    cy >= desc.DesktopCoordinates.top && cy < desc.DesktopCoordinates.bottom) {
                    if (FAILED(output.As(&selectedOutput))) continue;
                    selectedAdapter = adapter;
                    selectedDesktop = desc.DesktopCoordinates;
                    found = true;
                    break;
                }
            }
        }

        if (!found || selectedAdapter.Get() == nullptr || selectedOutput.Get() == nullptr) {
            return fail("Could not find a monitor containing the capture region", E_FAIL);
        }

        // Convert virtual-desktop coordinates into coordinates local to this output.
        localX_ = x - selectedDesktop.left;
        localY_ = y - selectedDesktop.top;
        if (localX_ < 0 || localY_ < 0 ||
            localX_ + width > selectedDesktop.right - selectedDesktop.left ||
            localY_ + height > selectedDesktop.bottom - selectedDesktop.top) {
            return fail("Capture ROI is outside the selected monitor bounds", E_INVALIDARG);
        }

        D3D_FEATURE_LEVEL featureLevel{};
        const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
        };
        hr = D3D11CreateDevice(
            selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels),
            D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
        if (hr == E_INVALIDARG) {
            // Older Windows SDK/runtime combinations may not accept 11_1 in the list.
            hr = D3D11CreateDevice(
                selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels + 1, ARRAYSIZE(levels) - 1,
                D3D11_SDK_VERSION, &device_, &featureLevel, &context_);
        }
        if (FAILED(hr)) return fail("D3D11CreateDevice", hr);

        hr = selectedOutput->DuplicateOutput(device_.Get(), &duplication_);
        if (FAILED(hr)) return fail("IDXGIOutput1::DuplicateOutput", hr);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = static_cast<UINT>(width);
        td.Height = static_cast<UINT>(height);
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr = device_->CreateTexture2D(&td, nullptr, &staging_);
        if (FAILED(hr)) return fail("CreateTexture2D(staging)", hr);

        std::cout << "[CAPTURE] DXGI Desktop Duplication 활성화됨. ROI="
                  << x << "," << y << " " << width << "x" << height << std::endl;
        return true;
    }

    // Returns true only when a new desktop frame was acquired and copied.
    // frameTimeMs is the frame's DXGI LastPresentTime converted to QPC-relative ms.
    bool capture(cv::Mat& outRoi, double& frameTimeMs,
                 const LARGE_INTEGER& qpcOrigin, const LARGE_INTEGER& qpcFrequency) {
        if (duplication_.Get() == nullptr || context_.Get() == nullptr || staging_.Get() == nullptr || qpcFrequency.QuadPart <= 0) return false;

        DXGI_OUTDUPL_FRAME_INFO frameInfo{};
        Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
        HRESULT hr = duplication_->AcquireNextFrame(100, &frameInfo, &desktopResource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            std::cerr << "[CAPTURE] DXGI access lost; capture needs reinitialization.\n";
            return false;
        }
        if (FAILED(hr)) {
            std::cerr << "[CAPTURE] AcquireNextFrame failed: 0x" << std::hex
                      << static_cast<unsigned long>(hr) << std::dec << std::endl;
            return false;
        }

        bool ok = false;
        do {
            // LastPresentTime is expressed in the same QPC clock domain as QueryPerformanceCounter.
            // A zero value means this update did not contain a new desktop image.
            if (frameInfo.LastPresentTime.QuadPart == 0) break;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> desktopTexture;
            hr = desktopResource.As(&desktopTexture);
            if (FAILED(hr)) break;

            D3D11_BOX srcBox{};
            srcBox.left = static_cast<UINT>(localX_);
            srcBox.top = static_cast<UINT>(localY_);
            srcBox.front = 0;
            srcBox.right = static_cast<UINT>(localX_ + width);
            srcBox.bottom = static_cast<UINT>(localY_ + height);
            srcBox.back = 1;
            context_->CopySubresourceRegion(staging_.Get(), 0, 0, 0, 0,
                                            desktopTexture.Get(), 0, &srcBox);

            D3D11_MAPPED_SUBRESOURCE mapped{};
            hr = context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(hr)) break;

            cv::Mat bgra(height, width, CV_8UC4, mapped.pData, mapped.RowPitch);
            cv::cvtColor(bgra, outRoi, cv::COLOR_BGRA2BGR);
            context_->Unmap(staging_.Get(), 0);

            frameTimeMs = static_cast<double>(frameInfo.LastPresentTime.QuadPart - qpcOrigin.QuadPart)
                        * 1000.0 / static_cast<double>(qpcFrequency.QuadPart);
            ok = true;
        } while (false);

        duplication_->ReleaseFrame();
        return ok;
    }

    void shutdown() noexcept {
        staging_.Reset();
        duplication_.Reset();
        context_.Reset();
        device_.Reset();
        factory_.Reset();
    }

    ~ScreenCapture() { shutdown(); }

private:
    int localX_ = 0, localY_ = 0;
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_;

    static bool fail(const char* what, HRESULT hr) {
        std::cerr << "[CAPTURE-ERR] " << what << " failed (0x" << std::hex
                  << static_cast<unsigned long>(hr) << std::dec << ")\n";
        return false;
    }
};
