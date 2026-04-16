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

namespace {

template <typename Database>
PartOfSpeech lookupEntryPartOfSpeech(const Database& database, ReadingView reading, SurfaceView text) {
    if (const auto* exact = database.findExact(reading)) {
        for (const EntryIndex index : *exact) {
            const auto& entry = database.entryAt(index);
            if (entry.text == text) {
                return entry.partOfSpeech;
            }
        }
    }
    return PartOfSpeech::Unknown;
}

}

PartOfSpeech Dictionary::lookupPartOfSpeech(ReadingView reading, SurfaceView text) const {
    if (reading.empty() || text.empty()) {
        return PartOfSpeech::Unknown;
    }

    std::scoped_lock lock(mutex);
    if (const auto userPos = lookupEntryPartOfSpeech(userDb, reading, text);
        userPos != PartOfSpeech::Unknown) {
        return userPos;
    }
    return lookupEntryPartOfSpeech(defaultDb(), reading, text);
}

PartOfSpeech Dictionary::lastCommittedPartOfSpeech() const {
    std::scoped_lock lock(mutex);
    return lastCommittedPartOfSpeechValue;
}

SurfaceText Dictionary::lastCommittedSurface() const {
    std::scoped_lock lock(mutex);
    return lastCommittedSurfaceText;
}

void Dictionary::recordCommit(const ReadingText& reading, const SurfaceText& text) {
    if (reading.empty() || text.empty()) {
        YUME_LOG_WARN("Dictionary", "ignore empty recordCommit readingSize=", reading.size(), " textSize=", text.size());
        return;
    }

    std::scoped_lock lock(mutex);
    if (!lastCommittedSurfaceText.empty()) {
        userDb.recordContextTransition(lastCommittedSurfaceText, reading, text);
    }
    userDb.recordCommit(reading, text);
    lastCommittedPartOfSpeechValue = lookupEntryPartOfSpeech(userDb, reading, text);
    if (lastCommittedPartOfSpeechValue == PartOfSpeech::Unknown) {
        lastCommittedPartOfSpeechValue = lookupEntryPartOfSpeech(defaultDb(), reading, text);
    }
    lastCommittedSurfaceText = text;
    ++contextGeneration;
    clearCaches();
    isUserDbDirty = true;
    ++pendingUserDbCommits;
    lastUserDbCommitTime = std::chrono::steady_clock::now();
    flushUserDbIfDue();
}

void Dictionary::blockCandidate(ReadingView reading, SurfaceView text) {
    if (reading.empty() || text.empty()) {
        YUME_LOG_WARN("Dictionary", "ignore empty blockCandidate readingSize=", reading.size(), " textSize=", text.size());
        return;
    }

    YUME_LOG_INFO("Dictionary", "blockCandidate readingSize=", reading.size(), " textSize=", text.size());
    BlackDb blackDbSnapshot;
    std::filesystem::path blackDbPath;
    uint64_t snapshotGeneration = 0;
    {
        std::scoped_lock lock(mutex);
        blackDb.block(reading, text);
        clearCaches();
        blackDbSnapshot = blackDb;
        blackDbPath = storagePaths.blackDbPath;
        snapshotGeneration = blackDb.generation();
    }

    std::scoped_lock saveLock(blackDbSaveMutex);
    if (!blackDbWritable) {
        YUME_LOG_ERROR("Dictionary", "blackDb persistence disabled path=", blackDbPath.string());
        return;
    }
    if (snapshotGeneration <= lastBlackDbSavedGeneration) {
        return;
    }
    if (blackDbSnapshot.saveToFile(blackDbPath)) {
        lastBlackDbSavedGeneration = snapshotGeneration;
    } else {
        YUME_LOG_ERROR("Dictionary", "failed to save blackDb path=", blackDbPath.string());
    }
}

void Dictionary::servicePersistence() const {
    std::scoped_lock lock(mutex);
    flushUserDbIfDue();
}

