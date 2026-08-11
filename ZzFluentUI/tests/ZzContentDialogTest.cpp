#include <cstring>
#include <memory>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtGui/QAccessible>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzContentDialog.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

/** @brief 为内容对话框默认按钮提供确定的 LanguageChange 翻译。 */
class ZzContentDialogTranslator final : public QTranslator
{
public:
    /** @brief 翻译三个默认按钮文本，其余文本保持未处理。 */
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
        if (std::strcmp(sourceText, "确定") == 0) {
            return QStringLiteral("Translated primary");
        }
        if (std::strcmp(sourceText, "取消") == 0) {
            return QStringLiteral("Translated secondary");
        }
        if (std::strcmp(sourceText, "关闭") == 0) {
            return QStringLiteral("Translated close");
        }
        return {};
    }
};

/** @brief 验证内容对话框属性、所有权、结果、焦点和遮罩契约。 */
class ZzContentDialogTest final : public QObject
{
    Q_OBJECT

private:
    /** @brief 按 objectName 返回对话框的标准按钮。 */
    static ZzFluentUI::ZzPushButton *button(
        ZzFluentUI::ZzContentDialog *dialog,
        const char *name)
    {
        return dialog->findChild<ZzFluentUI::ZzPushButton *>(
            QString::fromLatin1(name));
    }

private Q_SLOTS:
    void emitsOnlyForEffectivePropertyChanges()
    {
        ZzFluentUI::ZzContentDialog dialog;
        QSignalSpy titleSpy(
            &dialog, &ZzFluentUI::ZzContentDialog::titleChanged);
        QSignalSpy textSpy(
            &dialog, &ZzFluentUI::ZzContentDialog::textChanged);
        QSignalSpy primaryTextSpy(
            &dialog,
            &ZzFluentUI::ZzContentDialog::primaryButtonTextChanged);
        QSignalSpy secondaryVisibleSpy(
            &dialog,
            &ZzFluentUI::ZzContentDialog::secondaryButtonVisibleChanged);
        QSignalSpy closeEnabledSpy(
            &dialog,
            &ZzFluentUI::ZzContentDialog::closeButtonEnabledChanged);
        QSignalSpy defaultSpy(
            &dialog,
            &ZzFluentUI::ZzContentDialog::defaultButtonChanged);

        dialog.setTitle(QStringLiteral("Deploy build"));
        dialog.setTitle(QStringLiteral("Deploy build"));
        dialog.setText(QStringLiteral("Publish the selected artifact?"));
        dialog.setText(QStringLiteral("Publish the selected artifact?"));
        dialog.setPrimaryButtonText(QStringLiteral("Publish"));
        dialog.setPrimaryButtonText(QStringLiteral("Publish"));
        dialog.setSecondaryButtonVisible(true);
        dialog.setSecondaryButtonVisible(true);
        dialog.setCloseButtonEnabled(false);
        dialog.setCloseButtonEnabled(false);
        dialog.setDefaultButton(
            ZzFluentUI::ZzContentDialogButton::Primary);
        dialog.setDefaultButton(
            ZzFluentUI::ZzContentDialogButton::Primary);

        QCOMPARE(titleSpy.count(), 1);
        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(primaryTextSpy.count(), 1);
        QCOMPARE(secondaryVisibleSpy.count(), 1);
        QCOMPARE(closeEnabledSpy.count(), 1);
        QCOMPARE(defaultSpy.count(), 1);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Deploy build"));
        QCOMPARE(
            dialog.accessibleDescription(),
            QStringLiteral("Publish the selected artifact?"));
    }

    void transfersContentOwnershipExplicitly()
    {
        ZzFluentUI::ZzContentDialog dialog;
        QSignalSpy contentSpy(
            &dialog,
            &ZzFluentUI::ZzContentDialog::contentWidgetChanged);
        auto *first = new QLabel(QStringLiteral("First"));
        QPointer<QWidget> firstGuard(first);

        dialog.setContentWidget(first);
        dialog.setContentWidget(first);
        QCOMPARE(contentSpy.count(), 1);
        QCOMPARE(dialog.contentWidget(), first);
        QVERIFY(first->parentWidget() != nullptr);

        auto secondOwner = std::make_unique<QLineEdit>();
        QLineEdit *const second = secondOwner.get();
        dialog.setContentWidget(secondOwner.release());
        QVERIFY(firstGuard.isNull());
        QCOMPARE(contentSpy.count(), 2);
        QCOMPARE(dialog.contentWidget(), second);

        std::unique_ptr<QWidget> taken(dialog.takeContentWidget());
        QCOMPARE(taken.get(), second);
        QCOMPARE(taken->parentWidget(), nullptr);
        QCOMPARE(dialog.contentWidget(), nullptr);
        QCOMPARE(contentSpy.count(), 3);
        QCOMPARE(dialog.takeContentWidget(), nullptr);
    }

