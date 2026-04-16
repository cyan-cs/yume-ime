// Copyright (c) 2026-present cyan-cs
//
// Licensed under the MIT License.
// https://opensource.org/license/mit
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include "ime/engine/ime_engine.hpp"
#include "ime/input/key_event.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using yume::ime::dictionary::DictionaryStoragePaths;
using yume::ime::engine::ImeEngine;
using yume::ime::input::KeyCode;
using yume::ime::input::KeyEvent;

KeyEvent key(KeyCode code, std::optional<char16_t> ch, bool ctrl = false) {
    return KeyEvent(code, ch, true, false, ctrl, false);
}

DictionaryStoragePaths makeBenchStoragePaths(std::string_view name) {
    const auto dir = std::filesystem::path("build") / "bench_db" / std::string(name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (ec) {
        throw std::runtime_error(std::string("failed to reset benchmark directory: ") + dir.string());
    }

    ec.clear();
    std::filesystem::create_directories(dir, ec);
    if (ec || !std::filesystem::exists(dir)) {
        throw std::runtime_error(std::string("failed to create benchmark directory: ") + dir.string());
    }

    DictionaryStoragePaths paths;
    paths.userDbPath = dir / "user_db.bin";
    paths.blackDbPath = dir / "black_db.bin";
    return paths;
}

struct BenchHarness final {
    DictionaryStoragePaths paths;
    ImeEngine engine;
    ImeEngine::SessionSnapshot baseline;

    explicit BenchHarness(std::string_view name)
        : paths(makeBenchStoragePaths(name))
        , engine(paths) {
        if (engine.getCurrentState() == yume::ime::state::ImeState::Direct) {
            engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
        } else {
            engine.discardActiveSession();
        }
        baseline = engine.captureSessionSnapshot();
    }

    void reset() {
        engine.restoreSessionSnapshot(baseline);
    }
};

template <typename Fn>
void runBench(std::string_view name, int iterations, Fn&& fn) {
    const auto start = Clock::now();
    std::uint64_t checksum = 0;

    for (int i = 0; i < iterations; ++i) {
        checksum += fn();
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
    const double perIterationUs = static_cast<double>(elapsed) / static_cast<double>(iterations);

    std::cout << std::left << std::setw(24) << name
              << " total_us=" << elapsed
              << " per_iter_us=" << std::fixed << std::setprecision(3) << perIterationUs
              << " checksum=" << checksum << '\n';
}

std::uint64_t benchSimpleInput() {
    static BenchHarness harness("simple_input");
    harness.reset();
    auto& engine = harness.engine;

    const std::vector<KeyEvent> events = {
        key(KeyCode::K, u'k'),
        key(KeyCode::Y, u'y'),
        key(KeyCode::O, u'o'),
        key(KeyCode::U, u'u'),
    };

    std::uint64_t total = 0;
    for (const auto& event : events) {
        const auto output = engine.processKeyEvent(event);
        if (output.composition.has_value()) {
            total += output.composition->text.size();
        }
    }
    return total;
}

std::uint64_t benchSimpleInputCold() {
    ImeEngine engine;
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));

    const std::vector<KeyEvent> events = {
        key(KeyCode::K, u'k'),
        key(KeyCode::Y, u'y'),
        key(KeyCode::O, u'o'),
        key(KeyCode::U, u'u'),
    };

    std::uint64_t total = 0;
    for (const auto& event : events) {
        const auto output = engine.processKeyEvent(event);
        if (output.composition.has_value()) {
            total += output.composition->text.size();
        }
    }
    return total;
}

std::uint64_t benchConversionCommit() {
    static BenchHarness harness("conversion_commit");
    harness.reset();
    auto& engine = harness.engine;

    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));
    engine.processKeyEvent(key(KeyCode::U, u'u'));

    const auto converting = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
    const auto committed = engine.processKeyEvent(key(KeyCode::Enter, std::nullopt));

    std::uint64_t total = 0;
    if (converting.candidates.has_value() && converting.candidates->items) {
        const auto& candidateItems = *converting.candidates->items;
        total += candidateItems.size();
    }
    if (committed.commit.has_value()) {
        total += committed.commit->size();
    }
    return total;
}

std::uint64_t benchConversionCommitCold() {
    ImeEngine engine;
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));
    engine.processKeyEvent(key(KeyCode::U, u'u'));

    const auto converting = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
    const auto committed = engine.processKeyEvent(key(KeyCode::Enter, std::nullopt));

    std::uint64_t total = 0;
    if (converting.candidates.has_value() && converting.candidates->items) {
        total += converting.candidates->items->size();
    }
    if (committed.commit.has_value()) {
        total += committed.commit->size();
    }
    return total;
}

