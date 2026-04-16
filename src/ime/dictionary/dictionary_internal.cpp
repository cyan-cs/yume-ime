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



#include "ime/dictionary/dictionary_internal.hpp"

#include "utils/logger.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace yume::ime::dictionary::detail {

namespace {

std::mutex g_reloadCacheMutex;
std::unordered_map<std::string, CachedReloadState> g_reloadCache;

constexpr int32_t kPredictionLengthPenaltyStep = 8;
constexpr int32_t kExactPredictionBoostScore = 120;
constexpr char16_t kKatakanaOffset = 0x60;
constexpr int32_t kFullWidthNumberCandidateScore = -5;
constexpr int32_t kGroupedNumberCandidateScore = -6;
constexpr int32_t kFullWidthGroupedNumberCandidateScore = -7;
constexpr int32_t kCurrencyCandidateScore = -2;
constexpr int32_t kTimeCandidateScore = -2;
constexpr int32_t kSlashYearMonthCandidateScore = -3;
constexpr int32_t kJapaneseYearMonthCandidateScore = -4;
constexpr int32_t kSlashDateCandidateScore = -3;
constexpr int32_t kJapaneseDateCandidateScore = -4;
constexpr int32_t kKanjiNumberCandidateScore = -15;

using KanaRomajiEntry = std::pair<std::u16string_view, std::string_view>;

constexpr auto kKanaToRomaji = std::to_array<KanaRomajiEntry>({
    KanaRomajiEntry{u"\u304D\u3083", "kya"},
    KanaRomajiEntry{u"\u304D\u3085", "kyu"},
    KanaRomajiEntry{u"\u304D\u3087", "kyo"},
    KanaRomajiEntry{u"\u304E\u3083", "gya"},
    KanaRomajiEntry{u"\u304E\u3085", "gyu"},
    KanaRomajiEntry{u"\u304E\u3087", "gyo"},
    KanaRomajiEntry{u"\u3057\u3083", "sha"},
    KanaRomajiEntry{u"\u3057\u3085", "shu"},
    KanaRomajiEntry{u"\u3057\u3087", "sho"},
    KanaRomajiEntry{u"\u3058\u3083", "ja"},
    KanaRomajiEntry{u"\u3058\u3085", "ju"},
    KanaRomajiEntry{u"\u3058\u3087", "jo"},
    KanaRomajiEntry{u"\u3061\u3083", "cha"},
    KanaRomajiEntry{u"\u3061\u3085", "chu"},
    KanaRomajiEntry{u"\u3061\u3087", "cho"},
    KanaRomajiEntry{u"\u306B\u3083", "nya"},
    KanaRomajiEntry{u"\u306B\u3085", "nyu"},
    KanaRomajiEntry{u"\u306B\u3087", "nyo"},
    KanaRomajiEntry{u"\u3072\u3083", "hya"},
    KanaRomajiEntry{u"\u3072\u3085", "hyu"},
    KanaRomajiEntry{u"\u3072\u3087", "hyo"},
    KanaRomajiEntry{u"\u3073\u3083", "bya"},
    KanaRomajiEntry{u"\u3073\u3085", "byu"},
    KanaRomajiEntry{u"\u3073\u3087", "byo"},
    KanaRomajiEntry{u"\u3074\u3083", "pya"},
    KanaRomajiEntry{u"\u3074\u3085", "pyu"},
    KanaRomajiEntry{u"\u3074\u3087", "pyo"},
    KanaRomajiEntry{u"\u307F\u3083", "mya"},
    KanaRomajiEntry{u"\u307F\u3085", "myu"},
    KanaRomajiEntry{u"\u307F\u3087", "myo"},
    KanaRomajiEntry{u"\u308A\u3083", "rya"},
    KanaRomajiEntry{u"\u308A\u3085", "ryu"},
    KanaRomajiEntry{u"\u308A\u3087", "ryo"},
    KanaRomajiEntry{u"\u3075\u3041", "fa"},
    KanaRomajiEntry{u"\u3075\u3043", "fi"},
    KanaRomajiEntry{u"\u3075\u3047", "fe"},
    KanaRomajiEntry{u"\u3075\u3049", "fo"},
    KanaRomajiEntry{u"\u3094\u3041", "va"},
    KanaRomajiEntry{u"\u3094\u3043", "vi"},
    KanaRomajiEntry{u"\u3094", "vu"},
    KanaRomajiEntry{u"\u3094\u3047", "ve"},
    KanaRomajiEntry{u"\u3094\u3049", "vo"},
    KanaRomajiEntry{u"\u3042", "a"},
    KanaRomajiEntry{u"\u3044", "i"},
    KanaRomajiEntry{u"\u3046", "u"},
    KanaRomajiEntry{u"\u3048", "e"},
    KanaRomajiEntry{u"\u304A", "o"},
    KanaRomajiEntry{u"\u304B", "ka"},
    KanaRomajiEntry{u"\u304D", "ki"},
    KanaRomajiEntry{u"\u304F", "ku"},
    KanaRomajiEntry{u"\u3051", "ke"},
    KanaRomajiEntry{u"\u3053", "ko"},
    KanaRomajiEntry{u"\u304C", "ga"},
    KanaRomajiEntry{u"\u304E", "gi"},
    KanaRomajiEntry{u"\u3050", "gu"},
    KanaRomajiEntry{u"\u3052", "ge"},
    KanaRomajiEntry{u"\u3054", "go"},
    KanaRomajiEntry{u"\u3055", "sa"},
    KanaRomajiEntry{u"\u3057", "shi"},
    KanaRomajiEntry{u"\u3059", "su"},
    KanaRomajiEntry{u"\u305B", "se"},
    KanaRomajiEntry{u"\u305D", "so"},
    KanaRomajiEntry{u"\u3056", "za"},
    KanaRomajiEntry{u"\u3058", "ji"},
    KanaRomajiEntry{u"\u305A", "zu"},
    KanaRomajiEntry{u"\u305C", "ze"},
    KanaRomajiEntry{u"\u305E", "zo"},
    KanaRomajiEntry{u"\u305F", "ta"},
    KanaRomajiEntry{u"\u3061", "chi"},
    KanaRomajiEntry{u"\u3064", "tsu"},
    KanaRomajiEntry{u"\u3066", "te"},
    KanaRomajiEntry{u"\u3068", "to"},
    KanaRomajiEntry{u"\u3060", "da"},
    KanaRomajiEntry{u"\u3062", "ji"},
    KanaRomajiEntry{u"\u3065", "zu"},
    KanaRomajiEntry{u"\u3067", "de"},
    KanaRomajiEntry{u"\u3069", "do"},
    KanaRomajiEntry{u"\u306A", "na"},
    KanaRomajiEntry{u"\u306B", "ni"},
    KanaRomajiEntry{u"\u306C", "nu"},
    KanaRomajiEntry{u"\u306D", "ne"},
    KanaRomajiEntry{u"\u306E", "no"},
    KanaRomajiEntry{u"\u306F", "ha"},
    KanaRomajiEntry{u"\u3072", "hi"},
    KanaRomajiEntry{u"\u3075", "fu"},
    KanaRomajiEntry{u"\u3078", "he"},
    KanaRomajiEntry{u"\u307B", "ho"},
    KanaRomajiEntry{u"\u3070", "ba"},
    KanaRomajiEntry{u"\u3073", "bi"},
    KanaRomajiEntry{u"\u3076", "bu"},
    KanaRomajiEntry{u"\u3079", "be"},
    KanaRomajiEntry{u"\u307C", "bo"},
    KanaRomajiEntry{u"\u3071", "pa"},
    KanaRomajiEntry{u"\u3074", "pi"},
    KanaRomajiEntry{u"\u3077", "pu"},
    KanaRomajiEntry{u"\u307A", "pe"},
    KanaRomajiEntry{u"\u307D", "po"},
    KanaRomajiEntry{u"\u307E", "ma"},
    KanaRomajiEntry{u"\u307F", "mi"},
    KanaRomajiEntry{u"\u3080", "mu"},
    KanaRomajiEntry{u"\u3081", "me"},
    KanaRomajiEntry{u"\u3082", "mo"},
    KanaRomajiEntry{u"\u3084", "ya"},
    KanaRomajiEntry{u"\u3086", "yu"},
    KanaRomajiEntry{u"\u3088", "yo"},
    KanaRomajiEntry{u"\u3089", "ra"},
    KanaRomajiEntry{u"\u308A", "ri"},
    KanaRomajiEntry{u"\u308B", "ru"},
    KanaRomajiEntry{u"\u308C", "re"},
    KanaRomajiEntry{u"\u308D", "ro"},
    KanaRomajiEntry{u"\u308F", "wa"},
    KanaRomajiEntry{u"\u3092", "o"},
    KanaRomajiEntry{u"\u3093", "n"},
    KanaRomajiEntry{u"\u3041", "xa"},
    KanaRomajiEntry{u"\u3043", "xi"},
    KanaRomajiEntry{u"\u3045", "xu"},
    KanaRomajiEntry{u"\u3047", "xe"},
    KanaRomajiEntry{u"\u3049", "xo"},
    KanaRomajiEntry{u"\u3083", "xya"},
    KanaRomajiEntry{u"\u3085", "xyu"},
    KanaRomajiEntry{u"\u3087", "xyo"},
    KanaRomajiEntry{u"\u3063", "xtsu"},
    KanaRomajiEntry{u"\u30FC", "-"},
    KanaRomajiEntry{u"\u3001", ","},
    KanaRomajiEntry{u"\u3002", "."},
});

const auto kKanaToRomajiMap = [] {
    std::unordered_map<std::u16string_view, std::string_view> map;
    map.reserve(kKanaToRomaji.size());
    for (const auto& [kana, romaji] : kKanaToRomaji) {
        map.emplace(kana, romaji);
    }
    return map;
}();

bool isSmallHiragana(char16_t ch) {
    return ch == u'\u3083' || ch == u'\u3085' || ch == u'\u3087' ||
           ch == u'\u3041' || ch == u'\u3043' || ch == u'\u3045' ||
           ch == u'\u3047' || ch == u'\u3049';
}

bool isAsciiDigitString(std::u16string_view value) {
    if (value.empty()) {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](char16_t ch) {
        return ch >= u'0' && ch <= u'9';
    });
}

