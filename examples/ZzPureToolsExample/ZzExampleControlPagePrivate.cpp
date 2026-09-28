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
#include <ZzFluentUI/ZzControlAppearance.h>
#include <ZzFluentUI/ZzDoubleSpinBox.h>
#include <ZzFluentUI/ZzFlowLayout.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzInfoBadge.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzProgressRing.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzScrollArea.h>
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
            icon->setFixedSize(40, 40);
            button = icon;
        } else {
            auto *tool = new QToolButton(host);
            tool->setText(labels[i]);
            tool->setAutoRaise(i == 2);
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
        auto *variants = section(QCoreApplication::translate("ZzPureToolsExample", "菜单按钮"));
        auto *tool = new QToolButton(variants);
        tool->setText(QCoreApplication::translate("ZzPureToolsExample", "更多操作"));
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
    }
}

void ZzExampleControlPagePrivate::buildSelection(ZzExampleControlKind kind)
{
    auto *host = section(QCoreApplication::translate("ZzPureToolsExample", "选择与禁用状态"));
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
            auto *edit = new QLineEdit(host);
            edit->setMinimumWidth(220);
            edit->setPlaceholderText(i == 1
                    ? QCoreApplication::translate("ZzPureToolsExample", "输入密码")
                    : QCoreApplication::translate("ZzPureToolsExample", "输入文本"));
            edit->setAccessibleName(edit->placeholderText());
            edit->setClearButtonEnabled(true);
            if (i == 1)
                edit->setEchoMode(QLineEdit::Password);
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
    auto *host = section(QCoreApplication::translate("ZzPureToolsExample", "基本用法"));
    if (kind == ZzExampleControlKind::SpinBox) {
        auto *spin = new ZzFluentUI::ZzSpinBox(host);
        spin->setRange(1, 64);
        spin->setValue(8);
        spin->setSuffix(QCoreApplication::translate("ZzPureToolsExample", " 线程"));
        spin->setMinimumWidth(200);
        spin->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "整数值"));
        QObject::connect(spin, &QSpinBox::valueChanged, status,
            [this](int value) { status->setText(QString::number(value)); });
        zzAdd(host, spin);
    } else if (kind == ZzExampleControlKind::DoubleSpinBox) {
        auto *spin = new ZzFluentUI::ZzDoubleSpinBox(host);
        spin->setRange(0.1, 50.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.25);
        spin->setValue(8.0);
        spin->setSuffix(QStringLiteral(" MiB"));
        spin->setMinimumWidth(200);
        spin->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "小数值"));
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, status,
            [this](double value) { status->setText(QString::number(value, 'f', 2)); });
        zzAdd(host, spin);
    } else if (kind == ZzExampleControlKind::CalendarPicker) {
        auto *date = new ZzFluentUI::ZzCalendarPicker(host);
        date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        date->setDateRange(QDate(2026, 1, 1), QDate(2035, 12, 31));
        date->setDate(QDate(2026, 8, 6));
        date->setMinimumWidth(220);
        date->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "计划日期"));
        QObject::connect(date, &QDateTimeEdit::dateChanged, status,
            [this](const QDate &value) { status->setText(value.toString(Qt::ISODate)); });
        zzAdd(host, date);
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
        QObject::connect(horizontal, &QSlider::valueChanged, vertical, &QSlider::setValue);
        QObject::connect(vertical, &QSlider::valueChanged, horizontal, &QSlider::setValue);
        QObject::connect(horizontal, &QSlider::valueChanged, status,
            [this](int value) { status->setText(QString::number(value)); });
        zzAdd(host, horizontal);
        zzAdd(host, vertical);
        return;
    }
    const bool ringMode = kind == ZzExampleControlKind::ProgressRing;
    QProgressBar *determinate = nullptr;
    for (int i = 0; i < 3; ++i) {
        QProgressBar *bar
            = ringMode ? new ZzFluentUI::ZzProgressRing(host) : new QProgressBar(host);
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
    if (!ringMode) {
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
