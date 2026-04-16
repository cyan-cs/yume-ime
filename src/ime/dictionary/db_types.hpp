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



#pragma once

#include "ime/candidate/candidate_types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace yume::ime::dictionary {

    using EntryIndex = int32_t;
    using NodeIndex = int32_t;
    using EntryScore = int32_t;
    using ReadingText = std::u16string;
    using SurfaceText = std::u16string;
    using ReadingView = std::u16string_view;
    using SurfaceView = std::u16string_view;

    enum class PartOfSpeech : uint8_t {
        Unknown = 0,
        Noun,
        Verb,
        Adjective,
        Adverb,
        Particle,
        Copula,
        Ending,
        Modal,
    };

    enum class LexiconCategory : uint8_t {
        None = 0,
        PlaceName,
        CurrencyCode,
        CurrencySymbol,
    };

    struct LexiconEntry {
        ReadingText reading;
        SurfaceText text;
        EntryScore score = 0;
        PartOfSpeech partOfSpeech = PartOfSpeech::Unknown;
        LexiconCategory category = LexiconCategory::None;
    };

    using CandidateVector = std::vector<candidate::Candidate>;

    inline std::u16string makeEntryKey(ReadingView reading, SurfaceView text) {
        std::u16string key;
        key.reserve(reading.size() + text.size() + 1);
        key.append(reading);
        key.push_back(u'\t');
        key.append(text);
        return key;
    }

}
