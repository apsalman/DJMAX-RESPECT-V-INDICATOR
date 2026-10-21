#pragma once
#pragma warning(push, 0)
#include <opencv2/opencv.hpp>
#pragma warning(pop)

#include <vector>
#include <string>
#include <tuple>
#include <filesystem>
#include <iostream>
#include "Engine.hpp"
#include "Config.hpp"

class NoteDetector {
public:
    double threshold;
    int maxDetectY;
    std::unordered_map<int, std::vector<std::pair<cv::Mat, cv::Mat>>> templates;

    NoteDetector(double thresh = 0.75, int maxY = 610)
        : threshold(thresh), maxDetectY(maxY) {

        fs::path assetDir = Config::getExeDirectory() / "assets";
        if (!fs::exists(assetDir)) {
            assetDir = "assets";
        }

        int loadCount = 0;
        for (int m : {4, 5, 6}) {
            for (const std::string& col : { "white", "blue" }) {
                fs::path p = assetDir / ("cap" + std::to_string(m) + "_" + col + ".png");
                cv::Mat base = cv::imread(p.string(), cv::IMREAD_GRAYSCALE);
                if (base.empty()) continue;

                cv::Mat mask = cv::Mat::zeros(base.size(), CV_8UC1);
                int w = base.cols, h = base.rows;
                cv::circle(mask, cv::Point((w - 1) / 2, 0), (w - 3) / 2, cv::Scalar(255), -1);
                templates[m].push_back({ base, mask });
                loadCount++;
            }
        }
    }

    int detectMode(const cv::Mat& roi) {
        if (roi.rows < 890) return 0;
        cv::Mat strip = roi(cv::Rect(0, 825, 480, 65));
        cv::Scalar med = cv::mean(strip);
        if (med[0] < 10 || med[0] > 220) return 0;

        cv::Mat gray;
        cv::cvtColor(strip, gray, cv::COLOR_BGR2GRAY);
        gray.convertTo(gray, CV_32F);

        cv::Mat diff;
        cv::absdiff(gray.colRange(1, gray.cols), gray.colRange(0, gray.cols - 1), diff);

        std::unordered_map<int, double> scores;
        for (int n : {4, 5, 6}) {
            std::vector<double> strengths;
            for (int i = 1; i < n; ++i) {
                int x = static_cast<int>(std::round(480.0 * i / n));
                int x0 = std::max(0, x - 3), x1 = std::min(diff.cols - 1, x + 2);
                double minV, maxV;
                cv::minMaxLoc(diff.colRange(x0, x1 + 1), &minV, &maxV);
                strengths.push_back(maxV);
            }
            std::sort(strengths.begin(), strengths.end());
            scores[n] = strengths.size() > 1 ? strengths[1] : strengths[0];
        }

        int best_mode = 0;
        double best_score = 0;
        for (auto& [m, s] : scores) {
            if (s > best_score) { best_score = s; best_mode = m; }
        }
        return best_score >= 6.0 ? best_mode : 0;
    }

    std::vector<Detection> detect(const cv::Mat& roi, int mode) {
        std::vector<Detection> results;
        if (templates.find(mode) == templates.end()) return results;

        cv::Mat gray;
        cv::cvtColor(roi.rowRange(0, std::min(750, roi.rows)), gray, cv::COLOR_BGR2GRAY);

        for (int lane = 0; lane < mode; ++lane) {
            double center = 480.0 * (lane + 0.5) / mode;
            std::vector<std::tuple<float, double, int>> candidates;

            for (auto& [templ, mask] : templates[mode]) {
                int w = templ.cols, h = templ.rows;
                int x0 = std::max(0, static_cast<int>(std::round(center - (w - 1) / 2.0)) - 3);
                int x1 = std::min(480, x0 + w + 6);
                int bandH = std::min(gray.rows, maxDetectY + h + 1);
                if (x1 <= x0 || bandH <= h) continue;

                cv::Mat band = gray(cv::Rect(x0, 0, x1 - x0, bandH));
                cv::Mat match;
                cv::matchTemplate(band, templ, match, cv::TM_CCOEFF_NORMED, mask);

                std::vector<float> rowMax(match.rows, -1.0f);
                for (int y = 0; y < match.rows; ++y) {
                    const float* ptr = match.ptr<float>(y);
                    float mVal = -1.0f;
                    for (int x = 0; x < match.cols; ++x) {
                        if (ptr[x] > mVal) mVal = ptr[x];
                    }
                    rowMax[y] = mVal;
                }

                for (int y = 4; y < std::min(match.rows - 4, maxDetectY); ++y) {
                    float val = rowMax[y];
                    if (val >= threshold && val >= rowMax[y - 1] && val >= rowMax[y + 1]) {
                        bool isPeak = true;
                        for (int dy = -4; dy <= 4; ++dy) {
                            if (dy != 0 && rowMax[y + dy] > val) {
                                isPeak = false;
                                break;
                            }
                        }
                        if (isPeak) {
                            candidates.push_back({ val, static_cast<double>(y), w });
                        }
                    }
                }
            }

            std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a) > std::get<0>(b);
                });

            std::vector<double> selectedY;
            for (const auto& [score, y, w] : candidates) {
                bool conflict = false;
                for (double sy : selectedY) {
                    if (std::abs(y - sy) < w * 0.55) {
                        conflict = true;
                        break;
                    }
                }
                if (!conflict) {
                    selectedY.push_back(y);
                    results.push_back({ std::to_string(lane + 1), y, score, "normal" });
                }
            }
        }

        cv::Mat hsv;
        cv::cvtColor(roi.rowRange(0, std::min(750, roi.rows)), hsv, cv::COLOR_BGR2HSV);
        cv::Mat side_mask;
        cv::inRange(hsv, cv::Scalar(80, 55, 130), cv::Scalar(101, 255, 255), side_mask);

        for (int s = 0; s < 2; ++s) {
            cv::Mat part = side_mask.colRange(s * 240 + 5, s * 240 + 235);
            for (int y = 5; y < maxDetectY; ++y) {
                if (cv::countNonZero(part.row(y)) > 60 && cv::countNonZero(part.row(y + 3)) < 15) {
                    results.push_back({ (s == 0 ? "A" : "B"), static_cast<double>(y - 10), 1.0, "side" });
                }
            }
        }
        return results;
    }
};