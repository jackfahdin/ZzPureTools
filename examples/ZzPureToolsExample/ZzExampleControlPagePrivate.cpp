#include "ZzExampleControlPagePrivate.h"
#include "ZzExampleControlPage.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDate>
#include <QtGui/QFont>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <array>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzCalendarPicker.h>
#include <ZzFluentUI/ZzCalendar.h>
#include <ZzFluentUI/ZzControlAppearance.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzInfoBadge.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzPasswordBox.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzSliderValueTip.h>
#include <ZzFluentUI/ZzScrollArea.h>
#include <ZzFluentUI/ZzScrollBar.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

namespace ZzExample {
namespace {
/** @brief 将示例控件加入所在展示区的流式布局。 */
void zzAdd(QWidget *host, QWidget *control)
{
    host->layout()->addWidget(control);
}
} // namespace

ZzExampleControlPagePrivate::ZzExampleControlPagePrivate(ZzExampleControlPage *page)
    : q_ptr(page)
{
}

void ZzExampleControlPagePrivate::initialize(ZzExampleControlKind kind, const QString &title)
{
    auto *outer = new QVBoxLayout(q_ptr);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new ZzFluentUI::ZzScrollArea(q_ptr);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content = new QWidget(scroll);
    layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);
    auto *heading = new QLabel(title, content);
    heading->setWordWrap(true);
    QFont font = heading->font();
    font.setPointSizeF(font.pointSizeF() + 8.0);
    font.setWeight(QFont::DemiBold);
    heading->setFont(font);
    layout->addWidget(heading);
    auto *hint = new QLabel(QCoreApplication::translate("ZzPureToolsExample",
                                "独立预览当前控件，尝试不同状态与交互；颜色跟随应用主题。"),
        content);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    status = new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "操作控件以查看结果"), content);
    status->setObjectName(QStringLiteral("zzExampleControlStatus"));
    status->setWordWrap(true);

    using Kind = ZzExampleControlKind;
    switch (kind) {
    case Kind::PushButton:
    case Kind::IconButton:
    case Kind::ToolButton:
        buildButtons(kind);
        break;
    case Kind::RadioButton:
    case Kind::CheckBox:
    case Kind::ToggleSwitch:
        buildSelection(kind);
        break;
    case Kind::LineEdit:
    case Kind::PlainTextEdit:
    case Kind::ComboBox:
    case Kind::MultiSelectComboBox:
        buildInput(kind);
        break;
    case Kind::SpinBox:
    case Kind::DoubleSpinBox:
    case Kind::CalendarPicker:
    case Kind::RollerPicker:
        buildValue(kind);
        break;
    case Kind::Slider:
    case Kind::ProgressBar:
    case Kind::ProgressRing:
        buildProgress(kind);
        break;
    case Kind::MessageBar:
    case Kind::InfoBadge:
        buildFeedback(kind);
        break;
    case Kind::ScrollBar:
        buildScroll();
        break;
    }

    auto *enabled
        = new QCheckBox(QCoreApplication::translate("ZzPureToolsExample", "启用演示控件"), content);
    enabled->setChecked(true);
    QObject::connect(enabled, &QCheckBox::toggled, content, [this](bool checked) {
        for (auto *host : content->findChildren<QWidget *>(
                 QStringLiteral("zzExampleControlPreview"), Qt::FindDirectChildrenOnly)) {
            host->setEnabled(checked);
        }
    });
    layout->addSpacing(12);
    layout->addWidget(enabled);
    layout->addWidget(status);
    layout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll);
}