size_t asciiDigitPrefixLength(std::u16string_view value) {
    size_t length = 0;
    while (length < value.size() && value[length] >= u'0' && value[length] <= u'9') {
        ++length;
    }
    return length;
}

std::u16string toFullWidthDigits(std::u16string_view value) {
    std::u16string result;
    result.reserve(value.size());
    for (const char16_t ch : value) {
        result.push_back(static_cast<char16_t>(u'\uFF10' + (ch - u'0')));
    }
    return result;
}

std::u16string insertThousandsSeparators(std::u16string_view digits, bool fullWidthComma) {
    if (digits.size() <= 3) {
        return std::u16string(digits);
    }

    std::u16string result;
    result.reserve(digits.size() + ((digits.size() - 1) / 3));
    const size_t leadingCount = digits.size() % 3;
    size_t index = 0;
    if (leadingCount != 0) {
        result.append(digits.substr(0, leadingCount));
        index = leadingCount;
        if (index < digits.size()) {
            result.push_back(fullWidthComma ? u'\uFF0C' : u',');
        }
    }

    while (index < digits.size()) {
        result.append(digits.substr(index, 3));
        index += 3;
        if (index < digits.size()) {
            result.push_back(fullWidthComma ? u'\uFF0C' : u',');
        }
    }
    return result;
}

