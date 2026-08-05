#include <array>
#include <memory>
#include <vector>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QIntValidator>
#include <QtGui/QPainter>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace {

/** @brief 判断图像是否包含指定容差内的目标颜色。 */
bool zzContainsColor(const QImage &image, const QColor &expected)
{
    constexpr int tolerance = 8;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance) {
                return true;
            }
        }
    }
    return false;
}

/** @brief 使用指定状态直接渲染编辑器 frame。 */
QImage zzRenderFrame(
    QWidget *editor,
    ZzFluentUI::ZzFluentStyle *style,
    QStyle::State state,
    const QPalette &palette)
{
    constexpr QSize size(180, 64);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QStyleOptionFrame option;
    option.initFrom(editor);
    option.rect = image.rect();
    option.state = state;
    option.palette = palette;
    QPainter painter(&image);
    style->drawPrimitive(
        QStyle::PE_Frame,
        &option,
        &painter,
        editor);
    painter.end();
    return image;
}

} // namespace

/** @brief 验证标准 Qt 文本编辑器的 Fluent 绘制与原生编辑语义。 */
class ZzTextInputControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesPlatformContentMeasurements()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QStyleOptionFrame option;
        option.state = QStyle::State_Enabled;
        option.fontMetrics = QFontMetrics(QApplication::font());

        const QSize compactContents(8, 8);
        const QSize baseCompact = style.baseStyle()->sizeFromContents(
            QStyle::CT_LineEdit,
            &option,
            compactContents);
        const QSize compact = style.sizeFromContents(
            QStyle::CT_LineEdit,
            &option,
            compactContents);
        QVERIFY(compact.width() >= baseCompact.width());
        QVERIFY(compact.height() >= baseCompact.height());
        QVERIFY(compact.width() >= 96);
        QVERIFY(compact.height() >= 32);

        const QSize largeContents(420, 84);
        const QSize baseLarge = style.baseStyle()->sizeFromContents(
            QStyle::CT_LineEdit,
            &option,
            largeContents);
        const QSize large = style.sizeFromContents(
            QStyle::CT_LineEdit,
            &option,
            largeContents);
        QVERIFY(large.width() >= baseLarge.width());
        QVERIFY(large.height() >= baseLarge.height());
    }

    void drawsEveryStandardEditorFrame()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLineEdit lineEdit;
        QTextEdit textEdit;
        QPlainTextEdit plainTextEdit;
        QTextBrowser textBrowser;
        const std::array<QWidget *, 4> editors{
            &lineEdit,
            &textEdit,
            &plainTextEdit,
            &textBrowser};
        QPalette palette = style.standardPalette();
        const QColor base(Qt::blue);
        const QColor focus(Qt::green);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::Highlight, focus);

        for (QWidget *editor : editors) {
            editor->setStyle(&style);
            const QImage image = zzRenderFrame(
                editor,
                &style,
                QStyle::State_Enabled | QStyle::State_HasFocus,
                palette);
            QVERIFY(zzContainsColor(image, base));
            QVERIFY(zzContainsColor(image, focus));
            QCOMPARE(editor->style(), &style);
        }
    }

    void drawsNormalHoverFocusAndDisabledStates()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLineEdit editor;
        editor.setStyle(&style);
        QPalette palette = style.standardPalette();
        const QColor base(Qt::blue);
        const QColor focus(Qt::green);
        palette.setColor(QPalette::Base, base);
        palette.setColor(QPalette::Highlight, focus);

        const QImage normal = zzRenderFrame(
            &editor,
            &style,
            QStyle::State_Enabled,
            palette);
        QVERIFY(zzContainsColor(normal, base));
        QVERIFY(zzContainsColor(
            normal,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlStroke)));

        const QImage hover = zzRenderFrame(
            &editor,
            &style,
            QStyle::State_Enabled | QStyle::State_MouseOver,
            palette);
        QVERIFY(zzContainsColor(
            hover,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlFillHover)));

        const QImage focused = zzRenderFrame(
            &editor,
            &style,
            QStyle::State_Enabled | QStyle::State_HasFocus,
            palette);
        QVERIFY(zzContainsColor(focused, base));
        QVERIFY(zzContainsColor(focused, focus));

        const QImage disabled = zzRenderFrame(
            &editor,
            &style,
            QStyle::State_None,
            palette);
        QVERIFY(zzContainsColor(
            disabled,
            controller.snapshot()->color(
                ZzFluentUI::ZzColorToken::ControlFillDisabled)));
    }

    void preservesNativeLineEditSemantics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QLineEdit editor;
        editor.setStyle(&style);
        editor.setValidator(new QIntValidator(0, 999, &editor));
        editor.setClearButtonEnabled(true);
        QSignalSpy textSpy(&editor, &QLineEdit::textChanged);
        editor.show();
        editor.setFocus();
        QCoreApplication::processEvents();

        QTest::keyClicks(&editor, QStringLiteral("42"));
        QCOMPARE(editor.text(), QStringLiteral("42"));
        QTest::keyClicks(&editor, QStringLiteral("x"));
        QCOMPARE(editor.text(), QStringLiteral("42"));
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.text(), QStringLiteral("4"));
        editor.undo();
        QCOMPARE(editor.text(), QStringLiteral("42"));
        editor.redo();
        QCOMPARE(editor.text(), QStringLiteral("4"));

        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString(QStringLiteral("7"));
        QVERIFY(QCoreApplication::sendEvent(&editor, &inputMethodEvent));
        QCOMPARE(editor.text(), QStringLiteral("47"));
        QVERIFY(editor.inputMethodQuery(Qt::ImEnabled).toBool());

        editor.setEchoMode(QLineEdit::Password);
        QCOMPARE(editor.text(), QStringLiteral("47"));
        QVERIFY(editor.displayText() != editor.text());
        QVERIFY(!editor.findChildren<QAbstractButton *>().isEmpty());
        std::unique_ptr<QMenu> menu(editor.createStandardContextMenu());
        QVERIFY(menu != nullptr);
        QVERIFY(!menu->actions().isEmpty());
        QVERIFY(textSpy.count() >= 4);

        QLineEdit masked;
        masked.setInputMask(QStringLiteral("000-000;_"));
        masked.setText(QStringLiteral("123456"));
        QCOMPARE(masked.text(), QStringLiteral("123-456"));
    }

    void preservesNativeMultilineSemantics()
    {
        QTextEdit rich;
        rich.setHtml(QStringLiteral("<p><b>Alpha</b> beta</p>"));
        QCOMPARE(rich.toPlainText(), QStringLiteral("Alpha beta"));
        QTextCursor richCursor = rich.textCursor();
        richCursor.movePosition(QTextCursor::End);
        richCursor.insertText(QStringLiteral(" gamma"));
        rich.setTextCursor(richCursor);
        QCOMPARE(rich.toPlainText(), QStringLiteral("Alpha beta gamma"));
        rich.undo();
        QCOMPARE(rich.toPlainText(), QStringLiteral("Alpha beta"));
        rich.redo();
        QCOMPARE(rich.toPlainText(), QStringLiteral("Alpha beta gamma"));

        QPlainTextEdit plain;
        plain.setMaximumBlockCount(2);
        plain.setPlainText(QStringLiteral("one\ntwo\nthree"));
        QCOMPARE(plain.document()->blockCount(), 2);
        QVERIFY(!plain.toPlainText().contains(QLatin1String("one")));
        plain.selectAll();
        QCOMPARE(
            plain.textCursor().selectedText(),
            QStringLiteral("two\u2029three"));

        QInputMethodEvent inputMethodEvent;
        inputMethodEvent.setCommitString(QStringLiteral("four"));
        plain.moveCursor(QTextCursor::End);
        QVERIFY(QCoreApplication::sendEvent(&plain, &inputMethodEvent));
        QVERIFY(plain.toPlainText().endsWith(QLatin1String("four")));
    }

    void preservesAccessibleTextRoles()
    {
        QLineEdit lineEdit(QStringLiteral("Alpha"));
        QTextEdit textEdit(QStringLiteral("Beta"));
        QPlainTextEdit plainTextEdit(QStringLiteral("Gamma"));
        const std::array<QWidget *, 3> editors{
            &lineEdit,
            &textEdit,
            &plainTextEdit};
        const std::array<QString, 3> values{
            QStringLiteral("Alpha"),
            QStringLiteral("Beta"),
            QStringLiteral("Gamma")};

        for (std::size_t index = 0; index < editors.size(); ++index) {
            QAccessibleInterface *interface =
                QAccessible::queryAccessibleInterface(editors[index]);
            if (interface == nullptr) {
                QFAIL("标准文本编辑器缺少无障碍接口");
                return;
            }
            QCOMPARE(interface->role(), QAccessible::EditableText);
            QCOMPARE(
                interface->text(QAccessible::Value),
                values[index]);
        }
        plainTextEdit.setReadOnly(true);
        QAccessibleInterface *plainInterface =
            QAccessible::queryAccessibleInterface(&plainTextEdit);
        if (plainInterface == nullptr) {
            QFAIL("只读纯文本编辑器缺少无障碍接口");
            return;
        }
        QVERIFY(plainInterface->state().readOnly);
        lineEdit.setEnabled(false);
        QAccessibleInterface *lineInterface =
            QAccessible::queryAccessibleInterface(&lineEdit);
        if (lineInterface == nullptr) {
            QFAIL("禁用单行编辑器缺少无障碍接口");
            return;
        }
        QVERIFY(lineInterface->state().disabled);
    }

    void keepsPerInstanceInfrastructureStable()
    {
        constexpr int lineCount = 40;
        constexpr int richCount = 30;
        constexpr int plainCount = 30;
        constexpr int stateChangeRounds = 1000;
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QWidget host;
        host.setStyle(&style);
        std::vector<QLineEdit *> lines;
        std::vector<QTextEdit *> richEditors;
        std::vector<QPlainTextEdit *> plainEditors;
        lines.reserve(lineCount);
        richEditors.reserve(richCount);
        plainEditors.reserve(plainCount);

        for (int index = 0; index < lineCount; ++index) {
            auto *editor = new QLineEdit(&host);
            editor->setStyle(&style);
            editor->setText(QString::number(index));
            lines.push_back(editor);
        }
        for (int index = 0; index < richCount; ++index) {
            auto *editor = new QTextEdit(&host);
            editor->setStyle(&style);
            editor->setPlainText(QString::number(index));
            richEditors.push_back(editor);
        }
        for (int index = 0; index < plainCount; ++index) {
            auto *editor = new QPlainTextEdit(&host);
            editor->setStyle(&style);
            editor->setPlainText(QString::number(index));
            plainEditors.push_back(editor);
        }

        const qsizetype initialObjects =
            host.findChildren<QObject *>().size();
        const qsizetype initialAnimations =
            host.findChildren<QAbstractAnimation *>().size();
        const qsizetype initialTimers =
            host.findChildren<QTimer *>().size();
        for (int round = 0; round < stateChangeRounds; ++round) {
            const bool alternate = round % 2 == 0;
            const Qt::LayoutDirection direction = alternate
                ? Qt::LeftToRight
                : Qt::RightToLeft;
            for (int index = 0; index < lineCount; ++index) {
                QLineEdit *editor = lines[static_cast<std::size_t>(index)];
                editor->setText(QString::number(round + index));
                editor->setPlaceholderText(QString::number(index));
                editor->setReadOnly(alternate);
                editor->setLayoutDirection(direction);
            }
            for (int index = 0; index < richCount; ++index) {
                QTextEdit *editor =
                    richEditors[static_cast<std::size_t>(index)];
                editor->setPlainText(QString::number(round + index));
                editor->setReadOnly(alternate);
                editor->setLayoutDirection(direction);
            }
            for (int index = 0; index < plainCount; ++index) {
                QPlainTextEdit *editor =
                    plainEditors[static_cast<std::size_t>(index)];
                editor->setPlainText(QString::number(round + index));
                editor->setReadOnly(alternate);
                editor->setLayoutDirection(direction);
            }
        }

        QCOMPARE(host.findChildren<QObject *>().size(), initialObjects);
        QCOMPARE(
            host.findChildren<QAbstractAnimation *>().size(),
            initialAnimations);
        QCOMPARE(host.findChildren<QTimer *>().size(), initialTimers);
        for (QLineEdit *editor : lines) {
            QCOMPARE(editor->style(), &style);
        }
        for (QTextEdit *editor : richEditors) {
            QCOMPARE(editor->style(), &style);
        }
        for (QPlainTextEdit *editor : plainEditors) {
            QCOMPARE(editor->style(), &style);
        }
    }
};

QTEST_MAIN(ZzTextInputControlsTest)

#include "ZzTextInputControlsTest.moc"