void ZzExampleControlPagePrivate::buildScroll()
{
    auto *horizontal = section(QCoreApplication::translate(
        "ZzPureToolsExample", "横向滚动条：悬停显示箭头，点击步进，长按连续滚动"));
    auto *vertical = section(QCoreApplication::translate(
        "ZzPureToolsExample", "纵向滚动条：普通与禁用状态"));
    for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
        QWidget *host = orientation == Qt::Horizontal ? horizontal : vertical;
        for (int index = 0; index < 2; ++index) {
            auto *preview = new QWidget(host);
            auto *previewLayout = new QVBoxLayout(preview);
            previewLayout->setContentsMargins(0, 0, 0, 0);
            previewLayout->setSpacing(12);
            auto *label = new QLabel(index == 0
                    ? QCoreApplication::translate("ZzPureToolsExample", "普通")
                    : QCoreApplication::translate("ZzPureToolsExample", "不可用"), preview);
            previewLayout->addWidget(label);
            auto *bar = new ZzFluentUI::ZzScrollBar(orientation, preview);
            const QString name = orientation == Qt::Horizontal
                ? QStringLiteral("zzExampleScrollHorizontal")
                : QStringLiteral("zzExampleScrollVertical");
            bar->setObjectName(index == 0 ? name : name + QStringLiteral("Disabled"));
            bar->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample",
                "滚动位置"));
            bar->setRange(0, 100);
            bar->setPageStep(25);
            bar->setValue(35);
            bar->setEnabled(index == 0);
            bar->setFocusPolicy(Qt::StrongFocus);
            if (orientation == Qt::Horizontal) {
                bar->setFixedWidth(280);
            } else {
                bar->setFixedHeight(160);
            }
            QObject::connect(bar, &QScrollBar::valueChanged, status, [this](int value) {
                status->setText(QCoreApplication::translate("ZzPureToolsExample",
                    "滚动位置：%1 / 100").arg(value));
            });
            previewLayout->addWidget(bar);
            zzAdd(host, preview);
        }
    }
    auto *host = section(QCoreApplication::translate(
        "ZzPureToolsExample", "双轴滚动区域：滚轮纵向滚动，拖动底部滚动条横向滚动"));
    auto *area = new ZzFluentUI::ZzScrollArea(host);
    area->setObjectName(QStringLiteral("zzExampleScrollArea"));
    area->setFixedSize(320, 200);
    auto *sheet = new QWidget;
    auto *rows = new QVBoxLayout(sheet);
    for (int row = 1; row <= 16; ++row) {
        rows->addWidget(new QLabel(QCoreApplication::translate("ZzPureToolsExample",
            "第 %1 行    ·    横向和纵向滚动共享库内样式    ·    向右滚动查看更多内容")
                .arg(row), sheet));
    }
    sheet->setMinimumSize(640, 480);
    area->setWidget(sheet);
    zzAdd(host, area);
}

QWidget *ZzExampleControlPagePrivate::section(const QString &title)
{
    auto *label = new QLabel(title, content);
    label->setWordWrap(true);
    QFont font = label->font();
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    layout->addWidget(label);
    auto *host = new QWidget(content);
    host->setObjectName(QStringLiteral("zzExampleControlPreview"));
    new ZzFluentUI::ZzFlowLayout(12, 12, host);
    layout->addWidget(host);
    return host;
}

