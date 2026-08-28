#pragma once

#include <QtTest/QTest>

/**
 * @brief 等待布尔表达式成立并执行最终断言。
 * @param expression 可在 Qt 事件循环推进后变为真的表达式。
 *
 * Qt 6.8 的 QTRY 宏会把 chrono 计数隐式转换为 int，在严格的
 * -Wconversion 配置下无法编译。该断言直接使用 QTest::qWaitFor，既保留
 * 异步等待语义，也避免依赖存在版本差异的宏实现。
 */
#define ZZ_VERIFY_EVENTUALLY(expression)                                    \
    do {                                                                    \
        (void)QTest::qWaitFor(                                              \
            [&] { return static_cast<bool>(expression); });                 \
        QVERIFY(expression);                                                \
    } while (false)

/**
 * @brief 等待两个表达式相等并输出最终比较诊断。
 * @param actual 实际值表达式。
 * @param expected 期望值表达式。
 */
#define ZZ_COMPARE_EVENTUALLY(actual, expected)                             \
    do {                                                                    \
        (void)QTest::qWaitFor([&] { return (actual) == (expected); });       \
        QCOMPARE(actual, expected);                                         \
    } while (false)

/**
 * @brief 在指定毫秒数内等待两个表达式相等并输出最终比较诊断。
 * @param actual 实际值表达式。
 * @param expected 期望值表达式。
 * @param timeout 超时时间，单位为毫秒。
 */
#define ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(actual, expected, timeout)       \
    do {                                                                    \
        (void)QTest::qWaitFor(                                              \
            [&] { return (actual) == (expected); },                         \
            static_cast<int>(timeout));                                     \
        QCOMPARE(actual, expected);                                         \
    } while (false)
