#include "ZzPageTransitionPrivate.h"
#include "ZzStackTransitionOverlay.h"
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QLayout>

namespace ZzFluentUI {

/** @brief 在同步回调销毁外层时短暂保活状态，不延长任何页面寿命。 */
class ZzPageTransitionPrivate::ZzPrivate final : public QObject
{
public:
    QPointer<QWidget> owner;
    QPointer<ZzStackTransitionOverlay> overlay;
    QPointer<QVariantAnimation> animation;
    Settings settings;
    QSize pixels;
    QImage source;
    QImage target;
    Token generation = 0;
    bool alive = true;
    bool playing = false;
    bool capturing = false;
    bool clearing = false;
    bool observed = false;
    std::function<void()> finished;
    std::function<void(bool)> observer;

    /** @brief 核对当前操作仍属于同一轮有效捕获。 */
    bool valid(Token token) const { return alive && owner && token != 0 && token == generation; }
    /** @brief 关闭计时区间前先清标志，允许观察者再次取消。 */
    void endObservation()
    {
        if (!observed) return;
        observed = false;
        const auto callback = observer;
        if (callback) callback(false);
    }
    /** @brief 失效代次和图像；不发送播放完成通知。 */
    void clear()
    {
        if (clearing) return;
        clearing = true;
        ++generation;
        playing = false;
        source = {};
        target = {};
        if (animation) animation->stop();
        if (overlay) { overlay->clear(); overlay->hide(); }
        endObservation();
        clearing = false;
    }
    /** @brief 捕获局部图像，渲染回调只能取消而不能并行准备另一段。 */
    QImage capture(Token token, QWidget *page)
    {
        if (!valid(token) || capturing || !ZzPageTransitionPrivate::canCapture(page)) return {};
        const QPointer<QWidget> view(page);
        capturing = true;
        page->ensurePolished();
        if (valid(token) && view) view->setGeometry(QRect(QPoint(), settings.contentRect.size()));
        if (valid(token) && view && view->layout()) view->layout()->activate();
        QImage result;
        if (valid(token) && view) {
            result = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
            if (!result.isNull()) {
                result.setDevicePixelRatio(settings.dpr);
                result.fill(settings.background);
                QPainter painter(&result);
                view->render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
            }
        }
        capturing = false;
        if (!valid(token) || !view) return {};
        return result;
    }
};

ZzPageTransitionPrivate::ZzPageTransitionPrivate(QWidget *owner) : d(std::make_shared<ZzPrivate>())
{
    d->owner = owner;
    d->overlay = new ZzStackTransitionOverlay(owner);
    d->animation = new QVariantAnimation(owner);
    d->animation->setEasingCurve(QEasingCurve::OutCubic);
    d->animation->setStartValue(0.0);
    d->animation->setEndValue(1.0);
    const std::weak_ptr<ZzPrivate> weak(d);
    QObject::connect(owner, &QObject::destroyed, d.get(), [weak] {
        if (const auto state = weak.lock()) state->clear();
    });
    QObject::connect(d->animation, &QVariantAnimation::valueChanged, d.get(), [weak](const QVariant &value) {
        if (const auto state = weak.lock(); state && state->overlay) state->overlay->setProgress(value.toReal());
    });
    QObject::connect(d->animation, &QVariantAnimation::finished, d.get(), [weak] {
        const auto state = weak.lock();
        if (!state || !state->alive || !state->playing) return;
        const auto callback = state->finished;
        state->clear();
        if (state->alive && state->owner && callback) callback();
    });
}

ZzPageTransitionPrivate::~ZzPageTransitionPrivate()
{
    const auto state = d;
    state->alive = false;
    state->finished = {};
    state->clear();
    state->observer = {};
    delete state->animation.data();
    delete state->overlay.data();
}
void ZzPageTransitionPrivate::setFinishedCallback(std::function<void()> callback) { d->finished = std::move(callback); }
void ZzPageTransitionPrivate::setCaptureObserver(std::function<void(bool)> callback) { d->observer = std::move(callback); }

ZzPageTransitionPrivate::Token ZzPageTransitionPrivate::beginCapture(const Settings &settings)
{
    const auto state = d;
    if (state->capturing || state->clearing) return 0;
    state->clear();
    if (!state->alive || !state->owner) return 0;
    const Token token = state->generation;
    state->settings = settings;
    state->observed = true;
    const auto callback = state->observer;
    if (callback) callback(true);
    if (!state->valid(token)) return 0;
    state->pixels = ZzStackTransitionOverlay::boundedPixelSize(settings.contentRect.size(), settings.dpr);
    if (state->pixels.isEmpty() || settings.duration <= 0 || settings.mode <= Mode::None || settings.mode > Mode::Fade) {
        state->clear();
        return 0;
    }
    return token;
}

bool ZzPageTransitionPrivate::captureSource(Token token, QWidget *view)
{
    const auto state = d;
    if (!state->valid(token) || !state->source.isNull() || state->playing || state->capturing) return false;
    QImage frame = state->capture(token, view);
    if (!state->valid(token)) return false;
    if (frame.isNull()) { state->clear(); return false; }
    state->source = std::move(frame);
    return true;
}

bool ZzPageTransitionPrivate::captureTarget(Token token, QWidget *view)
{
    const auto state = d;
    if (!state->valid(token) || state->source.isNull() || !state->target.isNull() || state->playing || state->capturing) return false;
    QImage frame = state->capture(token, view);
    if (!state->valid(token)) return false;
    if (frame.isNull()) { state->clear(); return false; }
    state->target = std::move(frame);
    state->endObservation();
    return state->valid(token);
}

bool ZzPageTransitionPrivate::present(Token token)
{
    const auto state = d;
    if (!state->valid(token) || state->source.isNull() || state->target.isNull() || !state->overlay || !state->animation) return false;
    state->overlay->setGeometry(state->settings.contentRect);
    if (!state->valid(token)) return false;
    state->overlay->setFrames(std::move(state->source), std::move(state->target),
        static_cast<ZzStackTransitionOverlay::Mode>(state->settings.mode), state->settings.reverse);
    state->playing = true;
    state->overlay->show();
    if (!state->valid(token)) return false;
    state->overlay->raise();
    return state->valid(token) && state->playing;
}

bool ZzPageTransitionPrivate::play(Token token)
{
    const auto state = d;
    if (!state->valid(token)) return false;
    if (!state->playing && !present(token)) return false;
    if (!state->valid(token)) return false;
    state->animation->setDuration(state->settings.duration);
    state->animation->start();
    return state->valid(token) && state->playing;
}
void ZzPageTransitionPrivate::cancel() { const auto state = d; state->clear(); }
bool ZzPageTransitionPrivate::running() const noexcept { return d->playing && d->owner; }
qsizetype ZzPageTransitionPrivate::retainedBytes() const noexcept
{
    return d->source.sizeInBytes() + d->target.sizeInBytes() + (d->overlay ? d->overlay->retainedBytes() : 0);
}
bool ZzPageTransitionPrivate::canCapture(QWidget *view)
{
    if (!view) return false;
    auto widgets = view->findChildren<QWidget *>();
    widgets.prepend(view);
    for (auto *child : widgets) {
        if (child->testAttribute(Qt::WA_NativeWindow) || child->inherits("QOpenGLWidget")
            || child->inherits("QQuickWidget") || child->inherits("QVideoWidget")
            || child->inherits("QWindowContainer")) return false;
    }
    return true;
}
} // namespace ZzFluentUI
