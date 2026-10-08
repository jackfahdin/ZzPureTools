#include "ZzExampleTimelineSmoke.h"
#include "ZzExampleTimelineHelpers.h"

#include <QApplication>
#include <QMouseEvent>
#include <ZzFluentUI/ZzThemeController.h>

namespace ZzExample {
bool zzTimelinePageReady(const QWidget& window, ZzFluentUI::ZzThemeController* theme)
{
    auto* page = window.findChild<QWidget*>(QStringLiteral("zzExampleTimelinePage"));
    if (!page)
        return false;
    auto* timeline = page->findChild<ZzTimeline*>(QStringLiteral("zzTimeline_previewTimeline"));
    auto* selector = page->findChild<QComboBox*>(QStringLiteral("zzTimelineEventSelector"));
    auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("zzTimeline_editorTabs"));
    if (!timeline || !selector || !tabs || tabs->count() != 3 || page->findChildren<ZzTimeline*>().size() != 5
        || timeline->events().size() != 4)
        return false;
    const auto widget
        = [page]<typename T>(const char* name) { return page->findChild<T*>(QString::fromLatin1(name)); };
    auto* orientation = widget.operator()<QComboBox>("zzTimeline_orientationCombo");
    auto* layout = widget.operator()<QComboBox>("zzTimeline_layoutModeCombo");
    auto* reverse = widget.operator()<QCheckBox>("zzTimeline_reverseCheck");
    auto* title = widget.operator()<QLineEdit>("zzTimelineEventTitle");
    auto* time = widget.operator()<QLineEdit>("zzTimelineEventTime");
    auto* description = widget.operator()<QLineEdit>("zzTimelineEventDescription");
    auto* status = widget.operator()<QComboBox>("zzTimelineEventStatus");
    auto* placement = widget.operator()<QComboBox>("zzTimelineEventPlacement");
    auto* icon = widget.operator()<QLineEdit>("zzTimelineEventIcon");
    auto* color = widget.operator()<ZzExampleColorButton>("zzTimelineEventColor");
    auto* automatic = widget.operator()<QCheckBox>("zzTimelineEventAutoColor");
    auto* lineColor = widget.operator()<ZzExampleColorButton>("zzTimeline_lineColorButton");
    auto* autoLine = widget.operator()<QCheckBox>("zzTimelineAutoLineColor");
    auto* add = widget.operator()<QPushButton>("zzTimelineAddEvent");
    auto* remove = widget.operator()<QPushButton>("zzTimelineRemoveEvent");
    auto* reset = widget.operator()<QPushButton>("zzTimelineReset");
    if (!orientation || !layout || !reverse || !title || !time || !description || !status || !placement
        || !icon || !color || !automatic || !lineColor || !autoLine || !add || !remove || !reset)
        return false;
    selector->setCurrentIndex(2);
    auto* edited = timeline->events().at(2);
    title->setText(QStringLiteral("Edited"));
    time->setText(QStringLiteral("Now"));
    description->setText(QStringLiteral("Details"));
    status->setCurrentIndex(status->findData(ZzTimelineEvent::Warning));
    placement->setCurrentIndex(placement->findData(ZzTimelineEvent::LeftSide));
    icon->setText(QString(QChar(0xe713)));
    bool ready = edited->title() == QStringLiteral("Edited") && selector->currentText() == edited->title()
        && edited->timeText() == QStringLiteral("Now") && edited->description() == QStringLiteral("Details")
        && edited->status() == ZzTimelineEvent::Warning && edited->placement() == ZzTimelineEvent::LeftSide
        && edited->icon() == QString(QChar(0xe713));
    automatic->setChecked(false);
    color->setSelectedColor(Qt::red);
    autoLine->setChecked(false);
    lineColor->setSelectedColor(Qt::green);
    const auto oldTheme = theme->mode();
    theme->setMode(ZzFluentUI::ZzThemeMode::Dark);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
    ready = ready && edited->color() == QColor(Qt::red) && color->selectedColor() == QColor(Qt::red)
        && timeline->lineColor() == QColor(Qt::green);
    automatic->setChecked(true);
    autoLine->setChecked(true);
    ready = ready && !edited->color().isValid() && !timeline->lineColor().isValid();
    theme->setMode(oldTheme);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::ApplicationPaletteChange);
    reverse->setChecked(true);
    timeline->doItemsLayout();
    const auto index = timeline->model()->index(0, 0);
    timeline->scrollTo(index);
    const QPointF point(timeline->visualRect(index).center());
    QMouseEvent press(QEvent::MouseButtonPress, point, timeline->viewport()->mapToGlobal(point.toPoint()),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, timeline->viewport()->mapToGlobal(point.toPoint()),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(timeline->viewport(), &press);
    QApplication::sendEvent(timeline->viewport(), &release);
    ready = ready && selector->currentIndex() == 3;
    orientation->setCurrentIndex(orientation->findData(Qt::Horizontal));
    layout->setCurrentIndex(layout->findData(ZzTimeline::AlternatingReverse));
    ready = ready && timeline->orientation() == Qt::Horizontal
        && timeline->layoutMode() == ZzTimeline::AlternatingReverse;
    const auto setSlider = [&](const char* name, int value) {
        auto* slider = widget.operator()<QSlider>(name);
        if (!slider) {
            ready = false;
            return;
        }
        slider->setValue(value);
    };
    setSlider("zzTimeline_nodeSizeSlider", 22);
    setSlider("zzTimeline_lineWidthSlider", 7);
    setSlider("zzTimeline_horizontalItemWidthSlider", 280);
    setSlider("zzTimeline_animationDurationSlider", 2200);
    setSlider("zzTimeline_titleFontSizeSlider", 20);
    ready = ready && timeline->nodeSize() == 22 && qFuzzyCompare(timeline->lineWidth(), 3.5)
        && timeline->horizontalItemWidth() == 280 && timeline->animationDuration() == 2200
        && timeline->titleFontPixelSize() == 20;
    for (int count = 0; count < 4; ++count)
        remove->click();
    ready = ready && timeline->events().isEmpty() && selector->count() == 0 && !remove->isEnabled()
        && !title->isEnabled();
    add->click();
    ready = ready && timeline->events().size() == 1 && selector->currentIndex() == 0 && title->isEnabled();
    title->setText(QStringLiteral("Recreated"));
    ready = ready && timeline->eventAt(0)->title() == QStringLiteral("Recreated");
    reset->click();
    return ready && timeline->events().size() == 4 && selector->count() == 4
        && timeline->orientation() == Qt::Vertical && timeline->layoutMode() == ZzTimeline::Alternating
        && !timeline->isReverse() && timeline->nodeSize() == 14 && qFuzzyCompare(timeline->lineWidth(), 2.0)
        && timeline->titleFontPixelSize() == 0 && timeline->animationDuration() == 1400
        && !timeline->lineColor().isValid() && timeline->isEnabled();
}
} // namespace ZzExample