void ZzExampleControlPagePrivate::buildButtons(ZzExampleControlKind kind)
{
    auto *host
        = section(QCoreApplication::translate("ZzPureToolsExample", "标准、强调色、轻量与禁用"));
    const std::array appearances { ZzFluentUI::ZzButtonAppearance::Standard,
        ZzFluentUI::ZzButtonAppearance::Accent, ZzFluentUI::ZzButtonAppearance::Subtle,
        ZzFluentUI::ZzButtonAppearance::Standard };
    const std::array labels { QCoreApplication::translate("ZzPureToolsExample", "标准"),
        QCoreApplication::translate("ZzPureToolsExample", "主要操作"),
        QCoreApplication::translate("ZzPureToolsExample", "次要操作"),
        QCoreApplication::translate("ZzPureToolsExample", "不可用") };
    for (std::size_t i = 0; i < appearances.size(); ++i) {
        QAbstractButton *button = nullptr;
        if (kind == ZzExampleControlKind::PushButton) {
            button = new ZzFluentUI::ZzPushButton(labels[i], host);
        } else if (kind == ZzExampleControlKind::IconButton) {
            auto *icon = new ZzFluentUI::ZzIconButton(host);
            icon->setIconDescriptor(
                ZzFluentUI::ZzIconDescriptor::fromFontIcon(ZzFluentUI::ZzFontIcon::Star));
            icon->setFixedSize(32, 32);
            button = icon;
        } else {
            auto *tool = new QToolButton(host);
            tool->setObjectName(QStringLiteral("zzExampleToolButton%1").arg(i));
            tool->setText(labels[i]);
            tool->setAutoRaise(i == 2);
            tool->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            tool->setIconSize(QSize(18, 18));
            tool->setMinimumSize(36, 36);
            button = tool;
        }
        ZzFluentUI::ZzControlAppearance::setButtonAppearance(button, appearances[i]);
        button->setAccessibleName(labels[i]);
        button->setEnabled(i != 3);
        QObject::connect(button, &QAbstractButton::clicked, status, [this, text = labels[i]] {
            status->setText(
                QCoreApplication::translate("ZzPureToolsExample", "已点击：%1").arg(text));
        });
        zzAdd(host, button);
    }
    if (kind == ZzExampleControlKind::PushButton) {
        auto *variants
            = section(QCoreApplication::translate("ZzPureToolsExample", "可选中与局部强调色"));
        auto *toggle = new ZzFluentUI::ZzPushButton(
            QCoreApplication::translate("ZzPureToolsExample", "保持预览"), variants);
        toggle->setObjectName(QStringLiteral("zzExampleCheckableButton"));
        toggle->setCheckable(true);
        toggle->setChecked(true);
        QObject::connect(toggle, &QAbstractButton::toggled, status, [this](bool checked) {
            status->setText(checked
                    ? QCoreApplication::translate("ZzPureToolsExample", "预览已保持")
                    : QCoreApplication::translate("ZzPureToolsExample", "预览已释放"));
        });
        zzAdd(variants, toggle);
        auto *local = new ZzFluentUI::ZzPushButton(
            QCoreApplication::translate("ZzPureToolsExample", "紫色按钮"), variants);
        local->setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        ZzFluentUI::ZzControlAppearance::setAccentColor(local, QColor(QStringLiteral("#8752b5")));
        zzAdd(variants, local);
        auto *native = new QPushButton(
            QCoreApplication::translate("ZzPureToolsExample", "原生强调色按钮"), variants);
        ZzFluentUI::ZzControlAppearance::setButtonAppearance(
            native, ZzFluentUI::ZzButtonAppearance::Accent);
        ZzFluentUI::ZzControlAppearance::setAccentColor(native, QColor(QStringLiteral("#167344")));
        zzAdd(variants, native);
        auto *reset = new ZzFluentUI::ZzPushButton(
            QCoreApplication::translate("ZzPureToolsExample", "恢复跟随主题"), variants);
        QObject::connect(reset, &QAbstractButton::clicked, local, [local, native] {
            ZzFluentUI::ZzControlAppearance::resetAccentColor(local);
            ZzFluentUI::ZzControlAppearance::resetAccentColor(native);
        });
        zzAdd(variants, reset);
    } else if (kind == ZzExampleControlKind::ToolButton) {
        auto *icons = section(QCoreApplication::translate("ZzPureToolsExample", "纯图标与保持选中"));
        for (int i = 0; i < 3; ++i) {
            auto *button = new QToolButton(icons);
            button->setObjectName(QStringLiteral("zzExampleToolButtonIcon%1").arg(i));
            const QString label = QCoreApplication::translate("ZzPureToolsExample", "收藏");
            button->setAccessibleName(label);
            button->setToolTip(label);
            button->setIconSize(QSize(18, 18));
            button->setMinimumSize(36, 36);
            button->setAutoRaise(i == 1);
            button->setCheckable(true);
            button->setChecked(i == 2);
            QObject::connect(button, &QToolButton::toggled, status, [this](bool checked) {
                status->setText(checked
                    ? QCoreApplication::translate("ZzPureToolsExample", "已选中")
                    : QCoreApplication::translate("ZzPureToolsExample", "未选中"));
            });
            zzAdd(icons, button);
        }
        auto *variants = section(QCoreApplication::translate("ZzPureToolsExample", "菜单按钮"));
        auto *tool = new QToolButton(variants);
        tool->setObjectName(QStringLiteral("zzExampleToolButtonMenu"));
        tool->setText(QCoreApplication::translate("ZzPureToolsExample", "更多操作"));
        tool->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tool->setIconSize(QSize(18, 18));
        tool->setMinimumHeight(36);
        auto *menu = new QMenu(tool);
        menu->addAction(QCoreApplication::translate("ZzPureToolsExample", "复制"), status, [this] {
            status->setText(QCoreApplication::translate("ZzPureToolsExample", "已选择复制"));
        });
        menu->addAction(QCoreApplication::translate("ZzPureToolsExample", "粘贴"), status, [this] {
            status->setText(QCoreApplication::translate("ZzPureToolsExample", "已选择粘贴"));
        });
        tool->setMenu(menu);
        tool->setPopupMode(QToolButton::InstantPopup);
        zzAdd(variants, tool);
        refreshToolIcons();
    }
}

void ZzExampleControlPagePrivate::refreshToolIcons()
{
    auto *style = qobject_cast<ZzFluentUI::ZzFluentStyle *>(q_ptr->style());
    if (style == nullptr) {
        return;
    }
    const auto descriptor = ZzFluentUI::ZzIconDescriptor::fromFontIcon(ZzFluentUI::ZzFontIcon::Star);
    for (auto *tool : q_ptr->findChildren<QToolButton *>()) {
        if (!tool->objectName().startsWith(QStringLiteral("zzExampleToolButton"))) {
            continue;
        }
        QIcon icon;
        for (const auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected}) {
            const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
            const auto role = tool->property("accent").toBool() && mode != QIcon::Disabled
                ? QPalette::HighlightedText : QPalette::ButtonText;
            const auto pixmap = style->iconPixmap(descriptor, tool->iconSize(),
                q_ptr->devicePixelRatioF(), q_ptr->palette().color(group, role), tool->layoutDirection());
            icon.addPixmap(pixmap, mode, QIcon::Off);
            icon.addPixmap(pixmap, mode, QIcon::On);
        }
        tool->setIcon(icon);
    }
}