std::u16string toSimpleKanjiDigits(std::u16string_view value) {
    constexpr std::array<char16_t, 10> kKanjiDigits = {
        u'\u3007', u'\u4E00', u'\u4E8C', u'\u4E09', u'\u56DB',
        u'\u4E94', u'\u516D', u'\u4E03', u'\u516B', u'\u4E5D',
    };

    std::u16string result;
    result.reserve(value.size());
    for (const char16_t ch : value) {
        result.push_back(kKanjiDigits[static_cast<size_t>(ch - u'0')]);
    }
    return result;
}

int parseDecimalDigits(std::u16string_view digits) {
    int value = 0;
    for (const char16_t ch : digits) {
        value = value * 10 + static_cast<int>(ch - u'0');
    }
    return value;
}

bool isLeapYear(int year) {
    if ((year % 400) == 0) {
        return true;
    }
    if ((year % 100) == 0) {
        return false;
    }
    return (year % 4) == 0;
}

int daysInMonth(int year, int month) {
    constexpr std::array<int, 12> kDaysPerMonth = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };

    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return kDaysPerMonth[static_cast<size_t>(month - 1)];
}

bool tryParseCompactDate(
    std::u16string_view digits,
    int& year,
    int& month,
    int& day) {
    if (digits.size() != 8 || !isAsciiDigitString(digits)) {
        return false;
    }

    year = parseDecimalDigits(digits.substr(0, 4));
    month = parseDecimalDigits(digits.substr(4, 2));
    day = parseDecimalDigits(digits.substr(6, 2));

    if (year < 1000 || year > 9999) {
        return false;
    }

    const int maxDay = daysInMonth(year, month);
    return maxDay != 0 && day >= 1 && day <= maxDay;
}

