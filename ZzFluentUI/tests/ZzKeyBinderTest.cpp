#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTranslator>
#include <QtGui/QAccessible>
#include <QtGui/QKeyEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzKeyBinder.h>

/** @brief 为快捷键录制器操作说明提供确定翻译。 */
class ZzKeyBinderTranslator final : public QTranslator
{
public:
    /** @brief 声明测试翻译器包含可安装的内存翻译。 */
    [[nodiscard]] bool isEmpty() const override
    {
        return false;
    }

    /** @brief 翻译录制、取消和清空提示。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (sourceText != nullptr
            && std::strcmp(
                   sourceText,
                   "按下快捷键；Escape 取消；Backspace 清除")
                == 0) {
            return QStringLiteral("Translated key recording help");
        }
        return {};
    }
};

/** @brief 验证快捷键录制事务、Qt 原生键序列和对象预算。 */
class ZzKeyBinderTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 返回测试使用的 Ctrl+Shift+P 跨平台键序列。 */
    static QKeySequence primarySequence()
    {
        return QKeySequence(QKeyCombination(
            Qt::ControlModifier | Qt::ShiftModifier,
            Qt::Key_P));
    }

    /** @brief 返回测试使用的 Ctrl+O 跨平台键序列。 */
    static QKeySequence originalSequence()
    {
        return QKeySequence(QKeyCombination(
            Qt::ControlModifier,
            Qt::Key_O));
    }

    /** @brief 在 Qt 原生无障碍子树中查找指定键序列文本。 */
    static bool containsAccessibleValue(
        QAccessibleInterface *interface,
        const QString &expected)
    {
        if (interface == nullptr) {
            return false;
        }
        if (interface->text(QAccessible::Value) == expected) {
            return true;
        }
        for (int index = 0; index < interface->childCount(); ++index) {
            if (containsAccessibleValue(
                    interface->child(index),
                    expected)) {
                return true;
            }
        }
        return false;
    }

private Q_SLOTS:
    void constructorsAndPublicQtPropertiesRoundTrip()
    {
        ZzFluentUI::ZzKeyBinder empty;
        QCOMPARE(empty.maximumSequenceLength(), 1);
        QCOMPARE(empty.focusPolicy(), Qt::StrongFocus);
        QVERIFY(empty.keySequence().isEmpty());
        QVERIFY(!empty.isRecording());

        ZzFluentUI::ZzKeyBinder preset(primarySequence());
        QCOMPARE(preset.maximumSequenceLength(), 1);
        QCOMPARE(preset.keySequence(), primarySequence());
        preset.setClearButtonEnabled(true);
        QVERIFY(preset.isClearButtonEnabled());

        preset.setMaximumSequenceLength(2);
        QCOMPARE(preset.maximumSequenceLength(), 2);
        const QList<QKeyCombination> finishing{
            QKeyCombination(Qt::NoModifier, Qt::Key_Tab),
            QKeyCombination(Qt::NoModifier, Qt::Key_Backtab)};
        preset.setFinishingKeyCombinations(finishing);
        QCOMPARE(preset.finishingKeyCombinations(), finishing);

        preset.setKeySequence(originalSequence());
        QCOMPARE(preset.keySequence(), originalSequence());
        QVERIFY(!preset.isRecording());

        preset.setEnabled(false);
        preset.startRecording();
        QVERIFY(!preset.isRecording());
    }

    void startAndCancelAreIdempotent()
    {
        ZzFluentUI::ZzKeyBinder binder(originalSequence());
        QSignalSpy stateSpy(
            &binder,
            &ZzFluentUI::ZzKeyBinder::recordingChanged);
        QSignalSpy canceledSpy(
            &binder,
            &ZzFluentUI::ZzKeyBinder::recordingCanceled);
        QSignalSpy acceptedSpy(
            &binder,
            &ZzFluentUI::ZzKeyBinder::recordingAccepted);

        binder.startRecording();
        binder.startRecording();
        QVERIFY(binder.isRecording());
        QCOMPARE(stateSpy.size(), 1);

        binder.setKeySequence(primarySequence());
        binder.cancelRecording();
        binder.cancelRecording();
        QVERIFY(!binder.isRecording());
        QCOMPARE(binder.keySequence(), originalSequence());
        QCOMPARE(stateSpy.size(), 2);
        QCOMPARE(canceledSpy.size(), 1);
        QCOMPARE(acceptedSpy.size(), 0);
    }

