#include "ZzExampleTimelineHelpers.h"

#include <QPointer>
#include <memory>

namespace ZzExample {

QColor timelineRailColor(const ZzTimeline* timeline)
{
    if (timeline->lineColor().isValid())
        return timeline->lineColor();
    const auto palette = timeline->palette();
    const QColor background = palette.color(QPalette::Active, QPalette::Base);
    const QColor text = palette.color(QPalette::Active, QPalette::Text);
    return QColor(qRound(background.red() * 0.78 + text.red() * 0.22),
        qRound(background.green() * 0.78 + text.green() * 0.22),
        qRound(background.blue() * 0.78 + text.blue() * 0.22));
}

void resetTimelineEvents(ZzTimeline* timeline)
{
    timeline->clearEvents();
    timeline->addEvent(timelineSampleTime().addSecs(-2400), zzGaugeText("创建任务"),
        zzGaugeText("任务已添加到计划中。"), ZzTimelineEvent::Completed);
    timeline->addEvent(timelineSampleTime().addSecs(-1200), zzGaugeText("下载资源"),
        zzGaugeText("所需资源已经准备完成。"), ZzTimelineEvent::Completed);
    timeline->addEvent(timelineSampleTime().addSecs(-300), zzGaugeText("处理数据"),
        zzGaugeText("当前正在生成结果。"), ZzTimelineEvent::Current);
    timeline->addEvent(timelineSampleTime().addSecs(900), zzGaugeText("等待确认"),
        zzGaugeText("处理完成后需要人工确认。"), ZzTimelineEvent::Pending);
}

namespace {
    QColor eventColor(const ZzTimeline* timeline, const ZzTimelineEvent* event)
    {
        if (event && event->color().isValid())
            return event->color();
        if (event) {
            switch (event->status()) {
            case ZzTimelineEvent::Completed:
                return QColor(QStringLiteral("#107C10"));
            case ZzTimelineEvent::Warning:
                return QColor(QStringLiteral("#F2A900"));
            case ZzTimelineEvent::Error:
                return QColor(QStringLiteral("#D13438"));
            case ZzTimelineEvent::Pending: {
                // An event's default color follows the theme rail, not an explicit line
                // override.
                const auto palette = timeline->palette();
                const auto base = palette.color(QPalette::Active, QPalette::Base);
                const auto text = palette.color(QPalette::Active, QPalette::Text);
                return QColor(qRound(base.red() * 0.78 + text.red() * 0.22),
                    qRound(base.green() * 0.78 + text.green() * 0.22),
                    qRound(base.blue() * 0.78 + text.blue() * 0.22));
            }
            default:
                break;
            }
        }
        return timeline->palette().color(QPalette::Active, QPalette::Accent);
    }
} // namespace

void buildTimelineEventEditor(QFormLayout* form, ZzTimeline* timeline)
{
    auto* page = form->parentWidget();
    auto* selector = new QComboBox(page);
    selector->setObjectName(QStringLiteral("zzTimelineEventSelector"));
    const auto edit = [page](const char* name) {
        auto* result = new QLineEdit(page);
        result->setObjectName(QString::fromLatin1(name));
        return result;
    };
    auto* time = edit("zzTimelineEventTime");
    auto* title = edit("zzTimelineEventTitle");
    auto* description = edit("zzTimelineEventDescription");
    auto* icon = edit("zzTimelineEventIcon");
    icon->setPlaceholderText(zzGaugeText("可选 Fluent 字体图标字符"));
    auto* status = new QComboBox(page);
    status->setObjectName(QStringLiteral("zzTimelineEventStatus"));
    status->addItem(zzGaugeText("普通"), ZzTimelineEvent::Normal);
    status->addItem(zzGaugeText("已完成"), ZzTimelineEvent::Completed);
    status->addItem(zzGaugeText("当前"), ZzTimelineEvent::Current);
    status->addItem(zzGaugeText("等待"), ZzTimelineEvent::Pending);
    status->addItem(zzGaugeText("警告"), ZzTimelineEvent::Warning);
    status->addItem(zzGaugeText("错误"), ZzTimelineEvent::Error);
    auto* placement = new QComboBox(page);
    placement->setObjectName(QStringLiteral("zzTimelineEventPlacement"));
    placement->addItem(zzGaugeText("自动"), ZzTimelineEvent::Automatic);
    placement->addItem(zzGaugeText("左侧 / 上方"), ZzTimelineEvent::LeftSide);
    placement->addItem(zzGaugeText("右侧 / 下方"), ZzTimelineEvent::RightSide);
    auto* color = new ZzExampleColorButton(page);
    color->setObjectName(QStringLiteral("zzTimelineEventColor"));
    auto* automatic = new QCheckBox(zzGaugeText("自动"), page);
    automatic->setObjectName(QStringLiteral("zzTimelineEventAutoColor"));
    auto* colorRow = new QHBoxLayout;
    colorRow->addWidget(color);
    colorRow->addWidget(automatic);
    colorRow->addStretch();
    auto* add = new QPushButton(zzGaugeText("添加事件"), page);
    add->setObjectName(QStringLiteral("zzTimelineAddEvent"));
    auto* remove = new QPushButton(zzGaugeText("删除当前事件"), page);
    remove->setObjectName(QStringLiteral("zzTimelineRemoveEvent"));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(remove);
    form->addRow(zzGaugeText("当前事件"), selector);
    form->addRow(zzGaugeText("时间文字"), time);
    form->addRow(zzGaugeText("标题"), title);
    form->addRow(zzGaugeText("描述"), description);
    form->addRow(zzGaugeText("状态"), status);
    form->addRow(zzGaugeText("交错内容位置"), placement);
    form->addRow(zzGaugeText("字体图标"), icon);
    form->addRow(zzGaugeText("节点颜色"), colorRow);
    form->addRow(buttons);

    auto selected = std::make_shared<QPointer<ZzTimelineEvent>>();
    const auto load = [=] {
        auto* event = selected->data();
        for (QWidget* widget :
            QList<QWidget*> { time, title, description, icon, status, placement, automatic, remove })
            widget->setEnabled(event != nullptr);
        const QSignalBlocker a(time), b(title), c(description), d(icon), e(status), f(placement), g(color),
            h(automatic);
        time->setText(event ? event->timeText() : QString());
        time->setPlaceholderText(
            event ? event->timestamp().toString(timeline->timestampFormat()) : QString());
        title->setText(event ? event->title() : QString());
        description->setText(event ? event->description() : QString());
        icon->setText(event ? event->icon() : QString());
        status->setCurrentIndex(event ? status->findData(event->status()) : -1);
        placement->setCurrentIndex(event ? placement->findData(event->placement()) : -1);
        automatic->setChecked(!event || !event->color().isValid());
        color->setEnabled(event && event->color().isValid());
        color->setSelectedColor(eventColor(timeline, event));
    };
    const auto sync = [=] {
        const auto events = timeline->events();
        int index = static_cast<int>(events.indexOf(selected->data()));
        if (index < 0 && !events.isEmpty())
            index = qBound(0, selector->currentIndex(), int(events.size()) - 1);
        const QSignalBlocker blocker(selector);
        selector->clear();
        for (auto* event : events)
            selector->addItem(event->title());
        selector->setCurrentIndex(index);
        *selected = index >= 0 ? events.at(index) : nullptr;
        load();
    };
    color->themeColor = [=] { return eventColor(timeline, selected->data()); };
    QObject::connect(timeline, &ZzTimeline::eventsChanged, page, sync);
    QObject::connect(timeline, &ZzTimeline::timestampFormatChanged, page, load);
    QObject::connect(selector, &QComboBox::currentIndexChanged, page, [=](int index) {
        const auto events = timeline->events();
        *selected = index >= 0 && index < events.size() ? events.at(index) : nullptr;
        load();
    });
    QObject::connect(time, &QLineEdit::textChanged, timeline, [=](const QString& text) {
        if (*selected)
            (*selected)->setTimeText(text);
    });
    QObject::connect(title, &QLineEdit::textChanged, timeline, [=](const QString& text) {
        if (*selected)
            (*selected)->setTitle(text);
    });
    QObject::connect(description, &QLineEdit::textChanged, timeline, [=](const QString& text) {
        if (*selected)
            (*selected)->setDescription(text);
    });
    QObject::connect(icon, &QLineEdit::textChanged, timeline, [=](const QString& text) {
        if (*selected)
            (*selected)->setIcon(text);
    });
    QObject::connect(status, &QComboBox::currentIndexChanged, timeline, [=](int) {
        if (*selected)
            (*selected)->setStatus(static_cast<ZzTimelineEvent::Status>(status->currentData().toInt()));
    });
    QObject::connect(placement, &QComboBox::currentIndexChanged, timeline, [=](int) {
        if (*selected)
            (*selected)->setPlacement(
                static_cast<ZzTimelineEvent::Placement>(placement->currentData().toInt()));
    });
    QObject::connect(color, &ZzExampleColorButton::selectedColorChanged, timeline, [=](const QColor& value) {
        if (*selected && !automatic->isChecked())
            (*selected)->setColor(value);
    });
    QObject::connect(automatic, &QCheckBox::toggled, timeline, [=](bool checked) {
        if (*selected)
            (*selected)->setColor(checked ? QColor() : color->selectedColor());
    });
    QObject::connect(add, &QPushButton::clicked, timeline, [=] {
        auto* event = timeline->addEvent(
            timelineSampleTime(), zzGaugeText("新事件"), zzGaugeText("可以在右侧修改事件内容。"));
        selector->setCurrentIndex(static_cast<int>(timeline->events().indexOf(event)));
    });
    QObject::connect(remove, &QPushButton::clicked, timeline, [=] {
        if (*selected)
            timeline->removeEvent(selected->data());
    });
    QObject::connect(timeline, &ZzTimeline::eventClicked, selector,
        [=](ZzTimelineEvent* event) { selector->setCurrentIndex(static_cast<int>(timeline->events().indexOf(event))); });
    sync();
}
} // namespace ZzExample