bool tryParseCompactYearMonth(
    std::u16string_view digits,
    int& year,
    int& month) {
    if (digits.size() != 6 || !isAsciiDigitString(digits)) {
        return false;
    }

    year = parseDecimalDigits(digits.substr(0, 4));
    month = parseDecimalDigits(digits.substr(4, 2));
    return year >= 1000 && year <= 9999 && month >= 1 && month <= 12;
}

std::u16string formatSlashDate(int year, int month, int day) {
    std::u16string result;
    result.reserve(10);
    result.push_back(static_cast<char16_t>(u'0' + ((year / 1000) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 100) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (year % 10)));
    result.push_back(u'/');
    result.push_back(static_cast<char16_t>(u'0' + ((month / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (month % 10)));
    result.push_back(u'/');
    result.push_back(static_cast<char16_t>(u'0' + ((day / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (day % 10)));
    return result;
}

std::u16string appendDecimalNumber(std::u16string& target, int value) {
    if (value >= 10) {
        target.push_back(static_cast<char16_t>(u'0' + ((value / 10) % 10)));
    }
    target.push_back(static_cast<char16_t>(u'0' + (value % 10)));
    return target;
}

std::u16string formatJapaneseDate(int year, int month, int day) {
    std::u16string result;
    result.reserve(11);
    result.push_back(static_cast<char16_t>(u'0' + ((year / 1000) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 100) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (year % 10)));
    result.push_back(u'\u5E74');
    appendDecimalNumber(result, month);
    result.push_back(u'\u6708');
    appendDecimalNumber(result, day);
    result.push_back(u'\u65E5');
    return result;
}

std::u16string formatSlashYearMonth(int year, int month) {
    std::u16string result;
    result.reserve(7);
    result.push_back(static_cast<char16_t>(u'0' + ((year / 1000) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 100) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (year % 10)));
    result.push_back(u'/');
    result.push_back(static_cast<char16_t>(u'0' + ((month / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (month % 10)));
    return result;
}

std::u16string formatJapaneseYearMonth(int year, int month) {
    std::u16string result;
    result.reserve(8);
    result.push_back(static_cast<char16_t>(u'0' + ((year / 1000) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 100) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + ((year / 10) % 10)));
    result.push_back(static_cast<char16_t>(u'0' + (year % 10)));
    result.push_back(u'\u5E74');
    appendDecimalNumber(result, month);
    result.push_back(u'\u6708');
    return result;
}

std::string_view lookupRomaji(std::u16string_view kana) {
    const auto it = kKanaToRomajiMap.find(kana);
    return it != kKanaToRomajiMap.end() ? it->second : std::string_view{};
}

}
size_t CandidateKeyHash::operator()(const CandidateKey& key) const {
    return std::hash<std::u16string>{}(key.reading) ^
           (std::hash<std::u16string>{}(key.text) << 1);
}

bool FileSignature::operator==(const FileSignature& other) const {
    return exists == other.exists &&
           valid == other.valid &&
           size == other.size &&
           lastWriteTime == other.lastWriteTime;
}

std::string makeReloadCacheKey(const DictionaryStoragePaths& paths) {
    return paths.userDbPath.string() + '\n' + paths.blackDbPath.string();
}

FileSignature readFileSignature(const std::filesystem::path& path) {
    FileSignature signature;
    std::error_code ec;
    signature.exists = std::filesystem::exists(path, ec);
    if (ec) {
        signature.valid = false;
        return signature;
    }
    if (!signature.exists) {
        return signature;
    }

    signature.size = std::filesystem::file_size(path, ec);
    if (ec) {
        signature.valid = false;
        return signature;
    }

    signature.lastWriteTime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        signature.valid = false;
    }
    return signature;
}

bool tryLoadCachedReloadState(
    const DictionaryStoragePaths& paths,
    CachedReloadState& outState) {
    const FileSignature userDbSignature = readFileSignature(paths.userDbPath);
    const FileSignature blackDbSignature = readFileSignature(paths.blackDbPath);
    if (!userDbSignature.valid || !blackDbSignature.valid) {
        return false;
    }

    const std::scoped_lock lock(g_reloadCacheMutex);
    const auto it = g_reloadCache.find(makeReloadCacheKey(paths));
    if (it == g_reloadCache.end()) {
        return false;
    }

    if (it->second.userDbSignature != userDbSignature ||
        it->second.blackDbSignature != blackDbSignature) {
        return false;
    }

    outState = it->second;
    return true;
}

void storeCachedReloadState(
    const DictionaryStoragePaths& paths,
    const CachedReloadState& state) {
    const std::scoped_lock lock(g_reloadCacheMutex);
    g_reloadCache[makeReloadCacheKey(paths)] = state;
}

int32_t predictionLengthPenalty(size_t readingLength, size_t prefixLength) {
    const size_t diff = (readingLength >= prefixLength) ? (readingLength - prefixLength) : 0;
    return -static_cast<int32_t>(diff) * kPredictionLengthPenaltyStep;
}

int32_t exactPredictionBoost(size_t readingLength, size_t prefixLength) {
    return (readingLength == prefixLength) ? kExactPredictionBoostScore : 0;
}

int32_t contextualPartOfSpeechBoost(const LexiconEntry& entry, PartOfSpeech precedingPartOfSpeech) {
    switch (precedingPartOfSpeech) {
        case PartOfSpeech::Noun:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Particle: return 220;
                case PartOfSpeech::Copula: return 170;
                case PartOfSpeech::Ending: return -90;
                case PartOfSpeech::Modal: return -70;
                case PartOfSpeech::Adjective: return -40;
                case PartOfSpeech::Verb: return -30;
                case PartOfSpeech::Noun: return -20;
                default: return 0;
            }
        case PartOfSpeech::Particle:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Noun: return 180;
                case PartOfSpeech::Adjective: return 140;
                case PartOfSpeech::Verb: return 150;
                case PartOfSpeech::Copula: return 160;
                case PartOfSpeech::Ending: return 130;
                case PartOfSpeech::Modal: return 90;
                case PartOfSpeech::Particle: return -160;
                default: return 0;
            }
        case PartOfSpeech::Copula:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Particle: return 160;
                case PartOfSpeech::Noun: return 90;
                case PartOfSpeech::Adverb: return 40;
                case PartOfSpeech::Ending: return 180;
                case PartOfSpeech::Modal: return 200;
                case PartOfSpeech::Copula: return -100;
                default: return 0;
            }
        case PartOfSpeech::Ending:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Particle: return 180;
                case PartOfSpeech::Adverb: return 60;
                case PartOfSpeech::Modal: return 210;
                case PartOfSpeech::Ending: return -80;
                default: return 0;
            }
        case PartOfSpeech::Modal:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Particle: return 180;
                case PartOfSpeech::Modal: return -100;
                case PartOfSpeech::Copula: return -60;
                case PartOfSpeech::Ending: return -80;
                default: return 0;
            }
        case PartOfSpeech::Verb:
        case PartOfSpeech::Adjective:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Particle: return 180;
                case PartOfSpeech::Copula: return 40;
                case PartOfSpeech::Ending: return 220;
                case PartOfSpeech::Modal: return 150;
                case PartOfSpeech::Adverb: return 60;
                default: return 0;
            }
        case PartOfSpeech::Adverb:
            switch (entry.partOfSpeech) {
                case PartOfSpeech::Verb: return 140;
                case PartOfSpeech::Adjective: return 120;
                case PartOfSpeech::Copula: return 60;
                case PartOfSpeech::Ending: return 80;
                case PartOfSpeech::Modal: return 100;
                default: return 0;
            }
        case PartOfSpeech::Unknown:
        default:
            return 0;
    }
}