void ZzExampleControlPagePrivate::buildSelection(ZzExampleControlKind kind)
{
    auto *host = section(QCoreApplication::translate("ZzPureToolsExample", "选择与禁用状态"));
    if (kind == ZzExampleControlKind::RadioButton || kind == ZzExampleControlKind::CheckBox
        || kind == ZzExampleControlKind::ToggleSwitch) {
        auto *column = new QWidget(host);
        auto *choices = new QVBoxLayout(column);
        choices->setContentsMargins(0, 0, 0, 0);
        choices->setSpacing(8);
        zzAdd(host, column);
        host = column;
    }
    auto *group = kind == ZzExampleControlKind::RadioButton ? new QButtonGroup(host) : nullptr;
    const std::array labels { QCoreApplication::translate("ZzPureToolsExample", "选项一"),
        QCoreApplication::translate("ZzPureToolsExample", "选项二"),
        QCoreApplication::translate("ZzPureToolsExample", "不可用") };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        QAbstractButton *button = nullptr;
        if (kind == ZzExampleControlKind::RadioButton) {
            button = new QRadioButton(labels[i], host);
            if (group != nullptr) {
                group->addButton(button);
            }
        } else if (kind == ZzExampleControlKind::CheckBox) {
            button = new QCheckBox(labels[i], host);
        } else {
            button = new ZzFluentUI::ZzToggleSwitch(labels[i], host);
        }
        button->setChecked(i == 0);
        button->setEnabled(i != 2);
        QObject::connect(
            button, &QAbstractButton::toggled, status, [this, text = labels[i]](bool checked) {
                status->setText(QCoreApplication::translate("ZzPureToolsExample", "%1：%2")
                        .arg(text,
                            checked ? QCoreApplication::translate("ZzPureToolsExample", "已选中")
                                    : QCoreApplication::translate("ZzPureToolsExample", "未选中")));
            });
        zzAdd(host, button);
    }
    {
        const auto label = QCoreApplication::translate("ZzPureToolsExample", "已选中（禁用）");
        QAbstractButton *disabled = nullptr;
        if (kind == ZzExampleControlKind::RadioButton) {
            auto *radio = new QRadioButton(label, host);
            radio->setAutoExclusive(false);
            disabled = radio;
        } else if (kind == ZzExampleControlKind::CheckBox) {
            disabled = new QCheckBox(label, host);
        } else {
            disabled = new ZzFluentUI::ZzToggleSwitch(label, host);
        }
        disabled->setChecked(true);
        disabled->setEnabled(false);
        zzAdd(host, disabled);
    }
    if (kind == ZzExampleControlKind::CheckBox) {
        auto *mixed = section(QCoreApplication::translate("ZzPureToolsExample", "三态选择"));
        auto *check
            = new QCheckBox(QCoreApplication::translate("ZzPureToolsExample", "部分选中"), mixed);
        check->setTristate(true);
        check->setCheckState(Qt::PartiallyChecked);
        zzAdd(mixed, check);
    } else if (kind == ZzExampleControlKind::ToggleSwitch) {
        auto *accent = section(QCoreApplication::translate("ZzPureToolsExample", "局部强调色"));
        auto *toggle = new ZzFluentUI::ZzToggleSwitch(
            QCoreApplication::translate("ZzPureToolsExample", "局部强调色"), accent);
        toggle->setChecked(true);
        ZzFluentUI::ZzControlAppearance::setAccentColor(toggle, QColor(QStringLiteral("#8752b5")));
        zzAdd(accent, toggle);
    }
}

