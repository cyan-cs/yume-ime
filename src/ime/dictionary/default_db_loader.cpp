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



#include "ime/dictionary/default_db.hpp"

#include "ime/dictionary/default_db_helpers.hpp"
#include "utils/logger.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace yume::ime::dictionary {

namespace {

struct DefaultEntrySpec {
    ReadingView reading;
    SurfaceView text;
    int32_t score;
    PartOfSpeech partOfSpeech = PartOfSpeech::Unknown;
    LexiconCategory category = LexiconCategory::None;
};

constexpr std::array<DefaultEntrySpec, 12> kFallbackEntrySpecs = {{
    {u"\u304D\u3087\u3046", u"\u4ECA\u65E5", 100},
    {u"\u304D\u3087\u3046", u"\u5F37", 50},
    {u"\u304D\u3087\u3046", u"\u6559", 50},
    {u"\u304D\u3087\u3046", u"\u4EAC", 20},
    {u"\u308F\u305F\u3057", u"\u79C1", 100},
    {u"\u308F\u305F\u3057", u"\u6E21\u3057", 50},
    {u"\u306B\u307B\u3093", u"\u65E5\u672C", 100},
    {u"\u306B\u307B\u3093", u"\u4E8C\u672C", 40},
    {u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E", 100},
    {u"\u3068\u3046\u304D\u3087\u3046", u"\u6771\u4EAC", 100},
    {u"\u3068\u3046\u304D\u3087\u3046", u"\u6771\u4EAC\u90FD", 70},
    {u"\u306B\u307B\u3093\u3058\u3093", u"\u65E5\u672C\u4EBA", 100},
}};

constexpr int32_t kPlaceNameScoreCap = 40;
constexpr int32_t kCurrencyCodeScoreCap = 80;

}
void DefaultDb::buildFallbackEntries(Storage& storage) {
    storage.entries.clear();
    storage.entries.reserve(kFallbackEntrySpecs.size());
    for (const auto& spec : kFallbackEntrySpecs) {
        storage.entries.push_back(
            {
                std::u16string(spec.reading),
                std::u16string(spec.text),
                spec.score,
                spec.partOfSpeech,
                spec.category,
            });
    }
}

PartOfSpeech partOfSpeechFromFolderName(std::string folderName) {
    folderName = detail::trimAscii(std::move(folderName));
    std::transform(folderName.begin(), folderName.end(), folderName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (folderName == "noun") {
        return PartOfSpeech::Noun;
    }
    if (folderName == "verb") {
        return PartOfSpeech::Verb;
    }
    if (folderName == "adjective") {
        return PartOfSpeech::Adjective;
    }
    if (folderName == "adverb") {
        return PartOfSpeech::Adverb;
    }
    if (folderName == "particle") {
        return PartOfSpeech::Particle;
    }
    if (folderName == "copula") {
        return PartOfSpeech::Copula;
    }
    if (folderName == "ending") {
        return PartOfSpeech::Ending;
    }
    if (folderName == "modal") {
        return PartOfSpeech::Modal;
    }
    return PartOfSpeech::Unknown;
}

std::string normalizeFolderName(std::string folderName) {
    folderName = detail::trimAscii(std::move(folderName));
    std::transform(folderName.begin(), folderName.end(), folderName.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return folderName;
}

PartOfSpeech inferPartOfSpeechFromPath(
    const std::filesystem::path& lexiconRoot,
    const std::filesystem::path& lexiconPath) {
    std::error_code ec;
    const bool rootIsDirectory = std::filesystem::is_directory(lexiconRoot, ec);
    if (!ec && rootIsDirectory) {
        const auto relative = std::filesystem::relative(lexiconPath, lexiconRoot, ec);
        if (!ec) {
            for (const auto& component : relative) {
                if (component == relative.filename()) {
                    break;
                }
                const auto partOfSpeech = partOfSpeechFromFolderName(component.string());
                if (partOfSpeech != PartOfSpeech::Unknown) {
                    return partOfSpeech;
                }
            }
        }
    }

    return partOfSpeechFromFolderName(lexiconPath.parent_path().filename().string());
}

LexiconCategory inferCategoryFromPath(
    const std::filesystem::path& lexiconRoot,
    const std::filesystem::path& lexiconPath) {
    std::error_code ec;
    const bool rootIsDirectory = std::filesystem::is_directory(lexiconRoot, ec);
    if (!ec && rootIsDirectory) {
        const auto relative = std::filesystem::relative(lexiconPath, lexiconRoot, ec);
        if (!ec) {
            bool underNoun = false;
            for (const auto& component : relative) {
                if (component == relative.filename()) {
                    break;
                }

                const auto normalized = normalizeFolderName(component.string());
                if (normalized == "noun") {
                    underNoun = true;
                    continue;
                }
                if (underNoun && normalized == "place") {
                    return LexiconCategory::PlaceName;
                }
                if (underNoun && normalized == "numeral") {
                    continue;
                }
                if (underNoun && normalized == "currency_code") {
                    return LexiconCategory::CurrencyCode;
                }
                if (underNoun && normalized == "currency_symbol") {
                    return LexiconCategory::CurrencySymbol;
                }
            }
        }
    }

    return LexiconCategory::None;
}

int32_t normalizeEntryScore(int32_t score, LexiconCategory category) {
    if (category == LexiconCategory::PlaceName) {
        return (std::min)(score, kPlaceNameScoreCap);
    }
    if (category == LexiconCategory::CurrencyCode || category == LexiconCategory::CurrencySymbol) {
        return (std::min)(score, kCurrencyCodeScoreCap);
    }
    return score;
}

bool DefaultDb::loadEntriesFromPath(Storage& storage, const std::filesystem::path& path) {
    storage.entries.clear();

    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        return loadEntriesFromFile(storage, path, path.parent_path());
    }
    if (ec || !std::filesystem::is_directory(path, ec)) {
        return false;
    }

    std::vector<std::filesystem::path> lexiconFiles;
    for (std::filesystem::recursive_directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".tsv") {
            lexiconFiles.push_back(it->path());
        }
    }
    if (ec || lexiconFiles.empty()) {
        return false;
    }

    std::sort(lexiconFiles.begin(), lexiconFiles.end());
    for (const auto& lexiconFile : lexiconFiles) {
        if (!loadEntriesFromFile(storage, lexiconFile, path)) {
            YUME_LOG_WARN("DefaultDb", "skip unreadable lexicon path=", lexiconFile.string());
        }
    }

    if (storage.entries.empty()) {
        return false;
    }

    YUME_LOG_INFO("DefaultDb", "loaded lexicon entries=", storage.entries.size(), " path=", path.string());
    return true;
}

bool DefaultDb::loadEntriesFromFile(
    Storage& storage,
    const std::filesystem::path& path,
    const std::filesystem::path& lexiconRoot) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    const PartOfSpeech partOfSpeech = inferPartOfSpeechFromPath(lexiconRoot, path);
    const LexiconCategory category = inferCategoryFromPath(lexiconRoot, path);

