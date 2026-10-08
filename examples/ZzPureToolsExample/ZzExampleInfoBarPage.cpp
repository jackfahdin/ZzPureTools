#include "ZzExampleRadialGaugeHelpers.h"
#include "ZzExampleShowcasePagePrivate.h"

#include <QGridLayout>
#include <QProgressBar>
#include <QSpinBox>
#include <ZzFluentUI/ZzInfoBar.h>
#include <ZzFluentUI/ZzInfoBarHost.h>
#include <array>

namespace ZzExample {
using ZzFluentUI::ZzInfoBar;
using ZzFluentUI::ZzInfoBarHost;

namespace {
    constexpr std::array<const char*, 6> positions { "左上", "顶部", "右上", "左下", "底部",
        "右下" };

    QFormLayout* makeInfoForm(QWidget* page)
    {
        auto* form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setVerticalSpacing(10);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        return form;
    }

    QSpinBox* makeInfoSpin(QWidget* parent, int maximum, int value)
    {
        auto* spin = new QSpinBox(parent);
        spin->setRange(0, maximum);
        spin->setValue(value);
        return spin;
    }

    void buildInfoEditor(QVBoxLayout* mainLayout, QWidget* content, ZzInfoBarHost* host)
    {
        auto* card = makeCard(content);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);
        layout->addWidget(makeSectionTitle(zzGaugeText("实时属性"), card));
        auto* preview = new ZzInfoBar(card);
        preview->setObjectName(QStringLiteral("infoBarPreview"));
        preview->setTitle(zzGaugeText("信息"));
        preview->setMessage(zzGaugeText("新版本已经可以下载。"));
        preview->setActionButtonText(zzGaugeText("查看更新"));
        preview->setOpen(true);
        layout->addWidget(preview);

        auto* tabs = makePropertyTabs(card);
        tabs->setObjectName(QStringLiteral("infoBarEditorTabs"));
        auto* contentPage = new QWidget(tabs);
        auto* behaviorPage = new QWidget(tabs);
        auto* popupPage = new QWidget(tabs);
        auto* contentForm = makeInfoForm(contentPage);
        auto* behaviorForm = makeInfoForm(behaviorPage);
        auto* popupForm = makeInfoForm(popupPage);
        tabs->addTab(contentPage, zzGaugeText("内容"));
        tabs->addTab(behaviorPage, zzGaugeText("行为"));
        tabs->addTab(popupPage, zzGaugeText("弹出通知"));
        layout->addWidget(tabs);

        auto* severity = new QComboBox(contentPage);
        severity->setObjectName(QStringLiteral("infoBarSeverity"));
        for (const char* text : { "信息", "成功", "警告", "错误" })
            severity->addItem(zzGaugeText(text));
        auto* title = new QLineEdit(preview->title(), contentPage);
        title->setObjectName(QStringLiteral("infoBarTitle"));
        auto* message = new QPlainTextEdit(preview->message(), contentPage);
        message->setObjectName(QStringLiteral("infoBarMessage"));
        message->setFixedHeight(80);
        auto* action = new QLineEdit(preview->actionButtonText(), contentPage);
        action->setObjectName(QStringLiteral("infoBarAction"));
        auto* custom = new QCheckBox(zzGaugeText("使用自定义进度操作"), contentPage);
        custom->setObjectName(QStringLiteral("infoBarCustomAction"));
        contentForm->addRow(zzGaugeText("级别"), severity);
        contentForm->addRow(zzGaugeText("标题"), title);
        contentForm->addRow(zzGaugeText("消息"), message);
        contentForm->addRow(zzGaugeText("操作文字"), action);
        contentForm->addRow(custom);
        QObject::connect(severity, &QComboBox::currentIndexChanged, preview, [preview](int index) {
            preview->setSeverity(static_cast<ZzInfoBar::Severity>(index));
        });
        QObject::connect(title, &QLineEdit::textChanged, preview, &ZzInfoBar::setTitle);
        QObject::connect(message, &QPlainTextEdit::textChanged, preview,
            [preview, message] { preview->setMessage(message->toPlainText()); });
        QObject::connect(action, &QLineEdit::textChanged, preview, &ZzInfoBar::setActionButtonText);
        QObject::connect(custom, &QCheckBox::toggled, preview, [preview](bool enabled) {
            if (!enabled) {
                preview->setActionWidget(nullptr);
                return;
            }
            auto* widget = new QWidget;
            auto* row = new QHBoxLayout(widget);
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(8);
            auto* progress = new QProgressBar(widget);
            progress->setRange(0, 100);
            progress->setValue(65);
            progress->setFixedWidth(110);
            auto* button = new QPushButton(zzGaugeText("完成下载"), widget);
            row->addWidget(progress);
            row->addWidget(button);
            QObject::connect(
                button, &QPushButton::clicked, progress, [progress] { progress->setValue(100); });
            preview->setActionWidget(widget);
        });

