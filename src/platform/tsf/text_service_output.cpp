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



#include "platform/tsf/text_service.hpp"

#include "ime/composition/kana_transform.hpp"

namespace yume::platform::tsf {

namespace {

using CandidateVector = std::vector<ime::candidate::Candidate>;

size_t mapTransformedOffset(const ime::composition::kana::TransformResult& transform, size_t sourceOffset) {
    if (transform.sourceOffsets.empty()) {
        return 0;
    }
    const size_t clamped = sourceOffset < transform.sourceOffsets.size()
        ? sourceOffset
        : (transform.sourceOffsets.size() - 1);
    return transform.sourceOffsets[clamped];
}

std::u16string maybeTransformForHalfKatakana(
    const TextServiceConfig& config,
    std::u16string_view text) {
    if (!config.usesHalfKatakanaOutput()) {
        return std::u16string(text);
    }
    return ime::composition::kana::toHalfWidthKatakana(text);
}

void maybeTransformCompositionForHalfKatakana(
    const TextServiceConfig& config,
    ime::engine::CompositionState& composition) {
    if (!config.usesHalfKatakanaOutput()) {
        return;
    }

    const auto transform = ime::composition::kana::transformToHalfWidthKatakana(composition.text);
    composition.text = transform.text;
    composition.cursorPosition = mapTransformedOffset(transform, composition.cursorPosition);
    composition.segmentStart = mapTransformedOffset(transform, composition.segmentStart);
    composition.segmentEnd = mapTransformedOffset(transform, composition.segmentEnd);
}

}
HRESULT TextService::dispatchEngineOutput(ITfContext* context, const ime::engine::EngineOutput& output) {
    return dispatchEngineOutput(context, output, true);
}

HRESULT TextService::dispatchEngineOutput(
    ITfContext* context,
    const ime::engine::EngineOutput& output,
    bool requireSynchronous) {
    const auto adaptedOutput = adaptOutputForConfig(output);
    const HRESULT hr = requestEditSession(context, adaptedOutput, requireSynchronous);
    if (FAILED(hr)) {
        return hr;
    }
    updateCandidateUi(context, adaptedOutput);
    return S_OK;
}

ime::engine::EngineOutput TextService::adaptOutputForConfig(const ime::engine::EngineOutput& output) const {
    ime::engine::EngineOutput adapted = output;

    if (adapted.composition.has_value()) {
        maybeTransformCompositionForHalfKatakana(config, *adapted.composition);
    }

    if (adapted.commit.has_value()) {
        adapted.commit = maybeTransformForHalfKatakana(config, *adapted.commit);
    }

    if (adapted.candidates.has_value() && adapted.candidates->items && !adapted.candidates->items->empty()) {
        auto transformedItems = std::make_shared<CandidateVector>(*adapted.candidates->items);
        for (auto& candidate : *transformedItems) {
            candidate.text = maybeTransformForHalfKatakana(config, candidate.text);
        }
        adapted.candidates->items = transformedItems;
    }

    return adapted;
}

}