    void recordsModifierSequenceAndAcceptsOnFocusOut()
    {
        QWidget host;
        auto *layout = new QHBoxLayout(&host);
        ZzFluentUI::ZzKeyBinder binder;
        QWidget next;
        next.setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(&binder);
        layout->addWidget(&next);
        host.show();
        binder.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(binder.isRecording());

        QSignalSpy acceptedSpy(
            &binder,
            &ZzFluentUI::ZzKeyBinder::recordingAccepted);
        QTest::keyClick(
            &binder,
            Qt::Key_P,
            Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(binder.keySequence(), primarySequence());

        next.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(!binder.isRecording());
        QCOMPARE(acceptedSpy.size(), 1);
        QCOMPARE(
            acceptedSpy.at(0).at(0).value<QKeySequence>(),
            primarySequence());
    }

    void escapeRestoresSnapshotAndBackspaceClears()
    {
        QWidget host;
        auto *layout = new QHBoxLayout(&host);
        ZzFluentUI::ZzKeyBinder binder(originalSequence());
        binder.setMaximumSequenceLength(2);
        QWidget next;
        next.setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(&binder);
        layout->addWidget(&next);
        host.show();
        binder.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();

        QSignalSpy canceledSpy(
            &binder,
            &ZzFluentUI::ZzKeyBinder::recordingCanceled);
        QSignalSpy acceptedSpy(
            &binder,
            &ZzFluentUI::ZzKeyBinder::recordingAccepted);
        QTest::keyClick(
            &binder,
            Qt::Key_P,
            Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(binder.keySequence(), primarySequence());
        QTest::keyClick(&binder, Qt::Key_Escape);
        QVERIFY(!binder.isRecording());
        QCOMPARE(binder.keySequence(), originalSequence());
        QCOMPARE(canceledSpy.size(), 1);
        QCOMPARE(acceptedSpy.size(), 0);

        QTest::keyClick(&binder, Qt::Key_Backspace);
        QVERIFY(binder.isRecording());
        QVERIFY(binder.keySequence().isEmpty());
        next.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QCOMPARE(acceptedSpy.size(), 1);
        QVERIFY(
            acceptedSpy.at(0).at(0).value<QKeySequence>().isEmpty());
    }

    void qtHandlesMultipleChordsAndAutoRepeat()
    {
        QWidget host;
        auto *layout = new QHBoxLayout(&host);
        ZzFluentUI::ZzKeyBinder binder;
        QWidget next;
        next.setFocusPolicy(Qt::StrongFocus);
        layout->addWidget(&binder);
        layout->addWidget(&next);
        binder.setMaximumSequenceLength(2);
        host.show();
        binder.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();

        QTest::keyClick(&binder, Qt::Key_K, Qt::ControlModifier);
        QTest::keyClick(&binder, Qt::Key_C, Qt::ControlModifier);
        const QKeySequence expected(
            QKeyCombination(Qt::ControlModifier, Qt::Key_K),
            QKeyCombination(Qt::ControlModifier, Qt::Key_C));
        QCOMPARE(binder.keySequence(), expected);

        binder.setMaximumSequenceLength(1);
        binder.clear();
        QKeyEvent repeatPress(
            QEvent::KeyPress,
            Qt::Key_P,
            Qt::ControlModifier,
            QStringLiteral("p"),
            true,
            2);
        QCoreApplication::sendEvent(&binder, &repeatPress);
        QCOMPARE(
            binder.keySequence(),
            QKeySequence(QKeyCombination(
                Qt::ControlModifier,
                Qt::Key_P)));
        next.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(!binder.isRecording());
    }

    void languageChangeRefreshesHelpText()
    {
        ZzFluentUI::ZzKeyBinder binder;
        QCOMPARE(
            binder.accessibleDescription(),
            QStringLiteral("按下快捷键；Escape 取消；Backspace 清除"));
        QCOMPARE(binder.toolTip(), binder.accessibleDescription());

        ZzKeyBinderTranslator translator;
        QVERIFY(!translator.isEmpty());
        QVERIFY(QCoreApplication::installTranslator(&translator));
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&binder, &languageChange);
        QCOMPARE(
            binder.accessibleDescription(),
            QStringLiteral("Translated key recording help"));
        QCOMPARE(binder.toolTip(), binder.accessibleDescription());
        QCoreApplication::removeTranslator(&translator);
    }

    void preservesQtAccessibleNameAndNativeValue()
    {
        ZzFluentUI::ZzKeyBinder binder(primarySequence());
        binder.setAccessibleName(QStringLiteral("Build shortcut"));
        binder.show();
        QCoreApplication::processEvents();

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&binder);
        QVERIFY(interface != nullptr);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Build shortcut"));
        QVERIFY(containsAccessibleValue(
            interface,
            primarySequence().toString(QKeySequence::NativeText)));
    }

    void repeatedTransactionsKeepFixedObjectBudget()
    {
        ZzFluentUI::ZzKeyBinder binder(primarySequence());
        const qsizetype objectCount =
            binder.findChildren<QObject *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            binder.startRecording();
            binder.cancelRecording();
        }

        QCOMPARE(
            binder.findChildren<QObject *>().size(),
            objectCount);
        QCOMPARE(binder.keySequence(), primarySequence());
        QVERIFY(!binder.isRecording());
    }
};

QTEST_MAIN(ZzKeyBinderTest)

#include "ZzKeyBinderTest.moc"
