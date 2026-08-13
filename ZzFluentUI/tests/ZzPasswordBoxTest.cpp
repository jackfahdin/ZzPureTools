#include <cstring>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTranslator>
#include <QtGui/QAccessible>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzPasswordBox.h>
#include <ZzFluentUI/ZzPasswordRevealMode.h>
#include <ZzFluentUI/ZzThemeController.h>

/** @brief 为 PasswordBox 内部可访问文本提供确定翻译。 */
class ZzPasswordBoxTranslator final : public QTranslator
{
public:
    /** @brief 翻译查看按钮的名称、说明和 tooltip。 */
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
            && std::strcmp(sourceText, "显示密码") == 0) {
            return QStringLiteral("Translated reveal");
        }
        if (sourceText != nullptr
            && std::strcmp(sourceText, "按住时临时显示密码") == 0) {
            return QStringLiteral("Translated hold description");
        }
        if (sourceText != nullptr
            && std::strcmp(sourceText, "按住显示密码") == 0) {
            return QStringLiteral("Translated hold tooltip");
        }
        return {};
    }
};

/** @brief 验证 PasswordBox 的状态、输入、安全终止和对象预算。 */
class ZzPasswordBoxTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 返回 PasswordBox 固定拥有的查看按钮。 */
    static ZzFluentUI::ZzIconButton *revealButton(
        ZzFluentUI::ZzPasswordBox *box)
    {
        auto *button = box->findChild<ZzFluentUI::ZzIconButton *>(
            QStringLiteral("zzPasswordRevealButton"));
        Q_ASSERT(button != nullptr);
        return button;
    }

    /** @brief 创建应用级 Fluent style 供图标和逻辑尺寸测试使用。 */
    static std::unique_ptr<ZzFluentUI::ZzFluentStyle> createStyle(
        ZzFluentUI::ZzThemeController *controller)
    {
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        Q_ASSERT(fusion != nullptr);
        return std::make_unique<ZzFluentUI::ZzFluentStyle>(
            controller,
            fusion.release());
    }

