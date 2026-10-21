#pragma once
#include <mutex>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <optional>
#include <iostream>
#include <set>

struct Detection {
    std::string lane = "";
    double y = 0.0;
    double confidence = 0.0;
    std::string kind = "normal";
};

struct Note {
    int id = 0;
    int mode = 0;
    std::string lane = "";
    double y = 0.0;
    double seen = 0.0;
    std::optional<double> due;
    int hits = 1;
    bool consumed = false;
    bool ended = false;
};

struct MatchResult {
    int mode = 0;
    std::string lane = "";
    std::optional<double> due;
    std::optional<double> press;
    int note_id = 0;

    std::optional<int> error_ms() const {
        if (!due.has_value() || !press.has_value()) return std::nullopt;
        return static_cast<int>(std::round(press.value()) - std::round(due.value()));
    }
};

struct AssignPair {
    int i = 0;
    int j = 0;
    double error = 0.0;
};

inline std::vector<AssignPair> findAssignments(const std::vector<Detection>& oldDet,
    const std::vector<Detection>& newDet,
    double shift, double tolerance) {
    struct Cand { double err; int i; int j; };
    std::vector<Cand> cands;
    for (size_t i = 0; i < oldDet.size(); ++i) {
        for (size_t j = 0; j < newDet.size(); ++j) {
            if (oldDet[i].lane == newDet[j].lane) {
                double diff = std::abs(newDet[j].y - oldDet[i].y - shift);
                if (diff <= tolerance) {
                    cands.push_back({ diff, static_cast<int>(i), static_cast<int>(j) });
                }
            }
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.err < b.err;
        });

    std::set<int> usedI, usedJ;
    std::vector<AssignPair> pairs;
    for (const auto& c : cands) {
        if (usedI.find(c.i) == usedI.end() && usedJ.find(c.j) == usedJ.end()) {
            usedI.insert(c.i);
            usedJ.insert(c.j);
            pairs.push_back({ c.i, c.j, c.err });
        }
    }
    return pairs;
}