void ZzExampleControlPagePrivate::buildInput(ZzExampleControlKind kind)
{
    if (kind == ZzExampleControlKind::LineEdit) {
        auto *host
            = section(QCoreApplication::translate("ZzPureToolsExample", "普通、密码与只读输入"));
        for (int i = 0; i < 3; ++i) {
            QLineEdit *edit = nullptr;
            if (i == 1) {
                auto *password = new ZzFluentUI::ZzPasswordBox(host);
                password->setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Toggle);
                edit = password;
            } else {
                edit = new QLineEdit(host);
            }
            edit->setMinimumWidth(220);
            edit->setPlaceholderText(i == 1
                    ? QCoreApplication::translate("ZzPureToolsExample", "输入密码")
                    : QCoreApplication::translate("ZzPureToolsExample", "输入文本"));
            edit->setAccessibleName(edit->placeholderText());
            edit->setClearButtonEnabled(i != 1);
            if (i == 2) {
                edit->setText(QCoreApplication::translate("ZzPureToolsExample", "只读内容"));
                edit->setReadOnly(true);
            }
            QObject::connect(edit, &QLineEdit::textChanged, status, [this](const QString &text) {
                status->setText(QCoreApplication::translate("ZzPureToolsExample", "字符数：%1")
                        .arg(text.size()));
            });
            zzAdd(host, edit);
        }
    } else if (kind == ZzExampleControlKind::PlainTextEdit) {
        auto *host
            = section(QCoreApplication::translate("ZzPureToolsExample", "多行输入与只读内容"));
        for (int i = 0; i < 2; ++i) {
            auto *edit = new QPlainTextEdit(host);
            edit->setMinimumWidth(260);
            edit->setFixedHeight(150);
            edit->setPlaceholderText(
                QCoreApplication::translate("ZzPureToolsExample", "在此输入多行文本"));
            edit->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "多行文本"));
            if (i == 1) {
                edit->setPlainText(QCoreApplication::translate("ZzPureToolsExample", "只读内容"));
                edit->setReadOnly(true);
            }
            QObject::connect(edit, &QPlainTextEdit::textChanged, status, [this, edit] {
                status->setText(QCoreApplication::translate("ZzPureToolsExample", "字符数：%1")
                        .arg(edit->toPlainText().size()));
            });
            zzAdd(host, edit);
        }
    } else if (kind == ZzExampleControlKind::ComboBox) {
        auto *host
            = section(QCoreApplication::translate("ZzPureToolsExample", "标准与可编辑组合框"));
        for (int i = 0; i < 2; ++i) {
            auto *combo = new QComboBox(host);
            combo->setMinimumWidth(240);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            combo->addItems({ QStringLiteral("Linux Desktop"), QStringLiteral("Windows Desktop"),
                QStringLiteral("macOS Desktop") });
            combo->setEditable(i == 1);
            combo->setAccessibleName(i == 1
                    ? QCoreApplication::translate("ZzPureToolsExample", "可编辑选项")
                    : QCoreApplication::translate("ZzPureToolsExample", "标准选项"));
            QObject::connect(combo, &QComboBox::currentTextChanged, status, &QLabel::setText);
            zzAdd(host, combo);
        }
        auto *variants = section(
            QCoreApplication::translate("ZzPureToolsExample", "占位、图标与禁用状态"));
        auto *placeholder = new QComboBox(variants);
        placeholder->setMinimumWidth(240);
        placeholder->setPlaceholderText(
            QCoreApplication::translate("ZzPureToolsExample", "请选择工作环境"));
        placeholder->addItems({QStringLiteral("Development"), QStringLiteral("Production")});
        placeholder->setCurrentIndex(-1);
        placeholder->setAccessibleName(
            QCoreApplication::translate("ZzPureToolsExample", "占位选项"));
        QObject::connect(placeholder, &QComboBox::currentTextChanged, status, &QLabel::setText);
        zzAdd(variants, placeholder);

        auto *decorated = new QComboBox(variants);
        decorated->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        decorated->addItem(q_ptr->style()->standardIcon(QStyle::SP_DirIcon),
            QCoreApplication::translate("ZzPureToolsExample", "包含图标的开发工作目录"));
        decorated->addItem(q_ptr->style()->standardIcon(QStyle::SP_FileIcon),
            QCoreApplication::translate("ZzPureToolsExample", "Release configuration"));
        decorated->setAccessibleName(
            QCoreApplication::translate("ZzPureToolsExample", "带图标选项"));
        QObject::connect(decorated, &QComboBox::currentTextChanged, status, &QLabel::setText);
        zzAdd(variants, decorated);

        auto *disabled = new QComboBox(variants);
        disabled->setMinimumWidth(240);
        disabled->addItem(QCoreApplication::translate("ZzPureToolsExample", "不可用的选项"));
        disabled->setEnabled(false);
        zzAdd(variants, disabled);
    } else {
        auto *host
            = section(QCoreApplication::translate("ZzPureToolsExample", "多项选择与禁用选项"));
        auto *combo = new ZzFluentUI::ZzMultiSelectComboBox(host);
        combo->setMinimumWidth(280);
        combo->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "多项选择"));
        combo->setPlaceholderText(
            QCoreApplication::translate("ZzPureToolsExample", "选择构建范围"));
        combo->setOptions({ { QStringLiteral("shared"),
                                QCoreApplication::translate("ZzPureToolsExample", "共享库"), {}, {},
                                true, true },
            { QStringLiteral("static"), QCoreApplication::translate("ZzPureToolsExample", "静态库"),
                {}, {}, true, true },
            { QStringLiteral("tests"), QCoreApplication::translate("ZzPureToolsExample", "测试"),
                {}, {}, true, false },
            { QStringLiteral("unavailable"),
                QCoreApplication::translate("ZzPureToolsExample", "不可用"), {}, {}, false,
                false } });
        QObject::connect(combo, &ZzFluentUI::ZzMultiSelectComboBox::selectionChanged, status,
            [this, combo] { status->setText(combo->selectedText()); });
        zzAdd(host, combo);
    }
}