int32_t surfaceGrammarBoost(
    const LexiconEntry& entry,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) {
    if (precedingSurface.empty()) {
        return 0;
    }

    if (precedingSurface == u"\u3092") {
        switch (entry.partOfSpeech) {
            case PartOfSpeech::Verb: return 260;
            case PartOfSpeech::Adjective: return 80;
            case PartOfSpeech::Noun: return -180;
            case PartOfSpeech::Copula: return -220;
            case PartOfSpeech::Particle: return -260;
            case PartOfSpeech::Ending: return -180;
            case PartOfSpeech::Modal: return -160;
            default: return 0;
        }
    }

    if (precedingSurface == u"\u306E") {
        switch (entry.partOfSpeech) {
            case PartOfSpeech::Noun: return 220;
            case PartOfSpeech::Adjective: return 40;
            case PartOfSpeech::Verb: return -160;
            case PartOfSpeech::Particle: return -220;
            case PartOfSpeech::Copula: return -180;
            case PartOfSpeech::Ending: return -180;
            case PartOfSpeech::Modal: return -180;
            default: return 0;
        }
    }

    if (precedingSurface == u"\u306F" || precedingSurface == u"\u304C" || precedingSurface == u"\u3082") {
        switch (entry.partOfSpeech) {
            case PartOfSpeech::Copula: return 180;
            case PartOfSpeech::Verb: return 120;
            case PartOfSpeech::Adjective: return 100;
            case PartOfSpeech::Noun: return -140;
            case PartOfSpeech::Particle: return -220;
            case PartOfSpeech::Ending:
                return precedingPartOfSpeech == PartOfSpeech::Verb ? 160 : -60;
            case PartOfSpeech::Modal: return -40;
            default: return 0;
        }
    }

    if (precedingSurface == u"\u306B" || precedingSurface == u"\u3067" ||
        precedingSurface == u"\u3078" || precedingSurface == u"\u3068") {
        switch (entry.partOfSpeech) {
            case PartOfSpeech::Verb: return 140;
            case PartOfSpeech::Adjective: return 60;
            case PartOfSpeech::Noun: return -120;
            case PartOfSpeech::Particle: return -180;
            case PartOfSpeech::Copula: return -140;
            case PartOfSpeech::Ending: return -140;
            case PartOfSpeech::Modal: return -120;
            default: return 0;
        }
    }

    if (precedingSurface == u"\u306B\u306F" || precedingSurface == u"\u3067\u306F" ||
        precedingSurface == u"\u3068\u306F") {
        switch (entry.partOfSpeech) {
            case PartOfSpeech::Verb: return 200;
            case PartOfSpeech::Adjective: return 100;
            case PartOfSpeech::Copula: return 80;
            case PartOfSpeech::Noun: return -180;
            case PartOfSpeech::Particle: return -240;
            case PartOfSpeech::Ending: return -120;
            case PartOfSpeech::Modal: return -80;
            default: return 0;
        }
    }

    if (precedingSurface == u"\u306E\u3067" || precedingSurface == u"\u306E\u306B") {
        switch (entry.partOfSpeech) {
            case PartOfSpeech::Verb: return 180;
            case PartOfSpeech::Adjective: return 120;
            case PartOfSpeech::Copula: return 80;
            case PartOfSpeech::Adverb: return 40;
            case PartOfSpeech::Noun: return -200;
            case PartOfSpeech::Particle: return -260;
            case PartOfSpeech::Ending: return -140;
            case PartOfSpeech::Modal: return -120;
            default: return 0;
        }
    }

    return 0;
}

