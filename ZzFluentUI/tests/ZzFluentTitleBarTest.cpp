#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QTranslator>
#include <QtGui/QPixmap>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentTitleBar.h>

/** @brief 为标题栏 LanguageChange 测试提供确定翻译。 */
class ZzTitleBarTranslator final : public QTranslator
{
public:
    /** @brief 为所有标题栏静态命令添加测试前缀。 */
    [[nodiscard]] QString translate(
        const char *context,
        const char *sourceText,
        const char *disambiguation = nullptr,
        int plural = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(plural)
        if (sourceText == nullptr) {
            return {};
        }
        if (std::strcmp(sourceText, "最小化") == 0
            || std::strcmp(sourceText, "最大化") == 0
            || std::strcmp(sourceText, "还原") == 0
            || std::strcmp(sourceText, "关闭") == 0) {
            return QStringLiteral("T:") + QString::fromUtf8(sourceText);
        }
        return {};
    }
};

/** @brief 验证标题栏只发窗口意图并维护 chrome 可访问状态。 */
class ZzFluentTitleBarTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emitsSystemButtonIntentOnly()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setTitle(QStringLiteral("Workspace"));
        titleBar.show();
        QCoreApplication::processEvents();
        auto *minimize = qobject_cast<QToolButton *>(
            titleBar.minimizeButton());
        auto *maximize = qobject_cast<QToolButton *>(
            titleBar.maximizeButton());
        auto *close = qobject_cast<QToolButton *>(titleBar.closeButton());
        QVERIFY(minimize != nullptr);
        QVERIFY(maximize != nullptr);
        QVERIFY(close != nullptr);
        QSignalSpy minimizeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::minimizeRequested);
        QSignalSpy maximizeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::maximizeRestoreRequested);
        QSignalSpy closeSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::closeRequested);

        minimize->setFocus();
        QTest::keyClick(minimize, Qt::Key_Space);
        maximize->setFocus();
        QTest::keyClick(maximize, Qt::Key_Space);
        close->setFocus();
        QTest::keyClick(close, Qt::Key_Space);

        QCOMPARE(minimizeSpy.count(), 1);
        QCOMPARE(maximizeSpy.count(), 1);
        QCOMPARE(closeSpy.count(), 1);
        QVERIFY(titleBar.isVisible());
    }

    void exposesOnlyDescendantChromeWidgets()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        const QList<QWidget *> chrome{
            titleBar.windowIconWidget(),
            titleBar.minimizeButton(),
            titleBar.maximizeButton(),
            titleBar.closeButton()};
        for (QWidget *widget : chrome) {
            QVERIFY(widget != nullptr);
            QVERIFY(titleBar.isAncestorOf(widget));
        }
        const QList<QWidget *> interactive = titleBar.interactiveWidgets();
        QCOMPARE(interactive.size(), 3);
        QVERIFY(interactive.contains(titleBar.minimizeButton()));
        QVERIFY(interactive.contains(titleBar.maximizeButton()));
        QVERIFY(interactive.contains(titleBar.closeButton()));

        titleBar.setSystemButtonsVisible(false);
        QVERIFY(titleBar.minimizeButton()->isHidden());
        QVERIFY(titleBar.maximizeButton()->isHidden());
        QVERIFY(titleBar.closeButton()->isHidden());
    }

    void updatesTitleIconAndMaximizedAccessibility()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        QSignalSpy titleSpy(
            &titleBar,
            &ZzFluentUI::ZzFluentTitleBar::titleChanged);
        titleBar.setTitle(QStringLiteral("Editor"));
        titleBar.setTitle(QStringLiteral("Editor"));
        QPixmap pixmap(QSize(16, 16));
        pixmap.fill(Qt::green);
        titleBar.setWindowIcon(QIcon(pixmap));

        QCOMPARE(titleBar.title(), QStringLiteral("Editor"));
        QCOMPARE(titleSpy.count(), 1);
        QCOMPARE(
            titleBar.maximizeButton()->accessibleName(),
            QStringLiteral("最大化"));
        titleBar.setMaximized(true);
        QCOMPARE(
            titleBar.maximizeButton()->accessibleName(),
            QStringLiteral("还原"));
    }

    void refreshesTranslatedChromeText()
    {
        ZzFluentUI::ZzFluentTitleBar titleBar;
        titleBar.setMaximized(true);
        ZzTitleBarTranslator translator;
        QCoreApplication::installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&titleBar, &languageChange);

        QCOMPARE(
            titleBar.minimizeButton()->accessibleName(),
            QStringLiteral("T:最小化"));
        QCOMPARE(
            titleBar.maximizeButton()->accessibleName(),
            QStringLiteral("T:还原"));
        QCOMPARE(
            titleBar.closeButton()->accessibleName(),
            QStringLiteral("T:关闭"));
        QCOMPARE(
            titleBar.closeButton()->toolTip(),
            QStringLiteral("T:关闭"));

        QCoreApplication::removeTranslator(&translator);
    }
};

QTEST_MAIN(ZzFluentTitleBarTest)

#include "ZzFluentTitleBarTest.moc"
