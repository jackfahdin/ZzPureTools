#include "ZzPasswordBoxPrivate.h"

#include <algorithm>

#include <QtCore/QtMath>
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPasswordBox.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int zzPasswordButtonInsetDivisor = 4;
constexpr int zzPasswordButtonMinimumInset = 2;

} // namespace

ZzPasswordBoxPrivate::ZzPasswordBoxPrivate(ZzPasswordBox *q)
    : q_ptr(q)
    , theme(q)
    , revealButton(new ZzIconButton(q))
    , baseTextMargins(q->textMargins())
{
    Q_ASSERT(q_ptr != nullptr);
    revealButton->setObjectName(
        QStringLiteral("zzPasswordRevealButton"));
    revealButton->setFocusPolicy(Qt::StrongFocus);
    revealButton->setAutoRaise(true);

    callbackConnections[0] = QObject::connect(
        revealButton,
        &ZzIconButton::pressed,
        q_ptr,
        [this] {
            beginPeek();
        });
    callbackConnections[1] = QObject::connect(
        revealButton,
        &ZzIconButton::released,
        q_ptr,
        [this] {
            if (revealMode == ZzPasswordRevealMode::Peek) {
                endReveal();
            }
        });
    callbackConnections[2] = QObject::connect(
        q_ptr,
        &QLineEdit::textChanged,
        q_ptr,
        [this] {
            if (q_ptr->text().isEmpty()) {
                endReveal();
            }
            syncButtonGeometry();
        });
    callbackConnections[3] = QObject::connect(
        qApp,
        &QApplication::focusChanged,
        q_ptr,
        [this](QWidget *, QWidget *current) {
            if (revealMode == ZzPasswordRevealMode::Peek
                && current != q_ptr && current != revealButton) {
                endReveal();
            }
        });
    callbackConnections[4] = QObject::connect(
        qApp,
        &QApplication::applicationStateChanged,
        q_ptr,
        [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
                endReveal();
            }
        });
    callbackConnections[5] = QObject::connect(
        revealButton, &ZzIconButton::clicked, q_ptr, [this] {
            toggleVisibility();
        });

    applyVisibility(false);
    refreshPresentation();
}

ZzPasswordBoxPrivate::~ZzPasswordBoxPrivate()
{
    for (const QMetaObject::Connection &connection : callbackConnections) {
        QObject::disconnect(connection);
    }
}

void ZzPasswordBoxPrivate::refreshPresentation()
{
    const bool visible = isPasswordVisible();
    revealButton->setIconDescriptor(ZzIconDescriptor::fromFontIcon(
        visible ? ZzFontIcon::EyeSlash : ZzFontIcon::Eye));
    if (revealMode == ZzPasswordRevealMode::Toggle) {
        const QString action = visible ? ZzPasswordBox::tr("隐藏密码") : ZzPasswordBox::tr("显示密码");
        revealButton->setAccessibleName(action);
        revealButton->setAccessibleDescription(action);
        revealButton->setToolTip(action);
    } else {
        revealButton->setAccessibleName(ZzPasswordBox::tr("显示密码"));
        revealButton->setAccessibleDescription(ZzPasswordBox::tr("按住时临时显示密码"));
        revealButton->setToolTip(ZzPasswordBox::tr("按住显示密码"));
    }
    syncButtonGeometry();
}

void ZzPasswordBoxPrivate::refreshTheme()
{
    theme.refreshFallback();
    refreshPresentation();
}

void ZzPasswordBoxPrivate::setRevealMode(ZzPasswordRevealMode mode)
{
    if (revealMode == mode) {
        return;
    }
    const bool wasVisible = isPasswordVisible();
    revealMode = mode;
    revealActive = false;
    applyVisibility(wasVisible);
    refreshPresentation();
    Q_EMIT q_ptr->revealModeChanged(mode);
}

void ZzPasswordBoxPrivate::beginPeek()
{
    if (revealMode != ZzPasswordRevealMode::Peek
        || q_ptr->text().isEmpty() || !q_ptr->isEnabled()) {
        return;
    }
    const bool wasVisible = isPasswordVisible();
    revealActive = true;
    applyVisibility(wasVisible);
    refreshPresentation();
}

void ZzPasswordBoxPrivate::toggleVisibility()
{
    if (revealMode != ZzPasswordRevealMode::Toggle
        || q_ptr->text().isEmpty() || !q_ptr->isEnabled()) {
        return;
    }
    const bool wasVisible = isPasswordVisible();
    revealActive = !revealActive;
    applyVisibility(wasVisible);
    refreshPresentation();
}

void ZzPasswordBoxPrivate::endReveal()
{
    if (!revealActive) {
        return;
    }
    const bool wasVisible = isPasswordVisible();
    revealActive = false;
    applyVisibility(wasVisible);
    refreshPresentation();
}

void ZzPasswordBoxPrivate::syncButtonGeometry()
{
    const auto snapshot = theme.snapshot();
    const int controlHeight = qCeil(
        snapshot->metric(ZzMetricToken::ControlHeight));
    const int verticalPadding = qCeil(
        snapshot->metric(ZzMetricToken::VerticalPadding));
    const int horizontalPadding = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    const int minimumInset = std::max(
        zzPasswordButtonMinimumInset,
        verticalPadding / zzPasswordButtonInsetDivisor);
    const int availableHeight = std::max(1, q_ptr->height());
    const int buttonExtent = std::max(
        1,
        std::min(controlHeight - minimumInset, availableHeight));

    const bool showButton = shouldShowButton();
    if (!showButton && revealButton->hasFocus()) {
        q_ptr->setFocus(Qt::OtherFocusReason);
    }
    revealButton->setVisible(showButton);

    QMargins effectiveMargins = baseTextMargins;
    if (showButton) {
        const QRect contents = q_ptr->contentsRect();
        const int top = contents.top()
            + std::max(0, (contents.height() - buttonExtent) / 2);
        const int left = q_ptr->layoutDirection() == Qt::RightToLeft
            ? contents.left()
            : contents.right() - buttonExtent + 1;
        revealButton->setGeometry(
            QRect(left, top, buttonExtent, buttonExtent));
        revealButton->raise();
        const int reservation = buttonExtent + horizontalPadding;
        if (q_ptr->layoutDirection() == Qt::RightToLeft) {
            effectiveMargins.setLeft(
                baseTextMargins.left() + reservation);
        } else {
            effectiveMargins.setRight(
                baseTextMargins.right() + reservation);
        }
    }
    q_ptr->QLineEdit::setTextMargins(effectiveMargins);
}

bool ZzPasswordBoxPrivate::isPasswordVisible() const noexcept
{
    return revealMode == ZzPasswordRevealMode::Visible
        || ((revealMode == ZzPasswordRevealMode::Peek
                || revealMode == ZzPasswordRevealMode::Toggle) && revealActive);
}

void ZzPasswordBoxPrivate::applyVisibility(bool wasVisible)
{
    const bool visible = isPasswordVisible();
    q_ptr->QLineEdit::setEchoMode(
        visible ? QLineEdit::Normal : QLineEdit::Password);
    if (visible != wasVisible) {
        Q_EMIT q_ptr->passwordVisibilityChanged(visible);
    }
}

bool ZzPasswordBoxPrivate::shouldShowButton() const noexcept
{
    return (revealMode == ZzPasswordRevealMode::Peek || revealMode == ZzPasswordRevealMode::Toggle)
        && !q_ptr->text().isEmpty() && q_ptr->isEnabled();
}

} // namespace ZzFluentUI