int32_t rawReadingGrammarBoost(ReadingView reading, PartOfSpeech precedingPartOfSpeech, SurfaceView precedingSurface) {
    if (reading.empty() || precedingSurface.empty()) {
        return 0;
    }

    if (precedingSurface == u"\u3092" || precedingSurface == u"\u306F" ||
        precedingSurface == u"\u304C" || precedingSurface == u"\u3082" ||
        precedingSurface == u"\u306B" || precedingSurface == u"\u3067" ||
        precedingSurface == u"\u3078" || precedingSurface == u"\u3068" ||
        precedingSurface == u"\u3067\u306F" || precedingSurface == u"\u306B\u306F" ||
        precedingSurface == u"\u3068\u306F" || precedingSurface == u"\u306E\u3067" ||
        precedingSurface == u"\u306E\u306B") {
        if (reading.size() <= 2) {
            return -220;
        }
        return -140;
    }

    if (precedingSurface == u"\u306E") {
        return reading.size() == 1 ? -80 : -40;
    }

    (void)precedingPartOfSpeech;
    return 0;
}

int32_t syntheticReadingGrammarBoost(
    ReadingView reading,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) {
    if (reading.empty() || precedingSurface.empty()) {
        return 0;
    }

    if (precedingSurface == u"\u3092" || precedingSurface == u"\u306F" ||
        precedingSurface == u"\u304C" || precedingSurface == u"\u3082" ||
        precedingSurface == u"\u306B" || precedingSurface == u"\u3067" ||
        precedingSurface == u"\u3078" || precedingSurface == u"\u3068" ||
        precedingSurface == u"\u3067\u306F" || precedingSurface == u"\u306B\u306F" ||
        precedingSurface == u"\u3068\u306F" || precedingSurface == u"\u306E\u3067" ||
        precedingSurface == u"\u306E\u306B") {
        return -180;
    }

    if (precedingSurface == u"\u306E") {
        return -60;
    }

    (void)reading;
    (void)precedingPartOfSpeech;
    return 0;
}