void ZzExampleControlPagePrivate::buildValue(ZzExampleControlKind kind)
{
    if (kind == ZzExampleControlKind::SpinBox || kind == ZzExampleControlKind::DoubleSpinBox) {
        using ZzFluentUI::ZzSpinBoxButtonLayout;
        const std::array modes{
            ZzSpinBoxButtonLayout::Vertical,
            ZzSpinBoxButtonLayout::HorizontalSides,
            ZzSpinBoxButtonLayout::HorizontalRight,
            ZzSpinBoxButtonLayout::PlusMinusHorizontalSides};
        const std::array titles{
            QCoreApplication::translate("ZzPureToolsExample", "右侧竖排箭头（Vertical）：上增、下减"),
            QCoreApplication::translate("ZzPureToolsExample", "两侧箭头（HorizontalSides）：左减、右增"),
            QCoreApplication::translate("ZzPureToolsExample", "右侧横排箭头（HorizontalRight）：右侧先减后增，默认模式"),
            QCoreApplication::translate("ZzPureToolsExample", "两侧加减（PlusMinusHorizontalSides）：左减号、右加号")};
        for (std::size_t index = 0; index < modes.size(); ++index) {
            auto *modeHost = section(titles[index]);
            if (kind == ZzExampleControlKind::SpinBox) {
                auto *spin = new ZzFluentUI::ZzSpinBox(modeHost);
                spin->setButtonLayout(modes[index]);
                spin->setRange(1, 64);
                spin->setValue(8);
                spin->setSuffix(QCoreApplication::translate("ZzPureToolsExample", " 线程"));
                spin->setMinimumWidth(240);
                spin->setAccessibleName(titles[index]);
                QObject::connect(spin, &QSpinBox::valueChanged, status,
                    [this](int value) { status->setText(QString::number(value)); });
                zzAdd(modeHost, spin);
            } else {
                auto *spin = new ZzFluentUI::ZzDoubleSpinBox(modeHost);
                spin->setButtonLayout(modes[index]);
                spin->setRange(0.1, 50.0);
                spin->setDecimals(2);
                spin->setSingleStep(0.25);
                spin->setValue(8.0);
                spin->setSuffix(QStringLiteral(" MiB"));
                spin->setMinimumWidth(240);
                spin->setAccessibleName(titles[index]);
                QObject::connect(spin, &QDoubleSpinBox::valueChanged, status,
                    [this](double value) { status->setText(QString::number(value, 'f', 2)); });
                zzAdd(modeHost, spin);
            }
        }
        return;
    }
    auto *host = section(QCoreApplication::translate("ZzPureToolsExample", "基本用法"));
    if (kind == ZzExampleControlKind::CalendarPicker) {
        auto *date = new ZzFluentUI::ZzCalendarPicker(host);
        date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        date->setDateRange(QDate(2026, 1, 1), QDate(2035, 12, 31));
        date->setDate(QDate(2026, 8, 6));
        date->setMinimumWidth(220);
        date->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "计划日期"));
        QObject::connect(date, &QDateTimeEdit::dateChanged, status,
            [this](const QDate &value) { status->setText(value.toString(Qt::ISODate)); });
        zzAdd(host, date);
        auto *calendarHost = section(QCoreApplication::translate("ZzPureToolsExample", "日历(Calendar)"));
        auto *calendar = new ZzFluentUI::ZzCalendar(calendarHost);
        calendar->setDateRange(date->minimumDate(), date->maximumDate());
        calendar->setSelectedDate(date->date());
        QObject::connect(date, &QDateTimeEdit::dateChanged, calendar, &QCalendarWidget::setSelectedDate);
        QObject::connect(calendar, &QCalendarWidget::selectionChanged, date,
            [date, calendar] { date->setDate(calendar->selectedDate()); });
        zzAdd(calendarHost, calendar);
    } else {
        auto *roller = new ZzFluentUI::ZzRollerPicker(host);
        QStringList hours;
        QStringList minutes;
        for (int i = 0; i < 24; ++i)
            hours.append(QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0')));
        for (int i = 0; i < 60; ++i)
            minutes.append(QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0')));
        roller->setColumns({ { QStringLiteral("hour"), hours, 9, true, 88 },
            { QStringLiteral("minute"), minutes, 30, true, 88 } });
        roller->setMinimumWidth(220);
        roller->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "计划时间"));
        QObject::connect(
            roller, &ZzFluentUI::ZzRollerPicker::currentTextChanged, status, &QLabel::setText);
        zzAdd(host, roller);
    }
}