private Q_SLOTS:
    void defaultsAndModesAreIdempotent()
    {
        ZzFluentUI::ZzPasswordBox box;
        QSignalSpy modeSpy(
            &box, &ZzFluentUI::ZzPasswordBox::revealModeChanged);
        QSignalSpy visibleSpy(
            &box,
            &ZzFluentUI::ZzPasswordBox::passwordVisibilityChanged);

        QCOMPARE(
            box.revealMode(),
            ZzFluentUI::ZzPasswordRevealMode::Peek);
        QCOMPARE(box.echoMode(), QLineEdit::Password);
        QVERIFY(!box.isPasswordVisible());
        QVERIFY(!revealButton(&box)->isVisible());

        box.setText(QStringLiteral("secret"));
        box.show();
        QCoreApplication::processEvents();
        QVERIFY(revealButton(&box)->isVisible());

        box.setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Hidden);
        box.setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Hidden);
        QCOMPARE(box.echoMode(), QLineEdit::Password);
        QVERIFY(!revealButton(&box)->isVisible());
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(visibleSpy.count(), 0);

        box.setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Visible);
        box.setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Visible);
        QCOMPARE(box.echoMode(), QLineEdit::Normal);
        QVERIFY(box.isPasswordVisible());
        QVERIFY(!revealButton(&box)->isVisible());
        QCOMPARE(modeSpy.count(), 2);
        QCOMPARE(visibleSpy.count(), 1);

        box.setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Peek);
        QCOMPARE(box.echoMode(), QLineEdit::Password);
        QVERIFY(!box.isPasswordVisible());
        QVERIFY(revealButton(&box)->isVisible());
        QCOMPARE(modeSpy.count(), 3);
        QCOMPARE(visibleSpy.count(), 2);
    }

    void peekUsesPressAndReleaseWithoutChangingText()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = createStyle(&controller);
        ZzFluentUI::ZzPasswordBox box;
        box.setStyle(style.get());
        box.setText(QStringLiteral("correct horse battery staple"));
        box.resize(360, 40);
        box.show();
        QCoreApplication::processEvents();
        auto *button = revealButton(&box);
        QSignalSpy visibleSpy(
            &box,
            &ZzFluentUI::ZzPasswordBox::passwordVisibilityChanged);

        QTest::mousePress(button, Qt::LeftButton);
        QVERIFY(box.isPasswordVisible());
        QCOMPARE(box.echoMode(), QLineEdit::Normal);
        QCOMPARE(
            box.text(),
            QStringLiteral("correct horse battery staple"));
        QCOMPARE(visibleSpy.count(), 1);

        QTest::mouseRelease(button, Qt::LeftButton);
        QVERIFY(!box.isPasswordVisible());
        QCOMPARE(box.echoMode(), QLineEdit::Password);
        QCOMPARE(
            box.text(),
            QStringLiteral("correct horse battery staple"));
        QCOMPARE(visibleSpy.count(), 2);
    }

    void focusAndApplicationChangesEndPeek()
    {
        QWidget host;
        ZzFluentUI::ZzPasswordBox box(&host);
        QWidget other(&host);
        box.setText(QStringLiteral("secret"));
        box.setGeometry(0, 0, 240, 40);
        other.setGeometry(0, 50, 240, 40);
        other.setFocusPolicy(Qt::StrongFocus);
        host.resize(260, 100);
        host.show();
        QCoreApplication::processEvents();

        auto *button = revealButton(&box);
        QTest::mousePress(button, Qt::LeftButton);
        QVERIFY(box.isPasswordVisible());
        other.setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(!box.isPasswordVisible());
        QCOMPARE(box.echoMode(), QLineEdit::Password);

        QTest::mousePress(button, Qt::LeftButton);
        QVERIFY(box.isPasswordVisible());
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(&box, &deactivate);
        QVERIFY(!box.isPasswordVisible());
        QCOMPARE(box.echoMode(), QLineEdit::Password);
    }

    void emptyDisabledAndRtlGeometryStaySafe()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = createStyle(&controller);
        ZzFluentUI::ZzPasswordBox box;
        box.setStyle(style.get());
        box.resize(320, 40);
        box.show();
        QCoreApplication::processEvents();
        auto *button = revealButton(&box);
        QVERIFY(!button->isVisible());

        box.setText(QStringLiteral("a very long password value"));
        QCoreApplication::processEvents();
        QVERIFY(button->isVisible());
        QVERIFY(button->geometry().right() <= box.contentsRect().right());
        QVERIFY(box.textMargins().right() > button->width());

        box.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        QVERIFY(button->geometry().left() >= box.contentsRect().left());
        QVERIFY(box.textMargins().left() > button->width());
        QCOMPARE(box.textMargins().right(), 0);

        box.setEnabled(false);
        QCoreApplication::processEvents();
        QVERIFY(!button->isVisible());
        QVERIFY(!box.isPasswordVisible());
        box.setEnabled(true);
        box.clear();
        QCoreApplication::processEvents();
        QVERIFY(!button->isVisible());
    }

    void languageChangeRefreshesAccessibleText()
    {
        ZzFluentUI::ZzPasswordBox box;
        box.setText(QStringLiteral("secret"));
        auto *button = revealButton(&box);
        QCOMPARE(button->accessibleName(), QStringLiteral("显示密码"));

        ZzPasswordBoxTranslator translator;
        QVERIFY(QCoreApplication::installTranslator(&translator));
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&box, &languageChange);
        QCOMPARE(
            button->accessibleName(),
            QStringLiteral("Translated reveal"));
        QCOMPARE(
            button->accessibleDescription(),
            QStringLiteral("Translated hold description"));
        QCOMPARE(
            button->toolTip(),
            QStringLiteral("Translated hold tooltip"));
        QCoreApplication::removeTranslator(&translator);
    }

    void preservesNativeEditableAccessibilityWithoutPlaintextLeak()
    {
        ZzFluentUI::ZzPasswordBox box;
        box.setAccessibleName(QStringLiteral("Account password"));
        box.setText(QStringLiteral("do-not-expose"));
        box.show();
        QCoreApplication::processEvents();

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&box);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::EditableText);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Account password"));
        QVERIFY(
            interface->text(QAccessible::Value)
            != QStringLiteral("do-not-expose"));

        QAccessibleInterface *buttonInterface =
            QAccessible::queryAccessibleInterface(revealButton(&box));
        QVERIFY(buttonInterface != nullptr);
        QCOMPARE(buttonInterface->role(), QAccessible::Button);
        QVERIFY(!buttonInterface->text(QAccessible::Name).isEmpty());
    }

    void repeatedPeekKeepsFixedObjectBudget()
    {
        ZzFluentUI::ZzPasswordBox box;
        box.setText(QStringLiteral("secret"));
        box.show();
        QCoreApplication::processEvents();
        auto *const button = revealButton(&box);
        const qsizetype objectCount = box.findChildren<QObject *>().size();

        for (int iteration = 0; iteration < 1000; ++iteration) {
            QMetaObject::invokeMethod(button, "pressed");
            QMetaObject::invokeMethod(button, "released");
        }

        QCOMPARE(revealButton(&box), button);
        QCOMPARE(box.findChildren<QObject *>().size(), objectCount);
        QVERIFY(!box.isPasswordVisible());
        QCOMPARE(box.echoMode(), QLineEdit::Password);
    }

    void focusedRevealButtonCanBeDestroyedSafely()
    {
        auto box = std::make_unique<ZzFluentUI::ZzPasswordBox>();
        QPointer<ZzFluentUI::ZzPasswordBox> boxGuard(box.get());
        box->setText(QStringLiteral("secret"));
        box->resize(240, 40);
        box->show();
        QCoreApplication::processEvents();

        auto *const button = revealButton(box.get());
        QPointer<ZzFluentUI::ZzIconButton> buttonGuard(button);
        button->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(button->hasFocus());

        box.reset();
        QVERIFY(boxGuard.isNull());
        QVERIFY(buttonGuard.isNull());
    }
};

QTEST_MAIN(ZzPasswordBoxTest)
#include "ZzPasswordBoxTest.moc"
