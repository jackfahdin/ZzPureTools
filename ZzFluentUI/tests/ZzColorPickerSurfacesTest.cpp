#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtTest/QSignalSpy>
#include <QtCore/QPointer>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorPickerButton.h>
#include <ZzFluentUI/ZzColorPickerDialog.h>

using namespace ZzFluentUI;

class ZzColorPickerSurfacesTest final : public QObject
{
    Q_OBJECT
    // A missing stable picker, popup event handling, or dialog transaction must
    // break these tests; all edits use real widgets and focus transitions.
    static QLineEdit *editHex(ZzColorPicker *picker, const QString &text)
    {
        picker->window()->activateWindow();
        if (!QTest::qWaitForWindowActive(picker->window())) return nullptr;
        auto *tabs = picker->findChild<QTabBar *>();
        if (tabs) tabs->setCurrentIndex(2);
        auto *editor = picker->findChild<QLineEdit *>(QStringLiteral("zzHexColorEditor"));
        if (!editor) return nullptr;
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(editor, text);
        return editor;
    }
private Q_SLOTS:
    void buttonLiveValueAndStablePopup()
    {
        QWidget window;
        window.resize(500, 500);
        ZzColorPickerButton button(&window);
        button.move(20, 20);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *picker = button.colorPicker();
        QVERIFY(picker);
        QCOMPARE(picker->appearance(), ZzColorPicker::Fluent);
        QVERIFY(picker->isAlphaEnabled());
        QSignalSpy changed(&button, &ZzColorPickerButton::selectedColorChanged);
        button.setSelectedColor(Qt::red);
        button.setSelectedColor(Qt::red);
        QCOMPARE(changed.size(), 1);
        button.setSelectedColor(QColor());
        QCOMPARE(changed.size(), 1);
        for (int i = 0; i < 3; ++i) {
            QTest::mouseClick(&button, Qt::LeftButton);
            ZZ_VERIFY_EVENTUALLY(picker->isVisible());
            QCOMPARE(QApplication::activePopupWidget(), picker->window());
            picker->setCurrentColor(Qt::green);
            QCOMPARE(button.selectedColor(), QColor(Qt::green));
            QTest::keyClick(picker, Qt::Key_Escape);
            ZZ_VERIFY_EVENTUALLY(!picker->isVisible());
            ZZ_VERIFY_EVENTUALLY(button.hasFocus());
            QCOMPARE(button.colorPicker(), picker);
        }
        QCOMPARE(changed.size(), 2);
    }
    void outsideClickAndSameAnchorClose()
    {
        QWidget window;
        window.resize(600, 600);
        ZzColorPickerButton button(&window);
        button.move(20, 20);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(button.colorPicker());
        QTest::mouseClick(&button, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(button.colorPicker()->isVisible());
        QTest::mouseClick(&button, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(!button.colorPicker()->isVisible());
        QTest::mouseClick(&button, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(button.colorPicker()->isVisible());
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(590, 590));
        ZZ_VERIFY_EVENTUALLY(!button.colorPicker()->isVisible());
        ZZ_VERIFY_EVENTUALLY(button.hasFocus());
    }
    void popupFitsScreenAndRtl()
    {
        QWidget window;
        window.resize(100, 100);
        const QRect available = QApplication::primaryScreen()->availableGeometry();
        window.move(available.bottomRight() - QPoint(100, 100));
        ZzColorPickerButton button(&window);
        button.setLayoutDirection(Qt::RightToLeft);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(button.colorPicker());
        QTest::mouseClick(&button, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(button.colorPicker()->isVisible());
        auto *popup = button.colorPicker()->window();
        QCOMPARE(button.colorPicker()->layoutDirection(), Qt::RightToLeft);
        QVERIFY(available.contains(popup->frameGeometry()));
        auto *scroll = popup->findChild<QScrollArea *>();
        QVERIFY(scroll);
        auto *picker = button.colorPicker();
        // Reaching both opposite content corners proves constrained geometry
        // scrolls rather than silently clipping controls outside the viewport.
        scroll->ensureVisible(picker->width() - 1, picker->height() - 1, 0, 0);
        ZZ_VERIFY_EVENTUALLY(scroll->viewport()->rect().contains(
                picker->mapTo(scroll->viewport(), picker->rect().bottomRight())));
        scroll->ensureVisible(0, 0, 0, 0);
        ZZ_VERIFY_EVENTUALLY(scroll->viewport()->rect().contains(
                picker->mapTo(scroll->viewport(), QPoint())));
        const bool horizontalOverflow = picker->width() > scroll->viewport()->width();
        const bool verticalOverflow = picker->height() > scroll->viewport()->height();
        QCOMPARE(scroll->horizontalScrollBar()->maximum() > 0, horizontalOverflow);
        QCOMPARE(scroll->verticalScrollBar()->maximum() > 0, verticalOverflow);
        if (horizontalOverflow || verticalOverflow) {
            const QPoint anchorCenter = button.mapToGlobal(button.rect().center());
            if (popup->frameGeometry().contains(anchorCenter)) {
                QTest::mouseClick(scroll->viewport(), Qt::LeftButton, Qt::NoModifier,
                                 scroll->viewport()->mapFromGlobal(anchorCenter));
                QVERIFY(popup->isVisible());
            }
            auto *tabs = picker->findChild<QTabBar *>();
            QVERIFY(tabs);
            tabs->setCurrentIndex(2);
            auto *editor = picker->findChild<QLineEdit *>(QStringLiteral("zzHexColorEditor"));
            QVERIFY(editor);
            scroll->ensureWidgetVisible(editor);
            ZZ_VERIFY_EVENTUALLY(scroll->viewport()->rect().contains(
                    editor->mapTo(scroll->viewport(), editor->rect().center())));
            QTest::mouseClick(editor, Qt::LeftButton);
            ZZ_VERIFY_EVENTUALLY(editor->hasFocus());
            QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(editor, QStringLiteral("#80402010"));
            picker->commitPendingEdits();
            QCOMPARE(button.selectedColor(), QColor("#80402010"));
            QVERIFY(available.contains(popup->frameGeometry()));
        }
    }
    void nativePopupAnchorReleaseDoesNotConsumeNextClick()
    {
        QWidget window;
        window.resize(500, 500);
        ZzColorPickerButton button(&window);
        button.move(20, 20);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::mouseClick(&button, Qt::LeftButton);
        auto *popup = button.colorPicker()->window();
        QVERIFY(popup->isVisible());
        // Native popup grabs can deliver an outside anchor press/release to
        // the popup itself, so the anchor never sees the matching release.
        const QPoint outside = popup->mapFromGlobal(button.mapToGlobal(button.rect().center()));
        QTest::mousePress(popup, Qt::LeftButton, Qt::NoModifier, outside);
        QVERIFY(!popup->isVisible());
        QTest::mouseRelease(popup, Qt::LeftButton, Qt::NoModifier, outside);
        QTest::mouseClick(&button, Qt::LeftButton);
        QVERIFY(popup->isVisible());
    }
    void dialogRejectRestoresOpeningValue()
    {
        ZzColorPickerDialog dialog;
        dialog.setCurrentColor(QColor("#0078d4"));
        QVERIFY(dialog.colorPicker());
        QSignalSpy selected(&dialog, &ZzColorPickerDialog::colorSelected);
        for (int i = 0; i < 3; ++i) {
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            dialog.colorPicker()->setCurrentColor(Qt::red);
            dialog.reject();
            QCOMPARE(dialog.currentColor(), QColor("#0078d4"));
            QCOMPARE(selected.size(), 0);
        }
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.colorPicker()->setCurrentColor(Qt::green);
        QTest::keyClick(dialog.colorPicker(), Qt::Key_Escape);
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.currentColor(), QColor("#0078d4"));
    }
    void dialogAcceptPendingHexPreviewBeforeSelection()
    {
        ZzColorPickerDialog dialog;
        QVERIFY(dialog.colorPicker());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *editor = editHex(dialog.colorPicker(), QStringLiteral("#80402010"));
        QVERIFY(editor);
        ZZ_VERIFY_EVENTUALLY(editor->hasFocus());
        QStringList order;
        connect(&dialog, &ZzColorPickerDialog::currentColorChanged, &dialog,
                [&](const QColor &color) { QCOMPARE(color, QColor("#80402010")); order << "preview"; });
        connect(&dialog, &ZzColorPickerDialog::colorSelected, &dialog,
                [&](const QColor &color) { QCOMPARE(color, QColor("#80402010")); order << "selected"; });
        dialog.accept();
        QCOMPARE(dialog.currentColor(), QColor("#80402010"));
        QCOMPARE(order, QStringList({"preview", "selected"}));
        QCoreApplication::processEvents();
        QCOMPARE(order.size(), 2);
    }
    void dialogRejectPendingHexHasNoLatePreview()
    {
        ZzColorPickerDialog dialog;
        QVERIFY(dialog.colorPicker());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(editHex(dialog.colorPicker(), QStringLiteral("#80402010")));
        QSignalSpy changed(&dialog, &ZzColorPickerDialog::currentColorChanged);
        dialog.reject();
        QCOMPARE(dialog.currentColor(), QColor("#0078d4"));
        QCoreApplication::processEvents();
        QCOMPARE(changed.size(), 0);
    }
    void callbacksCanDeleteAndReenter()
    {
        auto *button = new ZzColorPickerButton;
        QPointer<ZzColorPickerButton> buttonGuard(button);
        QVERIFY(button->colorPicker());
        connect(button, &ZzColorPickerButton::selectedColorChanged, button, [button] { delete button; });
        button->setSelectedColor(Qt::red);
        QVERIFY(!buttonGuard);
        auto *dialog = new ZzColorPickerDialog;
        QPointer<ZzColorPickerDialog> guard(dialog);
        QVERIFY(dialog->colorPicker());
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog));
        QVERIFY(editHex(dialog->colorPicker(), QStringLiteral("#80402010")));
        connect(dialog, &ZzColorPickerDialog::currentColorChanged, dialog, [dialog] { delete dialog; });
        dialog->accept();
        QVERIFY(!guard);
        ZzColorPickerDialog reentrant;
        QSignalSpy selected(&reentrant, &ZzColorPickerDialog::colorSelected);
        connect(&reentrant, &ZzColorPickerDialog::colorSelected, &reentrant, [&] { reentrant.reject(); reentrant.accept(); });
        reentrant.show();
        reentrant.accept();
        QCOMPARE(selected.size(), 1);
        QCOMPARE(reentrant.result(), int(QDialog::Accepted));
    }
    void buttonPendingHexCloseCanDeleteOwner()
    {
        auto *window = new QWidget;
        window->resize(500, 500);
        auto *button = new ZzColorPickerButton(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::mouseClick(button, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(button->colorPicker()->isVisible());
        QVERIFY(editHex(button->colorPicker(), QStringLiteral("#80402010")));
        QPointer<QWidget> guard(window);
        connect(button, &ZzColorPickerButton::selectedColorChanged, window,
                [window](const QColor &color) { QCOMPARE(color, QColor("#80402010")); delete window; });
        QTest::keyClick(button->colorPicker()->focusWidget(), Qt::Key_Escape);
        ZZ_VERIFY_EVENTUALLY(!guard);
    }
    void dialogKeyboardAndMouseFinish()
    {
        ZzColorPickerDialog dialog;
        QSignalSpy selected(&dialog, &ZzColorPickerDialog::colorSelected);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *editor = editHex(dialog.colorPicker(), QStringLiteral("#ff123456"));
        QVERIFY(editor);
        QTest::keyClick(editor, Qt::Key_Return);
        ZZ_VERIFY_EVENTUALLY(!dialog.isVisible());
        QCOMPARE(selected.size(), 1);
        QCOMPARE(dialog.currentColor(), QColor("#123456"));
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(editHex(dialog.colorPicker(), QStringLiteral("#80abcdef")));
        QPushButton *cancel = nullptr;
        for (auto *button : dialog.findChildren<QPushButton *>()) {
            if (!button->isDefault()) cancel = button;
        }
        QVERIFY(cancel);
        QTest::mouseClick(cancel, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(!dialog.isVisible());
        QCoreApplication::processEvents();
        QCOMPARE(dialog.currentColor(), QColor("#123456"));
        QCOMPARE(selected.size(), 1);
    }
    void dialogRollbackAndSelectionCallbacksCanDelete()
    {
        auto *dialog = new ZzColorPickerDialog;
        QPointer<ZzColorPickerDialog> guard(dialog);
        dialog->show();
        dialog->setCurrentColor(Qt::red);
        connect(dialog, &ZzColorPickerDialog::currentColorChanged, dialog, [dialog] { delete dialog; });
        dialog->reject();
        QVERIFY(!guard);
        dialog = new ZzColorPickerDialog;
        guard = dialog;
        dialog->show();
        connect(dialog, &ZzColorPickerDialog::colorSelected, dialog, [dialog] { delete dialog; });
        dialog->accept();
        QVERIFY(!guard);
    }
    void invalidHexDoesNotReplaceValue()
    {
        ZzColorPickerDialog dialog;
        dialog.setCurrentColor(QColor("#80402010"));
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(editHex(dialog.colorPicker(), QStringLiteral("#123")));
        QSignalSpy selected(&dialog, &ZzColorPickerDialog::colorSelected);
        dialog.accept();
        QCOMPARE(dialog.currentColor(), QColor("#80402010"));
        QCOMPARE(selected.size(), 1);
        QCOMPARE(selected.at(0).at(0).value<QColor>(), QColor("#80402010"));
    }
    void parentOwnsSurfaces()
    {
        auto *parent = new QWidget;
        auto *button = new ZzColorPickerButton(parent);
        auto *dialog = new ZzColorPickerDialog(parent);
        QVERIFY(button->colorPicker());
        QVERIFY(dialog->colorPicker());
        parent->show();
        QVERIFY(QTest::qWaitForWindowExposed(parent));
        QTest::mouseClick(button, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(button->colorPicker()->isVisible());
        QPointer<ZzColorPicker> buttonPicker(button->colorPicker());
        QPointer<ZzColorPicker> dialogPicker(dialog->colorPicker());
        delete parent;
        QVERIFY(!buttonPicker);
        QVERIFY(!dialogPicker);
    }
};
QTEST_MAIN(ZzColorPickerSurfacesTest)
#include "ZzColorPickerSurfacesTest.moc"
