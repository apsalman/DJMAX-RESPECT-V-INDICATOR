#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

inline int parseKeyName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    name.erase(0, name.find_first_not_of(" \t\r\n"));
    name.erase(name.find_last_not_of(" \t\r\n") + 1);

    if (name.empty()) return 0;

    if (name == ";" || name == "semicolon") return VK_OEM_1;
    if (name == "'" || name == "quote" || name == "apostrophe") return VK_OEM_7;
    if (name == "/" || name == "slash") return VK_OEM_2;
    if (name == "`" || name == "tilde") return VK_OEM_3;
    if (name == "[" || name == "lbracket") return VK_OEM_4;
    if (name == "\\" || name == "backslash") return VK_OEM_5;
    if (name == "]" || name == "rbracket") return VK_OEM_6;
    if (name == "," || name == "comma") return VK_OEM_COMMA;
    if (name == "." || name == "period" || name == "dot") return VK_OEM_PERIOD;
    if (name == "-" || name == "minus") return VK_OEM_MINUS;
    if (name == "=" || name == "equal" || name == "plus") return VK_OEM_PLUS;

    if (name.length() == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return 'A' + (c - 'a');
        if (c >= '0' && c <= '9') return '0' + (c - '0');
    }

    if (name == "hangeul" || name == "hangul") return VK_HANGUL;
    if (name == "hanja") return VK_HANJA;
    if (name == "right") return VK_RIGHT;
    if (name == "left") return VK_LEFT;
    if (name == "up") return VK_UP;
    if (name == "down") return VK_DOWN;
    if (name == "space") return VK_SPACE;
    if (name == "tab") return VK_TAB;
    if (name == "enter") return VK_RETURN;
    if (name == "ralt") return VK_RMENU;
    if (name == "lalt") return VK_LMENU;
    if (name == "rctrl") return VK_RCONTROL;
    if (name == "lctrl") return VK_LCONTROL;
    if (name == "rshift") return VK_RSHIFT;
    if (name == "lshift") return VK_LSHIFT;

    if (name == "numpad0") return VK_NUMPAD0;
    if (name == "numpad1") return VK_NUMPAD1;
    if (name == "numpad2") return VK_NUMPAD2;
    if (name == "numpad3") return VK_NUMPAD3;
    if (name == "numpad4") return VK_NUMPAD4;
    if (name == "numpad5") return VK_NUMPAD5;
    if (name == "numpad6") return VK_NUMPAD6;
    if (name == "numpad7") return VK_NUMPAD7;
    if (name == "numpad8") return VK_NUMPAD8;
    if (name == "numpad9") return VK_NUMPAD9;
    if (name == "numpadadd") return VK_ADD;
    if (name == "numpadsub") return VK_SUBTRACT;
    if (name == "numpadmult") return VK_MULTIPLY;
    if (name == "numpaddiv") return VK_DIVIDE;
    if (name == "numpaddot") return VK_DECIMAL;

    return 0;
}

struct Config {
    int fps = 12;
    double globalOffset = -5.0;
    double maxWindow = 150.0;
    double judgeY = 750.0;
    std::string mode = "4";
    double noteThreshold = 0.75;
    int maxDetectY = 610;
    int overlayX = -1;
    int overlayY = -1;
    int maxMarkers = 5;
    int avgSamples = 40;
    bool showAvg = true;
    bool showEarlyLate = true;
    bool showFastSlowCount = true;
    int fastSlowY = 666;
    int fastSlowThresholdMs = 0;
    bool capture = true;
    int perfectRangeR = 0;
    int perfectRangeG = 255;
    int perfectRangeB = 120;

    std::unordered_map<int, std::unordered_map<std::string, int>> keyBindings;