std::uint64_t benchCandidateCycling() {
    static BenchHarness harness("candidate_cycling");
    harness.reset();
    auto& engine = harness.engine;

    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));
    engine.processKeyEvent(key(KeyCode::U, u'u'));
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt));

    std::uint64_t total = 0;
    for (int i = 0; i < 8; ++i) {
        const auto output = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
        if (output.candidates.has_value() && output.candidates->items) {
            total += static_cast<std::uint64_t>(output.candidates->selectedIndex);
        }
    }
    return total;
}

std::uint64_t benchCandidateCyclingCold() {
    ImeEngine engine;
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));
    engine.processKeyEvent(key(KeyCode::U, u'u'));
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt));

    std::uint64_t total = 0;
    for (int i = 0; i < 8; ++i) {
        const auto output = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
        if (output.candidates.has_value() && output.candidates->items) {
            total += static_cast<std::uint64_t>(output.candidates->selectedIndex);
        }
    }
    return total;
}

std::uint64_t benchSegmentNavigation() {
    static BenchHarness harness("segment_navigation");
    harness.reset();
    auto& engine = harness.engine;

    const std::vector<KeyEvent> events = {
        key(KeyCode::W, u'w'),
        key(KeyCode::A, u'a'),
        key(KeyCode::T, u't'),
        key(KeyCode::A, u'a'),
        key(KeyCode::S, u's'),
        key(KeyCode::H, u'h'),
        key(KeyCode::I, u'i'),
        key(KeyCode::H, u'h'),
        key(KeyCode::A, u'a'),
    };

    for (const auto& event : events) {
        engine.processKeyEvent(event);
    }

    engine.processKeyEvent(key(KeyCode::Space, std::nullopt));

    std::uint64_t total = 0;
    for (const auto code : {KeyCode::Right, KeyCode::Left, KeyCode::Enter, KeyCode::Enter}) {
        const auto output = engine.processKeyEvent(key(code, std::nullopt));
        if (output.composition.has_value()) {
            total += output.composition->segmentEnd - output.composition->segmentStart;
        }
        if (output.commit.has_value()) {
            total += output.commit->size();
        }
    }
    return total;
}

std::uint64_t benchSegmentNavigationCold() {
    ImeEngine engine;
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));

    const std::vector<KeyEvent> events = {
        key(KeyCode::W, u'w'),
        key(KeyCode::A, u'a'),
        key(KeyCode::T, u't'),
        key(KeyCode::A, u'a'),
        key(KeyCode::S, u's'),
        key(KeyCode::H, u'h'),
        key(KeyCode::I, u'i'),
        key(KeyCode::H, u'h'),
        key(KeyCode::A, u'a'),
    };

    for (const auto& event : events) {
        engine.processKeyEvent(event);
    }

    engine.processKeyEvent(key(KeyCode::Space, std::nullopt));

    std::uint64_t total = 0;
    for (const auto code : {KeyCode::Right, KeyCode::Left, KeyCode::Enter, KeyCode::Enter}) {
        const auto output = engine.processKeyEvent(key(code, std::nullopt));
        if (output.composition.has_value()) {
            total += output.composition->segmentEnd - output.composition->segmentStart;
        }
        if (output.commit.has_value()) {
            total += output.commit->size();
        }
    }
    return total;
}

} // namespace

int main(int argc, char** argv) {
    constexpr int kColdIterations = 1;
    constexpr int kHotIterations = 100;
    const bool runCold = (argc > 1) && std::string_view(argv[1]) == "--cold";

    std::cout << "ImeCoreBench hot_iterations=" << kHotIterations;
    if (runCold) {
        std::cout << " cold_iterations=" << kColdIterations;
    }
    std::cout << '\n';

    runBench("hot_simple_input", kHotIterations, benchSimpleInput);
    runBench("hot_conversion_commit", kHotIterations, benchConversionCommit);
    runBench("hot_candidate_cycling", kHotIterations, benchCandidateCycling);
    runBench("hot_segment_navigation", kHotIterations, benchSegmentNavigation);

    if (runCold) {
        runBench("cold_simple_input", kColdIterations, benchSimpleInputCold);
        runBench("cold_conversion_commit", kColdIterations, benchConversionCommitCold);
        runBench("cold_candidate_cycling", kColdIterations, benchCandidateCyclingCold);
        runBench("cold_segment_navigation", kColdIterations, benchSegmentNavigationCold);
    }

    return 0;
}
