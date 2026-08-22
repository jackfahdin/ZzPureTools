#include "ZzBottomPanePrivate.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtGui/QPixmap>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPivot.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int zzBottomPaneHandleExtent = 4;

} // namespace

ZzBottomPanePrivate::ZzBottomPanePrivate(ZzBottomPane *publicObject)
    : q_ptr(publicObject)
    , theme(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    resizeHandle = new QWidget(q_ptr);
    resizeHandle->setObjectName(QStringLiteral("zzBottomPaneResizeHandle"));
    resizeHandle->setFixedHeight(zzBottomPaneHandleExtent);
    resizeHandle->setCursor(Qt::SizeVerCursor);
    resizeHandle->setAccessibleName(
        ZzBottomPane::tr("调整底部工具区高度"));
    resizeHandle->setToolTip(ZzBottomPane::tr("调整底部工具区高度"));
    resizeHandle->installEventFilter(q_ptr);

    header = new QWidget(q_ptr);
    header->setObjectName(QStringLiteral("zzBottomPaneHeader"));
    header->setAccessibleName(ZzBottomPane::tr("底部工具区标题栏"));
    pivot = new ZzPivot(header);
    pivot->setObjectName(QStringLiteral("zzBottomPanePivot"));
    pivot->setAccessibleName(ZzBottomPane::tr("底部工具"));
    closeButton = new ZzIconButton(header);
    closeButton->setObjectName(QStringLiteral("zzBottomPaneCloseButton"));
    closeButton->setAccessibleName(ZzBottomPane::tr("关闭当前工具"));
    closeButton->setToolTip(ZzBottomPane::tr("关闭当前工具"));
    qobject_cast<ZzIconButton *>(closeButton)->setIconDescriptor(
        ZzIconDescriptor::fromBundledSvg(ZzBundledSvgIcon::Close));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    headerLayout->addWidget(pivot, 1);
    headerLayout->addWidget(closeButton);

    stackedWidget = new QStackedWidget(q_ptr);
    stackedWidget->setObjectName(QStringLiteral("zzBottomPaneStack"));

    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(resizeHandle);
    layout->addWidget(header);
    layout->addWidget(stackedWidget, 1);

    const std::shared_ptr<const ZzThemeSnapshot> snapshot = theme.snapshot();
    const int headerHeight = qRound(
        snapshot->metric(ZzMetricToken::BottomPaneHeaderHeight));
    header->setFixedHeight(headerHeight);
    closeButton->setFixedSize(headerHeight, headerHeight);
    applyExpandedHeight();

    QObject::connect(
        pivot, &QTabBar::currentChanged,
        q_ptr, [this](int index) {
            if (index >= 0 && stackedWidget->currentIndex() != index) {
                stackedWidget->setCurrentIndex(index);
            }
        });
    QObject::connect(
        stackedWidget, &QStackedWidget::currentChanged,
        q_ptr, [this](int index) {
            if (index >= 0 && pivot->currentIndex() != index) {
                pivot->setCurrentIndex(index);
            }
            syncCurrentWidget();
        });
    QObject::connect(
        stackedWidget, &QStackedWidget::widgetRemoved,
        q_ptr, [this](int index) { removeWidgetAt(index); });
    QObject::connect(
        closeButton, &QToolButton::clicked,
        q_ptr, [this] {
            if (QWidget *const current = stackedWidget->currentWidget();
                current != nullptr) {
                Q_EMIT q_ptr->widgetCloseRequested(current);
            }
        });
}

ZzBottomPanePrivate::~ZzBottomPanePrivate() = default;

bool ZzBottomPanePrivate::addWidget(
    QWidget *widget,
    const QString &title,
    const ZzIconDescriptor &icon)
{
    if (widget == nullptr || widget->parent() != nullptr || widgets.contains(widget)) {
        return false;
    }
    QPointer<ZzBottomPane> paneGuard(q_ptr);
    QPointer<QWidget> widgetGuard(widget);
    const int index = stackedWidget->addWidget(widget);
    if (index < 0 || paneGuard.isNull() || widgetGuard.isNull()) {
        return false;
    }
    widgets.insert(index, widgetGuard.data());
    pivot->addTab(pivotIcon(icon), title);
    if (paneGuard.isNull() || widgetGuard.isNull()
        || pivot->count() != widgets.size()) {
        return false;
    }
    return setCurrentWidget(widgetGuard.data());
}