    void destroysOwnedContentWithoutLateCallbacks()
    {
        QPointer<QWidget> contentGuard;
        {
            ZzFluentUI::ZzContentDialog dialog;
            auto *content = new QLabel(QStringLiteral("Owned"));
            contentGuard = content;
            dialog.setContentWidget(content);
            QCOMPARE(dialog.contentWidget(), content);
        }
        QVERIFY(contentGuard.isNull());
    }

    void mapsButtonsEnterAndEscapeToStableResults()
    {
        ZzFluentUI::ZzContentDialog dialog;
        dialog.setPrimaryButtonVisible(true);
        dialog.setSecondaryButtonVisible(true);
        dialog.setDefaultButton(
            ZzFluentUI::ZzContentDialogButton::Secondary);
        QSignalSpy finishedSpy(&dialog, &QDialog::finished);

        dialog.open();
        QCoreApplication::processEvents();
        QTest::keyClick(&dialog, Qt::Key_Return);
        QTRY_VERIFY(!dialog.isVisible());
        QCOMPARE(
            dialog.dialogResult(),
            ZzFluentUI::ZzContentDialogResult::Secondary);
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.at(0).at(0).toInt(), 2);

        dialog.setSecondaryButtonEnabled(false);
        dialog.show();
        QCoreApplication::processEvents();
        QCOMPARE(
            dialog.dialogResult(),
            ZzFluentUI::ZzContentDialogResult::None);
        QTest::keyClick(&dialog, Qt::Key_Return);
        QVERIFY(dialog.isVisible());
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QTRY_VERIFY(!dialog.isVisible());
        QCOMPARE(
            dialog.dialogResult(),
            ZzFluentUI::ZzContentDialogResult::Close);

        dialog.setDefaultButton(ZzFluentUI::ZzContentDialogButton::None);
        dialog.show();
        QCoreApplication::processEvents();
        QTest::keyClick(&dialog, Qt::Key_Enter);
        QVERIFY(dialog.isVisible());
        ZzFluentUI::ZzPushButton *primary = button(
            &dialog, "zzContentDialogPrimaryButton");
        QVERIFY(primary != nullptr);
        QTest::mouseClick(primary, Qt::LeftButton);
        QTRY_VERIFY(!dialog.isVisible());
        QCOMPARE(
            dialog.dialogResult(),
            ZzFluentUI::ZzContentDialogResult::Primary);