void ZzExampleControlPagePrivate::buildProgress(ZzExampleControlKind kind)
{
    auto *host = section(kind == ZzExampleControlKind::Slider
            ? QCoreApplication::translate("ZzPureToolsExample", "水平与垂直滑块")
            : kind == ZzExampleControlKind::ProgressBar
            ? QCoreApplication::translate("ZzPureToolsExample", "细线(Thin)：确定进度、忙碌与禁用")
            : QCoreApplication::translate("ZzPureToolsExample", "确定进度、忙碌与禁用状态"));
    if (kind == ZzExampleControlKind::Slider) {
        auto *horizontal = new QSlider(Qt::Horizontal, host);
        horizontal->setRange(0, 100);
        horizontal->setValue(68);
        horizontal->setMinimumWidth(240);
        horizontal->setAccessibleName(
            QCoreApplication::translate("ZzPureToolsExample", "水平滑块"));
        auto *vertical = new QSlider(Qt::Vertical, host);
        vertical->setRange(0, 100);
        vertical->setValue(68);
        vertical->setFixedHeight(150);
        vertical->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "垂直滑块"));
        static_cast<void>(ZzFluentUI::ZzSliderValueTip::attach(horizontal));
        static_cast<void>(ZzFluentUI::ZzSliderValueTip::attach(vertical));
        QObject::connect(horizontal, &QSlider::valueChanged, vertical, &QSlider::setValue);
        QObject::connect(vertical, &QSlider::valueChanged, horizontal, &QSlider::setValue);
        QObject::connect(horizontal, &QSlider::valueChanged, status,
            [this](int value) { status->setText(QString::number(value) + QLatin1Char('%')); });
        zzAdd(host, horizontal);
        zzAdd(host, vertical);
        auto *rawHost = section(QCoreApplication::translate(
            "ZzPureToolsExample", "原始数值提示（范围 -20～80）"));
        auto *raw = new QSlider(Qt::Horizontal, rawHost);
        raw->setRange(-20, 80);
        raw->setValue(20);
        raw->setMinimumWidth(240);
        raw->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "原始数值滑块"));
        static_cast<void>(ZzFluentUI::ZzSliderValueTip::attach(
            raw, ZzFluentUI::ZzSliderValueTipMode::Value));
        zzAdd(rawHost, raw);
        return;
    }
    const bool ringMode = kind == ZzExampleControlKind::ProgressRing;
    QProgressBar *determinate = nullptr;
    QProgressBar *thickDeterminate = nullptr;
    std::array<ZzFluentUI::ZzProgressRing *, 3> rings{};
    for (int i = 0; i < 3; ++i) {
        QProgressBar *bar
            = ringMode ? new ZzFluentUI::ZzProgressRing(host) : new QProgressBar(host);
        if (ringMode)
            rings[static_cast<std::size_t>(i)] = static_cast<ZzFluentUI::ZzProgressRing *>(bar);
        bar->setRange(0, i == 1 ? 0 : 100);
        bar->setValue(i == 2 ? 42 : 68);
        bar->setTextVisible(i != 1);
        bar->setFormat(QStringLiteral("%p%"));
        bar->setEnabled(i != 2);
        if (!ringMode)
            bar->setMinimumWidth(240);
        bar->setAccessibleName(i == 1
                ? QCoreApplication::translate("ZzPureToolsExample", "忙碌进度")
                : QCoreApplication::translate("ZzPureToolsExample", "确定进度"));
        bar->setObjectName(i == 0 ? QStringLiteral("zzExampleProgressDeterminate")
                : i == 1          ? QStringLiteral("zzExampleProgressBusy")
                                  : QStringLiteral("zzExampleProgressDisabled"));
        if (i == 0)
            determinate = bar;
        zzAdd(host, bar);
    }
    if (ringMode) {
        auto *compactHost = section(QCoreApplication::translate(
            "ZzPureToolsExample", "紧凑加载指示：无文字、细圆环"));
        auto *compact = new ZzFluentUI::ZzProgressRing(compactHost);
        compact->setFixedSize(32, 32);
        compact->setRingWidth(3);
        compact->setRange(0, 0);
        compact->setTextVisible(false);
        compact->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "紧凑加载指示"));
        zzAdd(compactHost, compact);
        auto *options = section(QCoreApplication::translate("ZzPureToolsExample", "圆环外观与速度"));
        auto *width = new ZzFluentUI::ZzSpinBox(options);
        width->setRange(1, 16);
        width->setValue(rings[0]->ringWidth());
        width->setPrefix(QCoreApplication::translate("ZzPureToolsExample", "环宽："));
        width->setSuffix(QStringLiteral(" px"));
        width->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "圆环线宽"));
        for (auto *ring : rings)
            QObject::connect(width, &QSpinBox::valueChanged, ring, &ZzFluentUI::ZzProgressRing::setRingWidth);
        zzAdd(options, width);
        auto *duration = new ZzFluentUI::ZzSpinBox(options);
        duration->setRange(200, 60000);
        duration->setSingleStep(100);
        duration->setValue(rings[1]->indeterminateDuration());
        duration->setPrefix(QCoreApplication::translate("ZzPureToolsExample", "周期："));
        duration->setSuffix(QStringLiteral(" ms"));
        duration->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "旋转一周的毫秒数"));
        QObject::connect(duration, &QSpinBox::valueChanged, rings[1],
            &ZzFluentUI::ZzProgressRing::setIndeterminateDuration);
        QObject::connect(duration, &QSpinBox::valueChanged, compact,
            &ZzFluentUI::ZzProgressRing::setIndeterminateDuration);
        zzAdd(options, duration);
    }
    if (!ringMode) {
        auto *thickHost = section(QCoreApplication::translate(
            "ZzPureToolsExample", "粗线(Thick)：确定进度、忙碌与禁用"));
        for (int i = 0; i < 3; ++i) {
            auto *bar = new QProgressBar(thickHost);
            ZzFluentUI::ZzControlAppearance::setProgressBarAppearance(
                bar, ZzFluentUI::ZzProgressBarAppearance::Thick);
            bar->setRange(0, i == 1 ? 0 : 100);
            bar->setValue(i == 2 ? 42 : 68);
            bar->setTextVisible(i != 1);
            bar->setEnabled(i != 2);
            bar->setMinimumWidth(240);
            bar->setAccessibleName(i == 1
                    ? QCoreApplication::translate("ZzPureToolsExample", "粗线忙碌进度")
                    : QCoreApplication::translate("ZzPureToolsExample", "粗线确定进度"));
            if (i == 0)
                thickDeterminate = bar;
            zzAdd(thickHost, bar);
        }
        auto *verticalHost = section(QCoreApplication::translate("ZzPureToolsExample", "垂直进度"));
        auto *vertical = new QProgressBar(verticalHost);
        vertical->setObjectName(QStringLiteral("zzExampleProgressVertical"));
        vertical->setOrientation(Qt::Vertical);
        vertical->setValue(64);
        vertical->setFixedHeight(112);
        zzAdd(verticalHost, vertical);
    }
    auto *settings = section(QCoreApplication::translate("ZzPureToolsExample", "调整进度值"));
    auto *value = new QSlider(Qt::Horizontal, settings);
    value->setObjectName(QStringLiteral("zzExampleControlValue"));
    value->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "进度值"));
    value->setRange(0, 100);
    value->setValue(68);
    value->setMinimumWidth(240);
    QObject::connect(value, &QSlider::valueChanged, determinate, &QProgressBar::setValue);
    if (thickDeterminate != nullptr)
        QObject::connect(value, &QSlider::valueChanged, thickDeterminate, &QProgressBar::setValue);
    static_cast<void>(ZzFluentUI::ZzSliderValueTip::attach(value));
    QObject::connect(value, &QSlider::valueChanged, status,
        [this](int current) { status->setText(QString::number(current)); });
    zzAdd(settings, value);
}