QWidget *ZzBottomPanePrivate::takeWidget(QWidget *widget)
{
    const int index = static_cast<int>(widgets.indexOf(widget));
    if (index < 0 || stackedWidget->indexOf(widget) != index) {
        return nullptr;
    }
    QPointer<ZzBottomPane> paneGuard(q_ptr);
    QPointer<QWidget> widgetGuard(widget);
    stackedWidget->removeWidget(widget);
    if (paneGuard.isNull() || widgetGuard.isNull()) {
        return nullptr;
    }
    widget->setParent(nullptr);
    return widget;
}

bool ZzBottomPanePrivate::setCurrentWidget(QWidget *widget)
{
    const int index = static_cast<int>(widgets.indexOf(widget));
    if (index < 0 || stackedWidget->indexOf(widget) != index) {
        return false;
    }
    QPointer<ZzBottomPane> paneGuard(q_ptr);
    stackedWidget->setCurrentIndex(index);
    return !paneGuard.isNull() && stackedWidget->currentWidget() == widget;
}

void ZzBottomPanePrivate::removeWidgetAt(int index)
{
    if (index < 0 || index >= widgets.size()) {
        return;
    }
    widgets.removeAt(index);
    QPointer<ZzBottomPane> paneGuard(q_ptr);
    pivot->removeTab(index);
    if (paneGuard.isNull()) {
        return;
    }
    syncCurrentWidget();
}

void ZzBottomPanePrivate::syncCurrentWidget()
{
    QWidget *const current = stackedWidget->currentWidget();
    if (lastNotifiedCurrent.data() == current) {
        return;
    }
    lastNotifiedCurrent = current;
    QPointer<ZzBottomPane> paneGuard(q_ptr);
    Q_EMIT q_ptr->currentWidgetChanged(current);
    if (paneGuard.isNull()) {
        return;
    }
}

int ZzBottomPanePrivate::clampHeight(int height) const noexcept
{
    return std::clamp(height, minimumHeight, maximumHeight);
}

void ZzBottomPanePrivate::applyExpandedHeight()
{
    q_ptr->setFixedHeight(expandedHeight);
}

bool ZzBottomPanePrivate::handleResizeDrag(
    int globalY,
    bool begin,
    bool finish)
{
    if (begin) {
        resizing = true;
        resizeStartGlobalY = globalY;
        resizeStartHeight = expandedHeight;
        return true;
    }
    if (!resizing) {
        return false;
    }
    QPointer<ZzBottomPane> paneGuard(q_ptr);
    q_ptr->setPaneHeight(resizeStartHeight + resizeStartGlobalY - globalY);
    if (paneGuard.isNull()) {
        return true;
    }
    if (finish) {
        resizing = false;
    }
    return true;
}

QIcon ZzBottomPanePrivate::pivotIcon(
    const ZzIconDescriptor &descriptor) const
{
    if (descriptor.source == ZzIconSource::SvgResource
        && !descriptor.resourceId.isEmpty()) {
        return QIcon(descriptor.resourceId);
    }
    auto *const fluentStyle = qobject_cast<ZzFluentStyle *>(q_ptr->style());
    if (fluentStyle == nullptr) {
        return {};
    }
    const std::shared_ptr<const ZzThemeSnapshot> snapshot = theme.snapshot();
    const int extent = qRound(snapshot->metric(ZzMetricToken::IconSmall));
    const QPixmap pixmap = fluentStyle->iconPixmap(
        descriptor,
        QSize(extent, extent),
        q_ptr->devicePixelRatioF(),
        q_ptr->palette().color(QPalette::ButtonText),
        q_ptr->layoutDirection());
    return pixmap.isNull() ? QIcon() : QIcon(pixmap);
}

} // namespace ZzFluentUI