        QTimer::singleShot(0, &dialog, [primary] {
            primary->click();
        });
        QCOMPARE(dialog.exec(), 1);
        QCOMPARE(
            dialog.dialogResult(),
            ZzFluentUI::ZzContentDialogResult::Primary);
    }

    void limitsModalOverlayToOwningWindowAndRestoresFocus()
    {
        QWidget firstWindow;
        QWidget secondWindow;
        auto *firstLayout = new QVBoxLayout(&firstWindow);
        auto *firstFocus = new QPushButton(QStringLiteral("First"), &firstWindow);
        firstLayout->addWidget(firstFocus);
        auto *secondLayout = new QVBoxLayout(&secondWindow);
        secondLayout->addWidget(
            new QPushButton(QStringLiteral("Second"), &secondWindow));
        firstWindow.resize(520, 360);
        secondWindow.resize(420, 300);
        firstWindow.show();
        secondWindow.show();
        firstWindow.activateWindow();
        firstFocus->setFocus(Qt::OtherFocusReason);
        QCoreApplication::processEvents();
        QVERIFY(firstFocus->hasFocus());

        ZzFluentUI::ZzContentDialog dialog(&firstWindow);
        dialog.setWindowModality(Qt::WindowModal);
        dialog.setTitle(QStringLiteral("Modal"));
        dialog.show();
        QCoreApplication::processEvents();
        QWidget *overlay = firstWindow.findChild<QWidget *>(
            QStringLiteral("zzContentDialogOverlay"),
            Qt::FindDirectChildrenOnly);
        QVERIFY(overlay != nullptr);
        if (overlay == nullptr) {
            return;
        }
        QVERIFY(overlay->isVisible());
        QCOMPARE(overlay->geometry(), firstWindow.rect());
        QCOMPARE(
            secondWindow.findChild<QWidget *>(
                QStringLiteral("zzContentDialogOverlay"),
                Qt::FindDirectChildrenOnly),
            nullptr);

        firstWindow.resize(600, 400);
        QCoreApplication::processEvents();
        QCOMPARE(overlay->geometry(), firstWindow.rect());
        dialog.reject();
        QCoreApplication::processEvents();
        QCOMPARE(
            firstWindow.findChild<QWidget *>(
                QStringLiteral("zzContentDialogOverlay"),
                Qt::FindDirectChildrenOnly),
            nullptr);
        QVERIFY(firstFocus->hasFocus());
    }

    void avoidsOverlayForNonModalDisplayAndFollowsParentLifetime()
    {
        auto *window = new QWidget;
        window->show();
        auto *dialog = new ZzFluentUI::ZzContentDialog(window);
        QPointer<ZzFluentUI::ZzContentDialog> dialogGuard(dialog);
        dialog->show();
        QCoreApplication::processEvents();
        QCOMPARE(
            window->findChild<QWidget *>(
                QStringLiteral("zzContentDialogOverlay"),
                Qt::FindDirectChildrenOnly),
            nullptr);
        delete window;
        QVERIFY(dialogGuard.isNull());
    }

    void appliesThemeAccessibilityLanguageAndRtl()
    {
        ZzFluentUI::ZzThemeController controller;
        std::unique_ptr<QStyle> fusion(
            QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(fusion != nullptr);
        ZzFluentUI::ZzFluentStyle style(&controller, fusion.release());
        ZzFluentUI::ZzContentDialog dialog;
        dialog.setStyle(&style);
        dialog.setTitle(QStringLiteral("Review"));
        dialog.setText(QStringLiteral("Review generated files."));
        dialog.setPrimaryButtonVisible(true);
        dialog.setSecondaryButtonVisible(true);
        dialog.setLayoutDirection(Qt::RightToLeft);

        auto *title = dialog.findChild<QLabel *>(
            QStringLiteral("zzContentDialogTitle"));
        auto *body = dialog.findChild<QLabel *>(
            QStringLiteral("zzContentDialogText"));
        auto *primary = button(&dialog, "zzContentDialogPrimaryButton");
        auto *close = button(&dialog, "zzContentDialogCloseButton");
        QVERIFY(title != nullptr);
        QVERIFY(body != nullptr);
        QVERIFY(primary != nullptr);
        QVERIFY(close != nullptr);
        QCOMPARE(
            title->font(),
            style.themeSnapshot()->font(
                ZzFluentUI::ZzTypographyToken::Title));
        QCOMPARE(
            body->font(),
            style.themeSnapshot()->font(
                ZzFluentUI::ZzTypographyToken::Body));
        QCOMPARE(primary->font(), body->font());
        QCOMPARE(dialog.layoutDirection(), Qt::RightToLeft);

        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&dialog);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::Dialog);
        QCOMPARE(
            interface->text(QAccessible::Name),
            QStringLiteral("Review"));

        dialog.setPrimaryButtonText(QStringLiteral("Keep custom"));
        ZzContentDialogTranslator translator;
        QCoreApplication::installTranslator(&translator);
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(&dialog, &languageChange);
        QCOMPARE(dialog.primaryButtonText(), QStringLiteral("Keep custom"));
        QCOMPARE(
            dialog.secondaryButtonText(),
            QStringLiteral("Translated secondary"));
        QCOMPARE(
            dialog.closeButtonText(),
            QStringLiteral("Translated close"));
        QCoreApplication::removeTranslator(&translator);

        QCOMPARE(dialog.findChildren<QTimer *>().size(), 0);
        QCOMPARE(dialog.findChildren<QAbstractAnimation *>().size(), 0);
    }
};

QTEST_MAIN(ZzContentDialogTest)

#include "ZzContentDialogTest.moc"