    std::string fileBytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (fileBytes.size() >= 3 &&
        static_cast<unsigned char>(fileBytes[0]) == 0xEF &&
        static_cast<unsigned char>(fileBytes[1]) == 0xBB &&
        static_cast<unsigned char>(fileBytes[2]) == 0xBF) {
        fileBytes.erase(0, 3);
    }

    std::istringstream lines(fileBytes);
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(lines, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto decodedLine = detail::decodeEntryText(line);
        if (decodedLine.empty()) {
            YUME_LOG_WARN("DefaultDb", "skip undecodable lexicon line path=", path.string(), " line=", lineNumber);
            continue;
        }

        detail::trimLexiconWhitespace(decodedLine);
        if (decodedLine.empty()) {
            continue;
        }

        size_t scoreEnd = decodedLine.size();
        while (scoreEnd > 0 && detail::isLexiconWhitespace(decodedLine[scoreEnd - 1])) {
            --scoreEnd;
        }

        size_t scoreBegin = scoreEnd;
        while (scoreBegin > 0 && decodedLine[scoreBegin - 1] >= u'0' && decodedLine[scoreBegin - 1] <= u'9') {
            --scoreBegin;
        }

        if (scoreBegin == scoreEnd || scoreBegin == 0 || !detail::isLexiconWhitespace(decodedLine[scoreBegin - 1])) {
            YUME_LOG_WARN("DefaultDb", "skip invalid lexicon score path=", path.string(), " line=", lineNumber);
            continue;
        }

        std::string scoreText;
        scoreText.reserve(scoreEnd - scoreBegin);
        for (size_t index = scoreBegin; index < scoreEnd; ++index) {
            scoreText.push_back(static_cast<char>(decodedLine[index]));
        }

        int32_t score = 0;
        try {
            score = std::stoi(scoreText);
        } catch (...) {
            YUME_LOG_WARN("DefaultDb", "skip invalid lexicon score path=", path.string(), " line=", lineNumber);
            continue;
        }

        auto body = decodedLine.substr(0, scoreBegin);
        detail::trimLexiconWhitespace(body);
        if (body.empty()) {
            YUME_LOG_WARN("DefaultDb", "skip empty lexicon body path=", path.string(), " line=", lineNumber);
            continue;
        }

        size_t readingEnd = 0;
        while (readingEnd < body.size() && !detail::isLexiconWhitespace(body[readingEnd])) {
            ++readingEnd;
        }
        if (readingEnd == 0 || readingEnd == body.size()) {
            YUME_LOG_WARN("DefaultDb", "skip invalid lexicon columns path=", path.string(), " line=", lineNumber);
            continue;
        }

        size_t textBegin = readingEnd;
        while (textBegin < body.size() && detail::isLexiconWhitespace(body[textBegin])) {
            ++textBegin;
        }
        if (textBegin >= body.size()) {
            YUME_LOG_WARN("DefaultDb", "skip invalid lexicon columns path=", path.string(), " line=", lineNumber);
            continue;
        }

        auto reading = body.substr(0, readingEnd);
        auto text = body.substr(textBegin);
        detail::trimLexiconWhitespace(reading);
        detail::trimLexiconWhitespace(text);
        if (reading.empty() || text.empty()) {
            YUME_LOG_WARN("DefaultDb", "skip empty lexicon columns path=", path.string(), " line=", lineNumber);
            continue;
        }

        storage.entries.push_back({
            std::move(reading),
            std::move(text),
            normalizeEntryScore(score, category),
            partOfSpeech,
            category,
        });
    }

    return true;
}

}
