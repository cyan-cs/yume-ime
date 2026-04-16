#pragma once

#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace testing {

struct TestInfo {
    const char* suite;
    const char* name;
    void (*body)();
};

class AbortCurrentTest final : public std::exception {
public:
    const char* what() const noexcept override {
        return "test aborted";
    }
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }

    bool add(const char* suite, const char* name, void (*body)()) {
        tests_.push_back(TestInfo{suite, name, body});
        return true;
    }

    const std::vector<TestInfo>& tests() const {
        return tests_;
    }

private:
    std::vector<TestInfo> tests_;
};

class TestContext {
public:
    void beginTest(const TestInfo& info) {
        current_ = &info;
        currentFailed_ = false;
        ++testCount_;
    }

    void recordFailure(const char* file, int line, const std::string& message) {
        currentFailed_ = true;
        ++failureCount_;
        std::cerr << file << ":" << line << ": failure";
        if (current_ != nullptr) {
            std::cerr << " in " << current_->suite << "." << current_->name;
        }
        std::cerr << ": " << message << "\n";
    }

    void endTest() {
        if (!currentFailed_) {
            ++passedCount_;
        }
        current_ = nullptr;
    }

    int failureCount() const {
        return failureCount_;
    }

    int passedCount() const {
        return passedCount_;
    }

    int testCount() const {
        return testCount_;
    }

private:
    const TestInfo* current_ = nullptr;
    int failureCount_ = 0;
    int passedCount_ = 0;
    int testCount_ = 0;
    bool currentFailed_ = false;
};

inline TestContext*& currentContextSlot() {
    static TestContext* ctx = nullptr;
    return ctx;
}

inline TestContext& currentContext() {
    return *currentContextSlot();
}

inline bool registerTest(const char* suite, const char* name, void (*body)()) {
    return TestRegistry::instance().add(suite, name, body);
}

template <typename T, typename = void>
struct IsStreamable : std::false_type {};

template <typename T>
struct IsStreamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T>
std::string renderValue(const T& value) {
    if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return value ? "true" : "false";
    } else if constexpr (IsStreamable<T>::value) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else {
        return "<value>";
    }
}

template <typename Left, typename Right>
void expectEq(const Left& left,
              const Right& right,
              const char* leftExpr,
              const char* rightExpr,
              const char* file,
              int line) {
    if (!(left == right)) {
        currentContext().recordFailure(
            file,
            line,
            std::string(leftExpr) + " == " + rightExpr + " (actual: " + renderValue(left) +
                " vs " + renderValue(right) + ")");
    }
}

template <typename Left, typename Right>
void expectNe(const Left& left,
              const Right& right,
              const char* leftExpr,
              const char* rightExpr,
              const char* file,
              int line) {
    if (!(left != right)) {
        currentContext().recordFailure(
            file,
            line,
            std::string(leftExpr) + " != " + rightExpr + " (both: " + renderValue(left) + ")");
    }
}

inline void expectTrue(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        currentContext().recordFailure(file, line, std::string(expr) + " is false");
    }
}

inline void expectFalse(bool condition, const char* expr, const char* file, int line) {
    if (condition) {
        currentContext().recordFailure(file, line, std::string(expr) + " is true");
    }
}

inline void assertTrue(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        currentContext().recordFailure(file, line, std::string(expr) + " is false");
        throw AbortCurrentTest();
    }
}

inline void assertFalse(bool condition, const char* expr, const char* file, int line) {
    if (condition) {
        currentContext().recordFailure(file, line, std::string(expr) + " is true");
        throw AbortCurrentTest();
    }
}

inline int InitGoogleTest(int*, char**) {
    return 0;
}

inline int RUN_ALL_TESTS() {
    TestContext context;
    currentContextSlot() = &context;

    for (const auto& test : TestRegistry::instance().tests()) {
        context.beginTest(test);
        try {
            test.body();
        } catch (const AbortCurrentTest&) {
        } catch (const std::exception& ex) {
            context.recordFailure(__FILE__, __LINE__, std::string("unhandled exception: ") + ex.what());
        } catch (...) {
            context.recordFailure(__FILE__, __LINE__, "unhandled non-standard exception");
        }
        context.endTest();
    }

    std::cout << "[==========] " << context.testCount() << " tests ran.\n";
    std::cout << "[  PASSED  ] " << context.passedCount() << " tests.\n";
    if (context.failureCount() != 0) {
        std::cout << "[  FAILED  ] " << context.failureCount() << " assertions.\n";
    }

    currentContextSlot() = nullptr;
    return context.failureCount() == 0 ? 0 : 1;
}

} // namespace testing

#define TEST(SuiteName, TestName)                                                                    \
    static void SuiteName##_##TestName##_Body();                                                     \
    namespace {                                                                                      \
    const bool SuiteName##_##TestName##_Registered =                                                 \
        ::testing::registerTest(#SuiteName, #TestName, &SuiteName##_##TestName##_Body);             \
    }                                                                                                \
    static void SuiteName##_##TestName##_Body()

#define EXPECT_EQ(left, right)                                                                       \
    do {                                                                                             \
        const auto& gtest_left = (left);                                                             \
        const auto& gtest_right = (right);                                                           \
        ::testing::expectEq(gtest_left, gtest_right, #left, #right, __FILE__, __LINE__);            \
    } while (false)

#define EXPECT_NE(left, right)                                                                       \
    do {                                                                                             \
        const auto& gtest_left = (left);                                                             \
        const auto& gtest_right = (right);                                                           \
        ::testing::expectNe(gtest_left, gtest_right, #left, #right, __FILE__, __LINE__);            \
    } while (false)

#define EXPECT_TRUE(condition)                                                                       \
    do {                                                                                             \
        ::testing::expectTrue(static_cast<bool>(condition), #condition, __FILE__, __LINE__);         \
    } while (false)

#define EXPECT_FALSE(condition)                                                                      \
    do {                                                                                             \
        ::testing::expectFalse(static_cast<bool>(condition), #condition, __FILE__, __LINE__);        \
    } while (false)

#define ASSERT_TRUE(condition)                                                                       \
    do {                                                                                             \
        ::testing::assertTrue(static_cast<bool>(condition), #condition, __FILE__, __LINE__);         \
    } while (false)

#define ASSERT_FALSE(condition)                                                                      \
    do {                                                                                             \
        ::testing::assertFalse(static_cast<bool>(condition), #condition, __FILE__, __LINE__);        \
    } while (false)
