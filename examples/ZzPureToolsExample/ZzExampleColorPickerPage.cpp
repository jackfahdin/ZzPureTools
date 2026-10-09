#include "ZzExampleColorPickerPage.h"
#include "ZzExampleCustomWidgetHelpers.h"
#include "ZzExampleShowcasePagePrivate.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabBar>
#include <ZzFluentUI/ZzColorPickerButton.h>
#include <ZzFluentUI/ZzColorPickerDialog.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzTabBar.h>

namespace ZzExample {
namespace {
using ZzFluentUI::ZzColorPicker;
QString text(const char *value) { return QCoreApplication::translate("ZzPureToolsExample", value); }

QList<QColor> referencePalette()
{
    const auto values = QStringLiteral(
        "FFB900 D13438 E3008C 8E8CD8 0099BC 00CC6A 567C73 69797E "
        "FF8C00 FF4343 BF0077 6B69D6 2D7D9A 10893E 486860 4A5459 "
        "F7630C E74856 C239B3 8764B8 00B7C3 7A7574 498205 647C64 "
        "CA5010 E81123 9A0089 744DA9 038387 5D5A58 107C10 525E54 "
        "DA3B01 EA005E 0078D4 B146C2 00B294 68768A 767676 847545 "
        "EF6950 C30052 0063B1 881798 018574 515C6B 4C4A48 7E735F").split(u' ');
    QList<QColor> colors;
    for (const auto &value : values) colors.append(QColor(u'#' + value));
    return colors;
}

QVBoxLayout *cardLayout(const char *title, QWidget *parent)
{
    auto *card = new ZzExampleCustomCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto *heading = new QLabel(text(title), card);
    auto font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    return layout;
}

QFormLayout *formPage(ZzFluentUI::ZzTabWidget *tabs, const char *title)
{
    auto *page = new QWidget(tabs);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(12, 12, 12, 12);
    form->setVerticalSpacing(10);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    tabs->addTab(page, text(title));
    return form;
}

QCheckBox *check(QFormLayout *form, const char *name, const char *label,
    ZzColorPicker *picker, void (ZzColorPicker::*setter)(bool))
{
    auto *box = new QCheckBox(text(label), form->parentWidget());
    box->setObjectName(QString::fromLatin1(name));
    box->setChecked(true);
    form->addRow(box);
    QObject::connect(box, &QCheckBox::toggled, picker, setter);
    return box;
}

void buildEditor(QVBoxLayout *layout, ZzColorPicker *picker, QWidget *owner)
{
    auto *tabs = new ZzFluentUI::ZzTabWidget(owner);
    tabs->fluentTabBar()->setAppearance(ZzFluentUI::ZzTabBarAppearance::PivotSlide);
    tabs->fluentTabBar()->setExpanding(false);
    tabs->fluentTabBar()->newTabButton()->hide();
    tabs->fluentTabBar()->setTearOffEnabled(false);
    tabs->fluentTabBar()->setTabTransferEnabled(false);
    tabs->setMovable(false);
    tabs->setElideMode(Qt::ElideNone);
    tabs->setObjectName(QStringLiteral("colorPickerEditorTabs"));
    tabs->setMinimumWidth(280);
    auto *color = formPage(tabs, "颜色与色谱");
    auto *hex = new QLineEdit(color->parentWidget());
    hex->setObjectName(QStringLiteral("colorPickerColorInput"));
    hex->setAccessibleName(text("当前颜色（HEX）"));
    color->addRow(text("当前颜色（HEX）"), hex);
    QObject::connect(hex, &QLineEdit::editingFinished, picker, [picker, hex] {
        const QColor value(hex->text());
        if (value.isValid()) picker->setCurrentColor(value);
        hex->setText(picker->currentColor().name(QColor::HexArgb).toUpper());
    });
    QObject::connect(picker, &ZzColorPicker::currentColorChanged, hex, [hex](const QColor &value) {
        hex->setText(value.name(QColor::HexArgb).toUpper());
    });
    auto *shape = new QComboBox(color->parentWidget());
    shape->setObjectName(QStringLiteral("colorPickerShape"));
    shape->addItem(text("方形（Box）"));
    shape->addItem(text("圆形（Ring）"));
    color->addRow(text("色谱形状"), shape);
    QObject::connect(shape, &QComboBox::currentIndexChanged, picker, [picker](int index) {
        picker->setColorSpectrumShape(index == 0 ? ZzColorPicker::Box : ZzColorPicker::Ring);
    });
    auto *representation = new QComboBox(color->parentWidget());
    representation->setObjectName(QStringLiteral("colorPickerRepresentation"));
    representation->addItems({QStringLiteral("RGBA"), QStringLiteral("HSVA")});
    color->addRow(text("颜色表示"), representation);
    QObject::connect(representation, &QComboBox::currentIndexChanged, picker, [picker](int index) {
        picker->setColorRepresentation(index == 0 ? ZzColorPicker::Rgba : ZzColorPicker::Hsva);
    });
    auto *palette = new QComboBox(color->parentWidget());
    palette->setObjectName(QStringLiteral("colorPickerPalette"));
    palette->addItems({text("参考色板（48 色）"), text("自定义色板（6 色）"), text("默认色板（24 色）")});
    color->addRow(text("色板内容"), palette);
    QObject::connect(palette, &QComboBox::currentIndexChanged, picker, [picker](int index) {
        if (index == 2) picker->resetPaletteColors();
        else if (index == 0) picker->setPaletteColors(referencePalette());
        else picker->setPaletteColors({QColor("#944E9B"), QColor("#FFB900"), QColor("#0078D4"),
            QColor("#00CC6A"), QColor("#D13438"), QColor("#800078D4")});
    });
    auto *display = formPage(tabs, "显示项");
    QList<QCheckBox *> defaults;
    defaults << check(display, "colorPickerAlpha", "启用透明度", picker, &ZzColorPicker::setAlphaEnabled)
        << check(display, "colorPickerSpectrumVisible", "显示色谱", picker, &ZzColorPicker::setColorSpectrumVisible)
        << check(display, "colorPickerPaletteVisible", "显示色板", picker, &ZzColorPicker::setColorPaletteVisible)
        << check(display, "colorPickerPreviewVisible", "显示颜色预览", picker, &ZzColorPicker::setColorPreviewVisible)
        << check(display, "colorPickerAlphaVisible", "显示透明度滑条", picker, &ZzColorPicker::setAlphaSliderVisible)
        << check(display, "colorPickerSliderVisible", "显示通道滑条", picker, &ZzColorPicker::setColorSliderVisible)
        << check(display, "colorPickerInputsVisible", "显示通道输入框", picker, &ZzColorPicker::setColorChannelTextInputVisible);
    auto *behavior = formPage(tabs, "交互");
    auto *disabled = new QCheckBox(text("禁用内嵌选择器"), behavior->parentWidget());
    disabled->setObjectName(QStringLiteral("colorPickerDisabled"));
    behavior->addRow(disabled);
    QObject::connect(disabled, &QCheckBox::toggled, picker, [picker](bool value) { picker->setEnabled(!value); });
    auto *rtl = new QCheckBox(text("从右向左布局"), behavior->parentWidget());
    rtl->setObjectName(QStringLiteral("colorPickerRtl"));
    behavior->addRow(rtl);
    QObject::connect(rtl, &QCheckBox::toggled, picker, [picker](bool value) {
        picker->setLayoutDirection(value ? Qt::RightToLeft : Qt::LeftToRight);
    });
    auto *hint = new QLabel(text("右侧属性实时作用于内嵌选择器。按钮即时更新，对话框仅在确定后提交。"), owner);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addWidget(tabs);
    auto *reset = new QPushButton(text("重置属性"), owner);
    reset->setObjectName(QStringLiteral("colorPickerReset"));
    layout->addWidget(reset, 0, Qt::AlignLeft);
    const auto resetEditor = [=] {
        for (auto *box : defaults) box->setChecked(true);
        disabled->setChecked(false);
        rtl->setChecked(false);
        shape->setCurrentIndex(0);
        representation->setCurrentIndex(0);
        palette->setCurrentIndex(0);
        picker->setPaletteColors(referencePalette());
        picker->setCurrentColor(QColor("#944E9B"));
        hex->setText(QStringLiteral("#FF944E9B"));
        tabs->setCurrentIndex(0);
        picker->findChild<QTabBar *>(QStringLiteral("zzColorPickerTabs"))->setCurrentIndex(0);
    };
    QObject::connect(reset, &QPushButton::clicked, picker, resetEditor);
    resetEditor();
}
} // namespace

ZzExampleColorPickerPage::ZzExampleColorPickerPage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("colorPickerContent"));
    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(12);
    auto *intro = new QLabel(text("通过色谱、色板或通道输入选择颜色，支持透明度与键盘操作。"), this);
    intro->setWordWrap(true);
    main->addWidget(intro);
    auto *inlineCard = cardLayout("内嵌颜色选择器", this);
    auto *row = new QHBoxLayout;
    row->setSpacing(20);
    auto *picker = new ZzColorPicker(inlineCard->parentWidget());
    picker->setObjectName(QStringLiteral("colorPickerInline"));
    picker->setAppearance(ZzColorPicker::Fluent);
    picker->setAlphaEnabled(true);
    picker->setMinimumWidth(340);
    picker->setMaximumWidth(440);
    row->addWidget(picker, 1, Qt::AlignTop);
    auto *editor = new QVBoxLayout;
    buildEditor(editor, picker, inlineCard->parentWidget());
    row->addLayout(editor, 1);
    inlineCard->addLayout(row);
    main->addWidget(inlineCard->parentWidget());

    auto *surfaces = new QHBoxLayout;
    auto *buttonCard = cardLayout("按钮弹层", this);
    auto *button = new ZzFluentUI::ZzColorPickerButton(buttonCard->parentWidget());
    button->setObjectName(QStringLiteral("colorPickerButton"));
    button->setSelectedColor(QColor("#FFB900"));
    button->colorPicker()->setPaletteColors(referencePalette());
    buttonCard->addWidget(button, 0, Qt::AlignLeft);
    auto *buttonValue = new QLabel(QStringLiteral("#FFFFB900"), buttonCard->parentWidget());
    buttonValue->setObjectName(QStringLiteral("colorPickerButtonValue"));
    QObject::connect(button, &ZzFluentUI::ZzColorPickerButton::selectedColorChanged, buttonValue,
        [buttonValue](const QColor &value) { buttonValue->setText(value.name(QColor::HexArgb).toUpper()); });
    buttonCard->addWidget(buttonValue);
    auto *buttonHint = new QLabel(text("点击展开，再次点击或按 Escape 收起。颜色即时生效。"), buttonCard->parentWidget());
    buttonHint->setWordWrap(true);
    buttonCard->addWidget(buttonHint);
    buttonCard->addStretch();
    surfaces->addWidget(buttonCard->parentWidget(), 1);

    auto *dialogCard = cardLayout("颜色对话框", this);
    auto *open = new QPushButton(text("打开颜色对话框"), dialogCard->parentWidget());
    open->setObjectName(QStringLiteral("colorPickerOpenDialog"));
    dialogCard->addWidget(open, 0, Qt::AlignLeft);
    auto *dialog = new ZzFluentUI::ZzColorPickerDialog(this);
    dialog->setObjectName(QStringLiteral("colorPickerDialog"));
    dialog->setCurrentColor(QColor("#0078D4"));
    dialog->colorPicker()->setPaletteColors(referencePalette());
    auto *selected = new QLabel(QStringLiteral("#FF0078D4"), dialogCard->parentWidget());
    selected->setObjectName(QStringLiteral("colorPickerDialogValue"));
    dialogCard->addWidget(selected);
    QObject::connect(open, &QPushButton::clicked, dialog, &QDialog::open);
    QObject::connect(dialog, &ZzFluentUI::ZzColorPickerDialog::colorSelected, selected,
        [selected](const QColor &value) { selected->setText(value.name(QColor::HexArgb).toUpper()); });
    auto *dialogHint = new QLabel(text("确定提交新颜色；取消、Escape 或关闭恢复打开前的颜色。"), dialogCard->parentWidget());
    dialogHint->setWordWrap(true);
    dialogCard->addWidget(dialogHint);
    dialogCard->addStretch();
    surfaces->addWidget(dialogCard->parentWidget(), 1);
    main->addLayout(surfaces);
    auto *reset = findChild<QPushButton *>(QStringLiteral("colorPickerReset"));
    QObject::connect(reset, &QPushButton::clicked, this, [=] {
        button->colorPicker()->window()->hide();
        button->setSelectedColor(QColor("#FFB900"));
        dialog->reject();
        dialog->setCurrentColor(QColor("#0078D4"));
        selected->setText(QStringLiteral("#FF0078D4"));
    });
    auto *apiCard = cardLayout("API 示例", this);
    auto *api = new QPlainTextEdit(apiCard->parentWidget());
    api->setReadOnly(true);
    api->setPlainText(QStringLiteral(
        "picker->setAppearance(ZzColorPicker::Fluent);\n"
        "picker->setAlphaEnabled(true);\n"
        "picker->setCurrentColor(QColor(\"#944E9B\"));\n"
        "picker->setColorSpectrumShape(ZzColorPicker::Ring);\n"
        "picker->setColorRepresentation(ZzColorPicker::Hsva);\n"
        "connect(button, &ZzColorPickerButton::selectedColorChanged, receiver, updateColor);\n"
        "connect(dialog, &ZzColorPickerDialog::colorSelected, receiver, commitColor);"));
    api->setFixedHeight(160);
    apiCard->addWidget(api);
    main->addWidget(apiCard->parentWidget());
}

void ZzExampleShowcasePagePrivate::buildColorPicker(QVBoxLayout *layout, QWidget *parent)
{
    layout->addWidget(new ZzExampleColorPickerPage(parent));
}
} // namespace ZzExample