        const auto makeCheck
            = [behaviorPage, behaviorForm](const char* text, const char* name, bool checked) {
                  auto* check = new QCheckBox(zzGaugeText(text), behaviorPage);
                  check->setObjectName(QString::fromLatin1(name));
                  check->setChecked(checked);
                  behaviorForm->addRow(check);
                  return check;
              };
        auto* open = makeCheck("显示信息栏", "infoBarOpen", true);
        auto* closable = makeCheck("允许关闭", "infoBarClosable", true);
        auto* icon = makeCheck("显示状态图标", "infoBarIcon", true);
        auto* animation = makeCheck("展开收起动画", "infoBarAnimation", true);
        auto* disabled = makeCheck("禁用", "infoBarDisabled", false);
        auto* duration = makeInfoSpin(behaviorPage, 2000, 167);
        duration->setSuffix(QStringLiteral(" ms"));
        behaviorForm->addRow(zzGaugeText("动画时长"), duration);
        QObject::connect(open, &QCheckBox::toggled, preview, &ZzInfoBar::setOpen);
        QObject::connect(preview, &ZzInfoBar::openChanged, open, &QCheckBox::setChecked);
        QObject::connect(closable, &QCheckBox::toggled, preview, &ZzInfoBar::setClosable);
        QObject::connect(icon, &QCheckBox::toggled, preview, &ZzInfoBar::setIconVisible);
        QObject::connect(animation, &QCheckBox::toggled, preview, &ZzInfoBar::setAnimationEnabled);
        QObject::connect(disabled, &QCheckBox::toggled, preview, &QWidget::setDisabled);
        QObject::connect(
            duration, &QSpinBox::valueChanged, preview, &ZzInfoBar::setAnimationDuration);

        auto* position = new QComboBox(popupPage);
        for (const char* text : positions)
            position->addItem(zzGaugeText(text));
        position->setCurrentIndex(ZzInfoBarHost::TopRight);
        auto* timeout = makeInfoSpin(popupPage, 30000, 4500);
        timeout->setSpecialValueText(zzGaugeText("不自动关闭"));
        timeout->setSuffix(QStringLiteral(" ms"));
        auto* margin = makeInfoSpin(popupPage, 100, host->margin());
        auto* spacing = makeInfoSpin(popupPage, 60, host->spacing());
        auto* width = makeInfoSpin(popupPage, 720, host->maximumWidth());
        width->setMinimum(160);
        popupForm->addRow(zzGaugeText("弹出位置"), position);
        popupForm->addRow(zzGaugeText("停留时间"), timeout);
        popupForm->addRow(zzGaugeText("窗口边距"), margin);
        popupForm->addRow(zzGaugeText("通知间距"), spacing);
        popupForm->addRow(zzGaugeText("最大宽度"), width);
        QObject::connect(margin, &QSpinBox::valueChanged, host, &ZzInfoBarHost::setMargin);
        QObject::connect(spacing, &QSpinBox::valueChanged, host, &ZzInfoBarHost::setSpacing);
        QObject::connect(width, &QSpinBox::valueChanged, host, &ZzInfoBarHost::setMaximumWidth);
        auto* show = new QPushButton(zzGaugeText("弹出当前信息栏"), popupPage);
        show->setObjectName(QStringLiteral("infoBarShowPopup"));
        popupForm->addRow(show);
        QObject::connect(show, &QPushButton::clicked, host, [=] {
            host->showInfoBar(preview->severity(), preview->title(), preview->message(),
                static_cast<ZzInfoBarHost::Position>(position->currentIndex()), timeout->value());
        });

        auto* reset = new QPushButton(zzGaugeText("重置属性"), card);
        reset->setObjectName(QStringLiteral("infoBarReset"));
        layout->addWidget(reset, 0, Qt::AlignLeft);
        QObject::connect(reset, &QPushButton::clicked, card, [=] {
            severity->setCurrentIndex(0);
            title->setText(zzGaugeText("信息"));
            message->setPlainText(zzGaugeText("新版本已经可以下载。"));
            action->setText(zzGaugeText("查看更新"));
            custom->setChecked(false);
            closable->setChecked(true);
            icon->setChecked(true);
            animation->setChecked(true);
            disabled->setChecked(false);
            duration->setValue(167);
            open->setChecked(true);
            position->setCurrentIndex(ZzInfoBarHost::TopRight);
            timeout->setValue(4500);
            margin->setValue(24);
            spacing->setValue(8);
            width->setValue(360);
            host->dismissAll();
        });
        auto* code = new QPlainTextEdit(card);
        code->setReadOnly(true);
        code->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        code->setPlainText(
            QStringLiteral("auto *bar = new ZzFluentUI::ZzInfoBar(parent);\n"
                           "bar->setSeverity(ZzFluentUI::ZzInfoBar::Success);\n"
                           "bar->setTitle(tr(\"Saved\"));\n"
                           "bar->setMessage(tr(\"All changes have been saved.\"));\n"
                           "bar->setOpen(true);\n\n"
                           "auto *host = new ZzFluentUI::ZzInfoBarHost(window, page);\n"
                           "host->showInfoBar(ZzFluentUI::ZzInfoBar::Success, tr(\"Saved\"),\n"
                           "    tr(\"All changes have been saved.\"),\n"
                           "    ZzFluentUI::ZzInfoBarHost::TopRight, 4500);"));
        code->setMinimumHeight(200);
        layout->addWidget(code);
        mainLayout->addWidget(card);
    }
} // namespace

