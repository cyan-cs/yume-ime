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


#include "ime/dictionary/dictionary.hpp"

#include "ime/dictionary/dictionary_internal.hpp"
#include "utils/logger.hpp"

namespace yume::ime::dictionary {

Dictionary::Dictionary() {
    YUME_LOG_INFO(
        "Dictionary",
        "initialize userDb=",
        storagePaths.userDbPath.string(),
        " blackDb=",
        storagePaths.blackDbPath.string());
    reload();
}

Dictionary::Dictionary(DictionaryStoragePaths paths)
    : storagePaths(std::move(paths)) {
    YUME_LOG_INFO(
        "Dictionary",
        "initialize userDb=",
        storagePaths.userDbPath.string(),
        " blackDb=",
        storagePaths.blackDbPath.string());
    reload();
}

Dictionary::~Dictionary() {
    std::scoped_lock lock(mutex);
    if (!flushUserDbNow()) {
        YUME_LOG_ERROR("Dictionary", "flushUserDbNow failed in destructor");
    }
}

void Dictionary::clearCaches() const {
    exactCandidateCache.clear();
    predictionCache.clear();
}

Dictionary::CacheGeneration Dictionary::combinedGeneration() const {
    return {
        userDb.generation(),
        blackDb.generation(),
        contextGeneration,
    };
}

const DefaultDb& Dictionary::defaultDb() const {
    static const DefaultDb database;
    return database;
}

}
