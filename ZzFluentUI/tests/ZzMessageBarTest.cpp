#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtGui/QEnterEvent>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMessageSeverity.h>

/** @brief 为消息条 LanguageChange 测试提供确定翻译。 */
class ZzMessageBarTranslator final : public QTranslator
{
public:
    /** @brief 只翻译关闭按钮文本，其余文本返回空值。 */
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
            && std::strcmp(sourceText, "关闭") == 0) {
            return QStringLiteral("Translated close");
        }
        return {};
    }
};

/** @brief 验证消息条属性、关闭意图、暂停计时和翻译生命周期。 */
class ZzMessageBarTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 向可见消息条发送确定的鼠标离开事件。 */
    static void sendLeave(ZzFluentUI::ZzMessageBar *bar)
    {
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(bar, &leave);
    }

private Q_SLOTS:
    void updatesContentSeverityAndCloseButton()
    {
        ZzFluentUI::ZzMessageBar bar;
        QSignalSpy textSpy(&bar, &ZzFluentUI::ZzMessageBar::textChanged);
        QSignalSpy severitySpy(
            &bar,
            &ZzFluentUI::ZzMessageBar::severityChanged);
        QSignalSpy closableSpy(
            &bar,
            &ZzFluentUI::ZzMessageBar::closableChanged);

        bar.setText(QStringLiteral("Saved"));
        bar.setText(QStringLiteral("Saved"));
        bar.setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
        bar.setSeverity(ZzFluentUI::ZzMessageSeverity::Success);
        auto *closeButton = bar.findChild<QToolButton *>(
            QStringLiteral("zzMessageBarCloseButton"));

        QCOMPARE(bar.text(), QStringLiteral("Saved"));
        QCOMPARE(
            bar.severity(),
            ZzFluentUI::ZzMessageSeverity::Success);
        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(severitySpy.count(), 1);
        QVERIFY(closeButton != nullptr);
        QCOMPARE(closeButton->accessibleName(), QStringLiteral("关闭"));
        QCOMPARE(bar.accessibleName(), QStringLiteral("Saved"));

        bar.setClosable(false);
        bar.setClosable(false);
        QCOMPARE(closableSpy.count(), 1);
        QVERIFY(closeButton->isHidden());
    }

    void escapeRequestsCloseOnlyOnce()
    {
        ZzFluentUI::ZzMessageBar bar;
        bar.show();
        QCoreApplication::processEvents();
        QSignalSpy closeSpy(
            &bar,
            &ZzFluentUI::ZzMessageBar::closeRequested);

        QTest::keyClick(&bar, Qt::Key_Escape);
        QTest::keyClick(&bar, Qt::Key_Escape);

        QCOMPARE(closeSpy.count(), 1);
        QVERIFY(bar.isVisible());
    }

    void timeoutRequestsCloseOnce()
    {
        ZzFluentUI::ZzMessageBar bar;
        QSignalSpy closeSpy(
            &bar,
            &ZzFluentUI::ZzMessageBar::closeRequested);
        bar.setTimeoutMilliseconds(30);
        bar.show();
        QCoreApplication::processEvents();
        sendLeave(&bar);

        QVERIFY(QTest::qWaitFor(
            [&closeSpy] { return closeSpy.count() == 1; }, 500));
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(bar.findChildren<QTimer *>().size(), 1);
        QVERIFY(bar.isVisible());
    }

    void hoverPausesAndResumesTimeout()
    {
        ZzFluentUI::ZzMessageBar bar;
        QSignalSpy closeSpy(
            &bar,
            &ZzFluentUI::ZzMessageBar::closeRequested);
        bar.setTimeoutMilliseconds(100);
        bar.show();
        QCoreApplication::processEvents();
        sendLeave(&bar);
        auto *timeoutTimer = bar.findChild<QTimer *>();
        QVERIFY(timeoutTimer != nullptr);
        QVERIFY(timeoutTimer->isActive());

        QEnterEvent enter(
            QPointF(1.0, 1.0),
            QPointF(1.0, 1.0),
            QPointF(1.0, 1.0));
        QCoreApplication::sendEvent(&bar, &enter);
        QVERIFY(!timeoutTimer->isActive());

        QTest::qWait(130);
        QCOMPARE(closeSpy.count(), 0);

        sendLeave(&bar);
        QVERIFY(timeoutTimer->isActive());
        QVERIFY(QTest::qWaitFor(
            [&closeSpy] { return closeSpy.count() == 1; }, 500));
        QCOMPARE(closeSpy.count(), 1);
    }

    void hiddenBarDoesNotConsumeTimeout()
    {
        ZzFluentUI::ZzMessageBar bar;
        QSignalSpy closeSpy(
            &bar,
            &ZzFluentUI::ZzMessageBar::closeRequested);
        bar.setTimeoutMilliseconds(40);
        bar.show();
        QCoreApplication::processEvents();
        sendLeave(&bar);
        bar.hide();

        QTimer hiddenWindow;
        hiddenWindow.setSingleShot(true);
        QSignalSpy hiddenWindowSpy(&hiddenWindow, &QTimer::timeout);
        hiddenWindow.start(80);
        QVERIFY(QTest::qWaitFor(
            [&hiddenWindowSpy] { return hiddenWindowSpy.count() == 1; }, 500));
        QCOMPARE(hiddenWindowSpy.count(), 1);
        QCOMPARE(closeSpy.count(), 0);

        bar.show();
        QCoreApplication::processEvents();
        sendLeave(&bar);
        QVERIFY(QTest::qWaitFor(
            [&closeSpy] { return closeSpy.count() == 1; }, 500));
        QCOMPARE(closeSpy.count(), 1);
    }

    void refreshesTranslatedClosePresentation()
    {
        ZzFluentUI::ZzMessageBar bar;
        auto *closeButton = bar.findChild<QToolButton *>(
            QStringLiteral("zzMessageBarCloseButton"));
        QVERIFY(closeButton != nullptr);
        ZzMessageBarTranslator translator;
        QCoreApplication::installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&bar, &languageChange);

        QCOMPARE(
            closeButton->accessibleName(),
            QStringLiteral("Translated close"));
        QCOMPARE(
            closeButton->toolTip(),
            QStringLiteral("Translated close"));

        QCoreApplication::removeTranslator(&translator);
    }
};

QTEST_MAIN(ZzMessageBarTest)

#include "ZzMessageBarTest.moc"