inline std::optional<double> estimateShift(const std::vector<Detection>& oldDet,
    const std::vector<Detection>& newDet,
    double dt,
    std::optional<double> currentVel,
    double minSpeed = 0.2,
    double maxSpeed = 8.0) {
    std::vector<double> candidateShifts;
    for (const auto& a : oldDet) {
        for (const auto& b : newDet) {
            if (a.lane == b.lane) {
                double dy = b.y - a.y;
                if (dy >= minSpeed * dt && dy <= maxSpeed * dt) {
                    candidateShifts.push_back(dy);
                }
            }
        }
    }
    if (candidateShifts.empty()) return std::nullopt;

    struct Hypothesis {
        int pairCount;
        double exactShift;
        double totalError;
    };
    std::vector<Hypothesis> hypotheses;

    for (double s : candidateShifts) {
        auto pairs = findAssignments(oldDet, newDet, s, 5.0);
        if (!pairs.empty()) {
            std::vector<double> diffs;
            double totErr = 0;
            for (const auto& p : pairs) {
                diffs.push_back(newDet[p.j].y - oldDet[p.i].y);
                totErr += p.error;
            }
            std::sort(diffs.begin(), diffs.end());
            // Python statistics.median(): average the middle two values for even N.
            const size_t mid = diffs.size() / 2;
            double medShift = (diffs.size() % 2 == 0)
                ? (diffs[mid - 1] + diffs[mid]) / 2.0
                : diffs[mid];

            bool duplicate = false;
            for (const auto& h : hypotheses) {
                if (std::abs(medShift - h.exactShift) <= 4.0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                hypotheses.push_back({ static_cast<int>(pairs.size()), medShift, totErr });
            }
        }
    }

    if (hypotheses.empty()) return std::nullopt;

    double priorShift = currentVel.has_value() ? (currentVel.value() * dt) : 0.0;

    std::sort(hypotheses.begin(), hypotheses.end(), [currentVel, priorShift](const Hypothesis& a, const Hypothesis& b) {
        if (a.pairCount != b.pairCount) return a.pairCount > b.pairCount;
        if (currentVel.has_value()) return std::abs(a.exactShift - priorShift) < std::abs(b.exactShift - priorShift);
        return a.totalError < b.totalError;
        });

    // Match the Python implementation: at low capture rates, repeated note
    // patterns can make two different displacements equally plausible.
    // Reject the measurement rather than selecting an arbitrary velocity.
    const auto& best = hypotheses[0];
    if (hypotheses.size() > 1 && best.pairCount == hypotheses[1].pairCount) {
        if (!currentVel.has_value() ||
            std::abs(std::abs(best.exactShift - priorShift) -
                     std::abs(hypotheses[1].exactShift - priorShift)) < 5.0) {
            return std::nullopt;
        }
    }

    return best.exactShift;
}

class TimingEngine {
public:
    std::mutex mtx;
    double offset;
    double window;
    double judgeY;
    int fps;
    std::optional<double> velocity;
    std::vector<Note> notes;
    int next_id = 1;
    int current_mode = 0;
    std::optional<double> last_time;
    std::vector<Detection> prev_detections;

    TimingEngine(double off = -5.0, double win = 150.0, double jY = 750.0, int f = 12)
        : offset(off), window(win), judgeY(jY), fps(f) {}

    void reset(int m = 0) {
        std::lock_guard<std::mutex> lock(mtx);
        current_mode = m;
        velocity.reset();
        notes.clear();
        last_time.reset();
        prev_detections.clear();
    }

    std::vector<MatchResult> onFrame(double t, int mode, const std::vector<Detection>& detections) {
        std::lock_guard<std::mutex> lock(mtx);
        if (mode != current_mode) {
            current_mode = mode;
            velocity.reset();
            notes.clear();
            last_time.reset();
            prev_detections.clear();
        }
        if (mode == 0) return {};

        // Match Python Engine.frame(): ignore duplicate/out-of-order timestamps,
        // and reacquire tracking after a gap relative to the configured FPS.
        if (last_time.has_value() && t <= last_time.value()) return {};

        double dt = last_time.has_value() ? (t - last_time.value()) : -1.0;
        if (dt > std::max(500.0, 3000.0 / std::max(1, fps))) {
            velocity.reset();
            notes.clear();
            prev_detections.clear();
            dt = -1.0;
        }

        std::optional<double> shift;
        if (dt > 0) {
            shift = estimateShift(prev_detections, detections, dt, velocity);
            if (shift.has_value()) {
                velocity = shift.value() / dt;
            }
            else {
                // A stable, unmoving image indicates a paused/stationary screen;
                // do not keep extrapolating stale note timing through it.
                auto stationary = findAssignments(prev_detections, detections, 0.0, 1.0);
                if (!prev_detections.empty() && !detections.empty() &&
                    stationary.size() >= std::min({ size_t(2), prev_detections.size(), detections.size() })) {
                    notes.clear();
                    velocity.reset();
                }
                if (!velocity.has_value()) notes.clear();
            }
        }

        double movement = shift.has_value() ? shift.value()
            : (velocity.has_value() ? velocity.value() * (dt > 0 ? dt : 0) : 0);
        for (auto& n : notes) {
            if (!n.ended) n.y += movement;
        }

        std::vector<Detection> noteDets;
        for (const auto& n : notes) {
            noteDets.push_back({ n.lane, n.y, 1.0, "normal" });
        }
        auto pairs = findAssignments(noteDets, detections, 0.0, velocity.has_value() ? 12.0 : 4.0);

        std::set<int> usedJ;
        for (const auto& p : pairs) {
            notes[p.i].y = detections[p.j].y;
            notes[p.i].seen = t;
            notes[p.i].hits++;
            usedJ.insert(p.j);
        }

        // [수정 완료] 여는 괄호 '(' 정상 추가
        for (size_t j = 0; j < detections.size(); ++j) {
            if (usedJ.find(static_cast<int>(j)) == usedJ.end()) {
                Note n;
                n.id = next_id++;
                n.mode = mode;
                n.lane = detections[j].lane;
                n.y = detections[j].y;
                n.seen = t;
                n.hits = velocity.has_value() ? 2 : 1;
                notes.push_back(n);
            }
        }

        if (velocity.has_value() && velocity.value() > 0) {
            for (auto& n : notes) {
                if (!n.ended) {
                    n.due = t + (judgeY - n.y) / velocity.value() + offset;
                    if (n.y >= judgeY) n.ended = true;
                }
            }
        }

        // Match Python's age threshold for unconfirmed detections.
        const double unconfirmedTimeout = 2000.0 / std::max(1, fps);
        notes.erase(std::remove_if(notes.begin(), notes.end(), [t, unconfirmedTimeout](const Note& n) {
            return (n.hits < 2 && n.y < 600.0 && (t - n.seen > unconfirmedTimeout));
            }), notes.end());

        notes.erase(std::remove_if(notes.begin(), notes.end(), [t, this](const Note& n) {
            return n.due.has_value() && (t > n.due.value() + window + 100.0);
            }), notes.end());

        prev_detections = detections;
        last_time = t;
        return {};
    }

    std::optional<MatchResult> onPress(double t, const std::string& lane) {
        std::lock_guard<std::mutex> lock(mtx);
        if (current_mode == 0) return std::nullopt;
        std::string phys_lane = (current_mode == 5 && (lane == "3a" || lane == "3b")) ? "3" : lane;

        Note* best_note = nullptr;
        double best_diff = 999999.0;

        for (auto& n : notes) {
            if (n.lane == phys_lane && !n.consumed && n.due.has_value() && n.hits >= 2) {
                double diff = std::abs(t - n.due.value());
                if (diff <= window && diff < best_diff) {
                    best_diff = diff;
                    best_note = &n;
                }
            }
        }

        if (best_note) {
            best_note->consumed = true;
            MatchResult res{ current_mode, lane, best_note->due, t, best_note->id };
            return res;
        }
        return std::nullopt;
    }
};