    static fs::path getExeDirectory() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return fs::path(buf).parent_path();
    }

    static fs::path getIniPath() {
        fs::path p = getExeDirectory() / "config.ini";
        if (fs::exists(p)) return fs::absolute(p);
        if (fs::exists("config.ini")) return fs::absolute(fs::path("config.ini"));
        return fs::absolute(p);
    }

    static void updateGeneralSetting(const std::string& key, const std::string& value) {
        fs::path iniPath = getIniPath();
        if (!fs::exists(iniPath)) return;

        std::ifstream in(iniPath);
        std::vector<std::string> lines;
        std::string line;
        bool keyUpdated = false;
        int generalSectionIndex = -1;
        int nextSectionIndex = -1;

        while (std::getline(in, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            if (!trimmed.empty()) {
                trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
            }

            if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
                std::string sec = trimmed.substr(1, trimmed.size() - 2);
                if (sec == "general") {
                    generalSectionIndex = static_cast<int>(lines.size());
                }
                else if (generalSectionIndex != -1 && nextSectionIndex == -1) {
                    nextSectionIndex = static_cast<int>(lines.size());
                }
            }

            if (generalSectionIndex != -1 && nextSectionIndex == -1) {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string k = line.substr(0, eq);
                    k.erase(0, k.find_first_not_of(" \t\r\n"));
                    k.erase(k.find_last_not_of(" \t\r\n") + 1);
                    if (k == key) {
                        line = key + "=" + value;
                        keyUpdated = true;
                    }
                }
            }
            lines.push_back(line);
        }
        in.close();

        if (!keyUpdated && generalSectionIndex != -1) {
            int insertPos = (nextSectionIndex != -1) ? nextSectionIndex : static_cast<int>(lines.size());
            lines.insert(lines.begin() + insertPos, key + "=" + value);
        }

        std::ofstream out(iniPath, std::ios::trunc | std::ios::binary);
        for (const auto& l : lines) {
            out << l << "\r\n";
        }
        out.close();

        std::cout << "[CONFIG] " << key << "=" << value << " 파일 저장 완료!\n";
    }

    static void saveOverlayPosition(int x, int y) {
        updateGeneralSetting("overlayX", std::to_string(x));
        updateGeneralSetting("overlayY", std::to_string(y));
    }

    static void saveMode(const std::string& mode) {
        updateGeneralSetting("mode", mode);
    }

    static Config load(const std::string& filename = "config.ini") {
        Config cfg;
        fs::path iniPath = getIniPath();

        std::ifstream file(iniPath);
        if (!file.is_open()) return cfg;

        std::cout << "[CONFIG] 파일 로드: " << iniPath.string() << std::endl;

        std::string line, section;
        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty()) continue;
            if (line[0] == '#' || line[0] == ';') continue;

            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);

            for (size_t i = 1; i < v.size(); ++i) {
                if ((v[i] == '#' || v[i] == ';') && (v[i - 1] == ' ' || v[i - 1] == '\t')) {
                    v = v.substr(0, i);
                    break;
                }
            }

            k.erase(0, k.find_first_not_of(" \t\r\n"));
            k.erase(k.find_last_not_of(" \t\r\n") + 1);
            v.erase(0, v.find_first_not_of(" \t\r\n"));
            v.erase(v.find_last_not_of(" \t\r\n") + 1);

            if (section == "general") {
                if (k == "fps") cfg.fps = std::stoi(v);
                else if (k == "globalOffset") cfg.globalOffset = std::stod(v);
                else if (k == "maxWindow") cfg.maxWindow = std::stod(v);
                else if (k == "judgeY") cfg.judgeY = std::stod(v);
                else if (k == "mode") cfg.mode = v;
                else if (k == "noteThreshold") cfg.noteThreshold = std::stod(v);
                else if (k == "maxDetectY") cfg.maxDetectY = std::stoi(v);
                else if (k == "overlayX") cfg.overlayX = std::stoi(v);
                else if (k == "overlayY") cfg.overlayY = std::stoi(v);
                else if (k == "fastSlowY") cfg.fastSlowY = std::stoi(v);
                else if (k == "fastSlowThresholdMs" || k == "fastSlowThreshold") cfg.fastSlowThresholdMs = std::max(0, std::stoi(v));
                else if (k == "Capture" || k == "capture") { std::string value=v; std::transform(value.begin(),value.end(),value.begin(),::tolower); cfg.capture=(value=="true"||value=="1"||value=="yes"||value=="on"); }
                else if (k == "showFastSlowCount") { std::string value=v; std::transform(value.begin(),value.end(),value.begin(),::tolower); cfg.showFastSlowCount=(value=="true"||value=="1"||value=="yes"||value=="on"); }
                else if (k == "maxMarkers") {
                    int val = std::stoi(v);
                    cfg.maxMarkers = (val > 1) ? val : 1; // 매크로 충돌 방지
                }
                else if (k == "avgSamples") {
                    int val = std::stoi(v);
                    cfg.avgSamples = (val > 1) ? val : 1; // 매크로 충돌 방지
                }
                else if (k == "showAVG") {
                    std::string value = v;
                    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                    cfg.showAvg = (value == "true" || value == "1" || value == "yes" || value == "on");
                }
                else if (k == "showEarlyLate") {
                    std::string value = v;
                    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                    cfg.showEarlyLate = (value == "true" || value == "1" || value == "yes" || value == "on");
                }
                else if (k == "perfectRangeColor") {
                    // RGB format: R,G,B (each component 0-255)
                    std::replace(v.begin(), v.end(), ';', ',');
                    std::stringstream colorStream(v);
                    std::string component;
                    int rgb[3] = { cfg.perfectRangeR, cfg.perfectRangeG, cfg.perfectRangeB };
                    int i = 0;
                    while (i < 3 && std::getline(colorStream, component, ',')) {
                        try { rgb[i] = std::clamp(std::stoi(component), 0, 255); } catch (...) {}
                        ++i;
                    }
                    if (i == 3) {
                        cfg.perfectRangeR = rgb[0];
                        cfg.perfectRangeG = rgb[1];
                        cfg.perfectRangeB = rgb[2];
                    }
                }
            }
            else if (section == "4LANE" || section == "5LANE" || section == "6LANE") {
                int modeNum = section[0] - '0';
                int vk = parseKeyName(v);
                cfg.keyBindings[modeNum][k] = vk;
                if (vk != 0) {
                    std::cout << "  - [" << section << "] " << k << " = '" << v << "' (VK: 0x" << std::hex << vk << std::dec << ")\n";
                }
            }
        }

        std::cout << "  - 모드: " << cfg.mode << "B\n"
            << "  - 판정선(judgeY): " << cfg.judgeY << ", 오프셋: " << cfg.globalOffset << "ms\n"
            << "  - [설정] 눈금자 잔상 마커: 최근 " << cfg.maxMarkers << "개\n"
            << "  - [설정] 평균 계산 타격수: 최근 " << cfg.avgSamples << "타\n"
            << "  - [설정] AVG 표시: " << (cfg.showAvg ? "ON" : "OFF") << "\n"
            << "  - [설정] Early/Late 표시: " << (cfg.showEarlyLate ? "ON" : "OFF") << "\n"
            << "  - [설정] ±42ms 경계 색상 RGB: " << cfg.perfectRangeR << "," << cfg.perfectRangeG << "," << cfg.perfectRangeB << "\n";

        if (cfg.overlayX != -1 && cfg.overlayY != -1) {
            std::cout << "  - [좌표] (" << cfg.overlayX << ", " << cfg.overlayY << ")\n";
        }

        return cfg;
    }
};