bool Dictionary::flush() const {
    std::scoped_lock lock(mutex);
    const bool userFlushed = flushUserDbNow();
    bool blackFlushed = false;
    {
        std::scoped_lock saveLock(blackDbSaveMutex);
        if (!blackDbWritable) {
            YUME_LOG_ERROR("Dictionary", "blackDb persistence disabled path=", storagePaths.blackDbPath.string());
            blackFlushed = false;
        } else {
            blackFlushed = blackDb.saveToFile(storagePaths.blackDbPath);
            if (blackFlushed) {
                lastBlackDbSavedGeneration = blackDb.generation();
            }
        }
    }
    YUME_LOG_INFO("Dictionary", "flush userFlushed=", userFlushed, " blackFlushed=", blackFlushed);
    return userFlushed && blackFlushed;
}

Dictionary::SessionSnapshot Dictionary::captureSessionSnapshot() const {
    std::scoped_lock lock(mutex);
    SessionSnapshot snapshot;
    snapshot.userDb = userDb;
    snapshot.blackDb = blackDb;
    snapshot.isUserDbDirty = isUserDbDirty;
    snapshot.pendingUserDbCommits = pendingUserDbCommits;
    snapshot.lastBlackDbSavedGeneration = lastBlackDbSavedGeneration;
    snapshot.contextGeneration = contextGeneration;
    snapshot.lastCommittedPartOfSpeech = lastCommittedPartOfSpeechValue;
    snapshot.lastCommittedSurfaceText = lastCommittedSurfaceText;
    snapshot.userDbWritable = userDbWritable;
    snapshot.blackDbWritable = blackDbWritable;
    snapshot.lastUserDbCommitTime = lastUserDbCommitTime;
    snapshot.lastUserDbFlushTime = lastUserDbFlushTime;
    return snapshot;
}

void Dictionary::restoreSessionSnapshot(const SessionSnapshot& snapshot) {
    std::scoped_lock lock(mutex);
    userDb = snapshot.userDb;
    blackDb = snapshot.blackDb;
    isUserDbDirty = snapshot.isUserDbDirty;
    pendingUserDbCommits = snapshot.pendingUserDbCommits;
    lastBlackDbSavedGeneration = snapshot.lastBlackDbSavedGeneration;
    contextGeneration = snapshot.contextGeneration;
    lastCommittedPartOfSpeechValue = snapshot.lastCommittedPartOfSpeech;
    lastCommittedSurfaceText = snapshot.lastCommittedSurfaceText;
    userDbWritable = snapshot.userDbWritable;
    blackDbWritable = snapshot.blackDbWritable;
    lastUserDbCommitTime = snapshot.lastUserDbCommitTime;
    lastUserDbFlushTime = snapshot.lastUserDbFlushTime;
    clearCaches();
}

std::chrono::steady_clock::duration Dictionary::currentUserDbFlushInterval() const {
    if (!isUserDbDirty) {
        return detail::kMaxUserDbFlushInterval;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto idleFor = now - lastUserDbCommitTime;
    if (idleFor >= detail::kMinUserDbFlushInterval) {
        return detail::kMinUserDbFlushInterval;
    }

    if (pendingUserDbCommits >= 24) {
        return detail::kMinUserDbFlushInterval;
    }
    if (pendingUserDbCommits >= 12) {
        return std::chrono::seconds(10);
    }
    if (pendingUserDbCommits >= 6) {
        return std::chrono::seconds(20);
    }

    return detail::kMaxUserDbFlushInterval;
}

void Dictionary::flushUserDbIfDue() const {
    if (!isUserDbDirty) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - lastUserDbFlushTime >= currentUserDbFlushInterval()) {
        flushUserDbNow();
    }
}

bool Dictionary::flushUserDbNow() const {
    if (!isUserDbDirty) {
        return true;
    }
    if (!userDbWritable) {
        YUME_LOG_ERROR("Dictionary", "userDb persistence disabled path=", storagePaths.userDbPath.string());
        return false;
    }

    if (!userDb.saveToFile(storagePaths.userDbPath)) {
        YUME_LOG_ERROR("Dictionary", "userDb save failed path=", storagePaths.userDbPath.string());
        return false;
    }

    isUserDbDirty = false;
    pendingUserDbCommits = 0;
    lastUserDbFlushTime = std::chrono::steady_clock::now();
    return true;
}

}