int32_t lexicalCategoryBoost(const LexiconEntry& entry, PartOfSpeech precedingPartOfSpeech) {
    if (entry.category != LexiconCategory::PlaceName) {
        return 0;
    }

    switch (precedingPartOfSpeech) {
        case PartOfSpeech::Particle:
            return 0;
        case PartOfSpeech::Unknown:
            return -90;
        case PartOfSpeech::Verb:
        case PartOfSpeech::Adjective:
        case PartOfSpeech::Adverb:
        case PartOfSpeech::Ending:
        case PartOfSpeech::Modal:
            return -70;
        case PartOfSpeech::Noun:
        case PartOfSpeech::Copula:
        default:
            return -30;
    }
}

void appendUniqueCandidate(
    Dictionary::CandidateVector& outCandidates,
    std::unordered_map<CandidateKey, size_t, CandidateKeyHash>& candidateIndices,
    const candidate::Candidate& candidate) {
    const auto [it, inserted] = candidateIndices.emplace(
        CandidateKey{candidate.reading, candidate.text},
        outCandidates.size());
    if (inserted) {
        outCandidates.push_back(candidate);
        return;
    }

    auto& existing = outCandidates[it->second];
    if (candidate.score > existing.score) {
        existing = candidate;
    }
}

void sortAndTrim(Dictionary::CandidateVector& outCandidates, size_t limit) {
    std::sort(outCandidates.begin(), outCandidates.end(), [](const auto& a, const auto& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.reading.size() != b.reading.size()) {
            return a.reading.size() < b.reading.size();
        }
        return a.text < b.text;
    });

    if (outCandidates.size() > limit) {
        outCandidates.resize(limit);
    }
}

void appendEntryCandidate(
    Dictionary::CandidateVector& outCandidates,
    std::unordered_map<CandidateKey, size_t, CandidateKeyHash>& candidateIndices,
    const LexiconEntry& entry,
    const BlackDb& blackDb,
    int32_t extraScore) {
    if (blackDb.isBlocked(entry.reading, entry.text)) {
        return;
    }

    appendUniqueCandidate(outCandidates, candidateIndices, {
        entry.text,
        entry.reading,
        entry.score + extraScore,
    });
}