void ZzExampleShowcasePagePrivate::buildInfoBar(QVBoxLayout* mainLayout, QWidget* content)
{
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);
    auto* description = new QLabel(
        zzGaugeText("在页面内展示重要信息，或在窗口边缘弹出可自动关闭的通知。"), content);
    description->setWordWrap(true);
    mainLayout->addWidget(description);
    auto* host = new ZzInfoBarHost(q_ptr->window(), q_ptr);
    host->setObjectName(QStringLiteral("infoBarHost"));

    auto* card = makeCard(content);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeSectionTitle(zzGaugeText("页面内信息栏"), card));
    const std::array<const char*, 4> titles { "信息", "成功", "警告", "错误" };
    const std::array<const char*, 4> messages { "新版本已经可以下载。", "所有更改均已保存。",
        "网络连接不稳定，部分内容可能延迟。", "无法连接到服务，请稍后重试。" };
    QList<ZzInfoBar*> bars;
    for (int index = 0; index < 4; ++index) {
        auto* bar = new ZzInfoBar(card);
        bar->setObjectName(QStringLiteral("infoBarInline%1").arg(index));
        bar->setSeverity(static_cast<ZzInfoBar::Severity>(index));
        bar->setTitle(zzGaugeText(titles[index]));
        bar->setMessage(zzGaugeText(messages[index]));
        if (index == 0) {
            bar->setActionButtonText(zzGaugeText("查看更新"));
            QObject::connect(bar, &ZzInfoBar::actionTriggered, bar, [bar] {
                bar->setMessage(zzGaugeText("当前已经是最新版本。"));
                bar->setActionButtonText({});
            });
        }
        layout->addWidget(bar);
        bar->setOpen(true);
        bars.append(bar);
    }
    auto* reopen = new QPushButton(zzGaugeText("重新显示全部通知"), card);
    reopen->setObjectName(QStringLiteral("infoBarReopen"));
    layout->addWidget(reopen, 0, Qt::AlignLeft);
    QObject::connect(reopen, &QPushButton::clicked, card, [bars] {
        for (auto* bar : bars)
            bar->setOpen(true);
    });
    mainLayout->addWidget(card);

    auto* popupCard = makeCard(content);
    auto* popupLayout = new QVBoxLayout(popupCard);
    popupLayout->setContentsMargins(16, 16, 16, 16);
    popupLayout->setSpacing(12);
    popupLayout->addWidget(makeSectionTitle(zzGaugeText("窗口级弹出"), popupCard));
    auto* hint = new QLabel(zzGaugeText("通知在 4.5 "
                                        "秒后自动关闭，鼠标悬停时暂停计时；空间不足时按顺序排队。"),
        popupCard);
    hint->setWordWrap(true);
    popupLayout->addWidget(hint);
    auto* grid = new QGridLayout;
    for (int index = 0; index < 6; ++index) {
        auto* button = new QPushButton(zzGaugeText(positions[index]), popupCard);
        grid->addWidget(button, index / 3, index % 3);
        QObject::connect(button, &QPushButton::clicked, host, [host, index] {
            const int sequence = host->property("sequence").toInt() + 1;
            host->setProperty("sequence", sequence);
            host->showInfoBar(static_cast<ZzInfoBar::Severity>((sequence - 1) % 4),
                zzGaugeText("通知 %1").arg(sequence), zzGaugeText("鼠标悬停可暂停自动关闭。"),
                static_cast<ZzInfoBarHost::Position>(index));
        });
    }
    popupLayout->addLayout(grid);
    auto* burst = new QPushButton(zzGaugeText("在右上角连续弹出 10 条"), popupCard);
    popupLayout->addWidget(burst);
    QObject::connect(burst, &QPushButton::clicked, host, [host] {
        for (int index = 0; index < 10; ++index)
            host->showInfoBar(static_cast<ZzInfoBar::Severity>(index % 4),
                zzGaugeText("通知 %1").arg(index + 1),
                zzGaugeText("空间不足时等待，关闭后依次补位。"));
    });
    auto* dismiss = new QPushButton(zzGaugeText("关闭全部弹出通知"), popupCard);
    dismiss->setObjectName(QStringLiteral("infoBarDismissAll"));
    popupLayout->addWidget(dismiss);
    QObject::connect(dismiss, &QPushButton::clicked, host, [host] { host->dismissAll(); });
    mainLayout->addWidget(popupCard);
    buildInfoEditor(mainLayout, content, host);
}
} // namespace ZzExample