void ZzExampleControlPagePrivate::buildFeedback(ZzExampleControlKind kind)
{
    const std::array severities { ZzFluentUI::ZzMessageSeverity::Information,
        ZzFluentUI::ZzMessageSeverity::Success, ZzFluentUI::ZzMessageSeverity::Warning,
        ZzFluentUI::ZzMessageSeverity::Error };
    const std::array labels { QCoreApplication::translate("ZzPureToolsExample", "信息"),
        QCoreApplication::translate("ZzPureToolsExample", "成功"),
        QCoreApplication::translate("ZzPureToolsExample", "警告"),
        QCoreApplication::translate("ZzPureToolsExample", "错误") };
    for (std::size_t i = 0; i < severities.size(); ++i) {
        auto *host = section(labels[i]);
        if (kind == ZzExampleControlKind::MessageBar) {
            auto *message = new ZzFluentUI::ZzMessageBar(host);
            message->setText(labels[i]);
            message->setSeverity(severities[i]);
            QObject::connect(
                message, &ZzFluentUI::ZzMessageBar::closeRequested, message, &QWidget::hide);
            zzAdd(host, message);
            auto *restore = new ZzFluentUI::ZzPushButton(
                QCoreApplication::translate("ZzPureToolsExample", "重新显示"), host);
            QObject::connect(restore, &QAbstractButton::clicked, message, &QWidget::show);
            zzAdd(host, restore);
        } else {
            auto *badge = new ZzFluentUI::ZzInfoBadge(host);
            badge->setSeverity(severities[i]);
            zzAdd(host, badge);
            auto *number = new ZzFluentUI::ZzInfoBadge(host);
            number->setKind(ZzFluentUI::ZzInfoBadgeKind::Number);
            number->setSeverity(severities[i]);
            number->setValue(12);
            zzAdd(host, number);
        }
    }
}

} // namespace ZzExample