std::u16string toKatakana(std::u16string_view reading) {
    std::u16string result;
    result.reserve(reading.size());
    for (char16_t ch : reading) {
        if (ch >= u'\u3041' && ch <= u'\u3096') {
            result.push_back(static_cast<char16_t>(ch + kKatakanaOffset));
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

std::u16string toRomaji(ReadingView reading) {
    std::u16string result;
    result.reserve(reading.size() * 3);

    bool pendingSokuon = false;
    for (size_t i = 0; i < reading.size();) {
        if (reading[i] == u'\u3063') {
            // Apply the next romaji initial as a doubled consonant.
            pendingSokuon = true;
            ++i;
            continue;
        }

        std::string_view matchedRomaji;
        size_t matchedLength = 0;
        if (i + 1 < reading.size() && isSmallHiragana(reading[i + 1])) {
            matchedRomaji = lookupRomaji(std::u16string_view(reading.data() + i, 2));
            if (!matchedRomaji.empty()) {
                matchedLength = 2;
            }
        }

        if (matchedLength == 0) {
            matchedRomaji = lookupRomaji(std::u16string_view(reading.data() + i, 1));
            if (matchedRomaji.empty()) {
                result.push_back(reading[i]);
                pendingSokuon = false;
                ++i;
                continue;
            }
            matchedLength = 1;
        }

        if (pendingSokuon && !matchedRomaji.empty()) {
            result.push_back(static_cast<char16_t>(matchedRomaji.front()));
            pendingSokuon = false;
        }
        for (const char ch : matchedRomaji) {
            result.push_back(static_cast<char16_t>(ch));
        }
        i += matchedLength;
    }

    if (pendingSokuon) {
        result += u"xtsu";
    }
    return result;
}

void appendSyntheticReadingCandidates(
    Dictionary::CandidateVector& outCandidates,
    std::unordered_map<CandidateKey, size_t, CandidateKeyHash>& candidateIndices,
    ReadingView reading,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) {
    if (reading.empty()) {
        return;
    }

    const std::u16string normalizedReading(reading);
    const std::u16string romaji = toRomaji(reading);
    const int32_t syntheticGrammarScore =
        syntheticReadingGrammarBoost(reading, precedingPartOfSpeech, precedingSurface);
    if (!romaji.empty()) {
        appendUniqueCandidate(outCandidates, candidateIndices, {
            romaji,
            normalizedReading,
            kRomajiCandidateScore + syntheticGrammarScore,
        });
    }

    const std::u16string katakana = toKatakana(reading);
    if (!katakana.empty()) {
        appendUniqueCandidate(outCandidates, candidateIndices, {
            katakana,
            normalizedReading,
            kKatakanaCandidateScore + syntheticGrammarScore,
        });
    }

    const size_t digitPrefixLength = asciiDigitPrefixLength(reading);
    if (digitPrefixLength == 0) {
        return;
    }

    const std::u16string_view digits = reading.substr(0, digitPrefixLength);
    const std::u16string_view suffix = reading.substr(digitPrefixLength);

    if (suffix.empty()) {
        const std::u16string fullWidthDigits = toFullWidthDigits(digits);
        appendUniqueCandidate(outCandidates, candidateIndices, {
            fullWidthDigits,
            normalizedReading,
            kFullWidthNumberCandidateScore,
        });

        const std::u16string groupedDigits = insertThousandsSeparators(digits, false);
        if (groupedDigits != normalizedReading) {
            appendUniqueCandidate(outCandidates, candidateIndices, {
                groupedDigits,
                normalizedReading,
                kGroupedNumberCandidateScore,
            });
        }

        if (fullWidthDigits.size() > 3) {
            const std::u16string fullWidthGroupedDigits = insertThousandsSeparators(fullWidthDigits, true);
            if (fullWidthGroupedDigits != fullWidthDigits) {
                appendUniqueCandidate(outCandidates, candidateIndices, {
                    fullWidthGroupedDigits,
                    normalizedReading,
                    kFullWidthGroupedNumberCandidateScore,
                });
            }
        }

        int year = 0;
        int month = 0;
        int day = 0;
        if (tryParseCompactDate(digits, year, month, day)) {
            appendUniqueCandidate(outCandidates, candidateIndices, {
                formatSlashDate(year, month, day),
                normalizedReading,
                kSlashDateCandidateScore,
            });
            appendUniqueCandidate(outCandidates, candidateIndices, {
                formatJapaneseDate(year, month, day),
                normalizedReading,
                kJapaneseDateCandidateScore,
            });
        }

        if (tryParseCompactYearMonth(digits, year, month)) {
            appendUniqueCandidate(outCandidates, candidateIndices, {
                formatSlashYearMonth(year, month),
                normalizedReading,
                kSlashYearMonthCandidateScore,
            });
            appendUniqueCandidate(outCandidates, candidateIndices, {
                formatJapaneseYearMonth(year, month),
                normalizedReading,
                kJapaneseYearMonthCandidateScore,
            });
        }

        const std::u16string kanjiDigits = toSimpleKanjiDigits(digits);
        appendUniqueCandidate(outCandidates, candidateIndices, {
            kanjiDigits,
            normalizedReading,
            kKanjiNumberCandidateScore,
        });
        return;
    }

    if (suffix == u"\u3048\u3093") {
        appendUniqueCandidate(outCandidates, candidateIndices, {
            insertThousandsSeparators(digits, false) + u"\u5186",
            normalizedReading,
            kCurrencyCandidateScore,
        });
        appendUniqueCandidate(outCandidates, candidateIndices, {
            toFullWidthDigits(digits) + u"\u5186",
            normalizedReading,
            kFullWidthNumberCandidateScore,
        });
    }

    if (suffix == u"\u3058") {
        appendUniqueCandidate(outCandidates, candidateIndices, {
            std::u16string(digits) + u"\u6642",
            normalizedReading,
            kTimeCandidateScore,
        });
    }
}

bool recoverCorruptFile(const std::filesystem::path& path, std::string_view label) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        YUME_LOG_ERROR("Dictionary", "exists check failed for corrupt ", label, " path=", path.string());
        return false;
    }
    if (!exists) {
        return true;
    }

    const auto timestamp = std::to_wstring(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const std::filesystem::path backupPath = path.native() + std::wstring(L".corrupt.") + timestamp;
    std::filesystem::rename(path, backupPath, ec);
    if (ec) {
        YUME_LOG_ERROR("Dictionary", "failed to quarantine corrupt ", label, " path=", path.string());
        return false;
    }

    YUME_LOG_WARN(
        "Dictionary",
        "quarantined corrupt ",
        label,
        " path=",
        path.string(),
        " backup=",
        backupPath.string());
    return true;
}

}
