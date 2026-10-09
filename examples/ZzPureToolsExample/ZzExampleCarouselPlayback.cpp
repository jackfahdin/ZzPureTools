#include "ZzExampleCarouselPlayback.h"

#include <QApplication>
#include <QEvent>
#include <QStyle>
#include <algorithm>
#include <limits>

namespace ZzExample {
ZzExampleCarouselPlayback::ZzExampleCarouselPlayback(
    ZzFluentUI::ZzCarouselView* view, QObject* parent)
    : QObject(parent)
    , view_(view)
    , timer_(this)
{
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (!canPlay()) {
            restart();
            return;
        }
        QPointer<ZzExampleCarouselPlayback> self(this);
        view_->showNext();
        if (self)
            restart();
    });
    if (!view)
        return;
    view->installEventFilter(this);
    connect(view, &ZzFluentUI::ZzCarouselView::currentRowChanged, this, [this] { restart(); });
    connect(
        view, &ZzFluentUI::ZzCarouselView::wrapAroundEnabledChanged, this, [this] { restart(); });
    connect(
        view, &ZzFluentUI::ZzCarouselView::animationDurationChanged, this, [this] { restart(); });
    connect(view, &QObject::destroyed, this, [this] { timer_.stop(); });
    if (auto* model = view->model()) {
        connect(model, &QAbstractItemModel::rowsInserted, this, [this] { restart(); });
        connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { restart(); });
        connect(model, &QAbstractItemModel::modelReset, this, [this] { restart(); });
        connect(model, &QObject::destroyed, this, [this] { timer_.stop(); });
    }
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* current) {
        if (view_
            && ((old && (old == view_ || view_->isAncestorOf(old)))
                || (current && (current == view_ || view_->isAncestorOf(current)))))
            restart();
    });
}

void ZzExampleCarouselPlayback::setEnabled(bool enabled)
{
    if (enabled_ == enabled)
        return;
    enabled_ = enabled;
    restart();
}
void ZzExampleCarouselPlayback::setInterval(int milliseconds)
{
    const int bounded = std::max(100, milliseconds);
    if (interval_ == bounded)
        return;
    interval_ = bounded;
    restart();
}
void ZzExampleCarouselPlayback::setPauseOnHover(bool enabled)
{
    if (pauseOnHover_ == enabled)
        return;
    pauseOnHover_ = enabled;
    restart();
}

bool ZzExampleCarouselPlayback::canPlay() const
{
    if (!enabled_ || !view_ || !view_->isVisible() || !view_->isEnabled() || !view_->model()
        || (pauseOnHover_ && hovered_)
        || !view_->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, view_))
        return false;
    auto* focus = QApplication::focusWidget();
    if (focus && (focus == view_ || view_->isAncestorOf(focus)))
        return false;
    const int count = view_->model()->rowCount(view_->rootIndex());
    return count > 1 && view_->currentRow() >= 0
        && (view_->isWrapAroundEnabled() || view_->currentRow() < count - 1);
}

void ZzExampleCarouselPlayback::restart()
{
    timer_.stop();
    if (!canPlay())
        return;
    const qint64 delay = qint64(interval_) + view_->animationDuration();
    timer_.start(int(std::min<qint64>(delay, std::numeric_limits<int>::max())));
}

bool ZzExampleCarouselPlayback::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == view_) {
        switch (event->type()) {
        case QEvent::Enter:
            hovered_ = true;
            if (pauseOnHover_)
                restart();
            break;
        case QEvent::Leave:
            hovered_ = false;
            if (pauseOnHover_)
                restart();
            break;
        case QEvent::Hide:
            hovered_ = false;
            timer_.stop();
            break;
        case QEvent::Show:
        case QEvent::EnabledChange:
        case QEvent::StyleChange:
        case QEvent::DynamicPropertyChange:
            restart();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}
} // namespace ZzExample
