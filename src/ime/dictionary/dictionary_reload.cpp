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

#include "ime/dictionary/db_binary_io.hpp"
#include "ime/dictionary/dictionary_internal.hpp"
#include "utils/logger.hpp"

namespace yume::ime::dictionary {

namespace {

bool isMissingPath(const std::filesystem::path& path, std::string_view label) {
    return binary_io::pathStateNoThrow(path, label) == binary_io::PathState::Missing;
}

template <typename Database>
bool ensureDbFileExists(
    const Database& database,
    const std::filesystem::path& path,
    bool& writable,
    bool loaded,
    bool missing,
    std::string_view label) {
    if (!loaded || !missing) {
        return loaded;
    }

    if (database.saveToFile(path)) {
        return true;
    }

    writable = false;
    YUME_LOG_ERROR("Dictionary", "failed to create missing ", label, " path=", path.string());
    return false;
}

}

bool Dictionary::reload() {
    std::scoped_lock lock(mutex);
    defaultDb().reloadIfChanged();

    detail::CachedReloadState cachedState;
    if (detail::tryLoadCachedReloadState(storagePaths, cachedState)) {
        userDb = cachedState.snapshot.userDb;
        blackDb = cachedState.snapshot.blackDb;
        isUserDbDirty = cachedState.snapshot.isUserDbDirty;
        pendingUserDbCommits = cachedState.snapshot.pendingUserDbCommits;
        lastBlackDbSavedGeneration = cachedState.snapshot.lastBlackDbSavedGeneration;
        contextGeneration = cachedState.snapshot.contextGeneration;
        lastCommittedPartOfSpeechValue = cachedState.snapshot.lastCommittedPartOfSpeech;
        lastCommittedSurfaceText = cachedState.snapshot.lastCommittedSurfaceText;
        userDbWritable = cachedState.snapshot.userDbWritable;
        blackDbWritable = cachedState.snapshot.blackDbWritable;
        lastUserDbCommitTime = std::chrono::steady_clock::now();
        lastUserDbFlushTime = std::chrono::steady_clock::now();
        clearCaches();
        YUME_LOG_INFO(
            "Dictionary",
            "reload from cache userLoaded=",
            cachedState.userLoaded,
            " blackLoaded=",
            cachedState.blackLoaded);
        return cachedState.userLoaded && cachedState.blackLoaded;
    }

    userDbWritable = true;
    blackDbWritable = true;
    const bool userDbMissing = isMissingPath(storagePaths.userDbPath, "DictionaryReloadUserDb");
    const bool blackDbMissing = isMissingPath(storagePaths.blackDbPath, "DictionaryReloadBlackDb");

    bool userLoaded = userDb.loadFromFile(storagePaths.userDbPath);
    if (!userLoaded) {
        userDb = UserDb{};
        const bool recovered = detail::recoverCorruptFile(storagePaths.userDbPath, "userDb");
        if (!recovered) {
            userDbWritable = false;
        }
        userLoaded = recovered;
    }

    bool blackLoaded = blackDb.loadFromFile(storagePaths.blackDbPath);
    if (!blackLoaded) {
        blackDb = BlackDb{};
        const bool recovered = detail::recoverCorruptFile(storagePaths.blackDbPath, "blackDb");
        if (!recovered) {
            blackDbWritable = false;
        }
        blackLoaded = recovered;
    }

    userLoaded = ensureDbFileExists(
        userDb,
        storagePaths.userDbPath,
        userDbWritable,
        userLoaded,
        userDbMissing,
        "userDb");
    blackLoaded = ensureDbFileExists(
        blackDb,
        storagePaths.blackDbPath,
        blackDbWritable,
        blackLoaded,
        blackDbMissing,
        "blackDb");

    clearCaches();
    isUserDbDirty = false;
    pendingUserDbCommits = 0;
    contextGeneration = 0;
    lastCommittedPartOfSpeechValue = PartOfSpeech::Unknown;
    lastCommittedSurfaceText.clear();
    {
        std::scoped_lock saveLock(blackDbSaveMutex);
        lastBlackDbSavedGeneration = blackDb.generation();
    }
    lastUserDbCommitTime = std::chrono::steady_clock::now();
    lastUserDbFlushTime = std::chrono::steady_clock::now();

    detail::CachedReloadState state;
    state.userDbSignature = detail::readFileSignature(storagePaths.userDbPath);
    state.blackDbSignature = detail::readFileSignature(storagePaths.blackDbPath);
    state.snapshot.userDb = userDb;
    state.snapshot.blackDb = blackDb;
    state.snapshot.isUserDbDirty = isUserDbDirty;
    state.snapshot.pendingUserDbCommits = pendingUserDbCommits;
    state.snapshot.lastBlackDbSavedGeneration = lastBlackDbSavedGeneration;
    state.snapshot.contextGeneration = contextGeneration;
    state.snapshot.lastCommittedPartOfSpeech = lastCommittedPartOfSpeechValue;
    state.snapshot.lastCommittedSurfaceText = lastCommittedSurfaceText;
    state.snapshot.userDbWritable = userDbWritable;
    state.snapshot.blackDbWritable = blackDbWritable;
    state.snapshot.lastUserDbCommitTime = lastUserDbCommitTime;
    state.snapshot.lastUserDbFlushTime = lastUserDbFlushTime;
    state.userLoaded = userLoaded;
    state.blackLoaded = blackLoaded;
    if (state.userDbSignature.valid && state.blackDbSignature.valid) {
        detail::storeCachedReloadState(storagePaths, state);
    }

    YUME_LOG_INFO("Dictionary", "reload userLoaded=", userLoaded, " blackLoaded=", blackLoaded);
    return userLoaded && blackLoaded;
}

}
