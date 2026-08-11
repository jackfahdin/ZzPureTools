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

    QObject::connect(
        revealButton,
        &ZzIconButton::pressed,
        q_ptr,
        [this] {
            beginPeek();
        });
    QObject::connect(
        revealButton,
        &ZzIconButton::released,
        q_ptr,
        [this] {
            endPeek();
        });
    QObject::connect(
        q_ptr,
        &QLineEdit::textChanged,
        q_ptr,
        [this] {
            if (q_ptr->text().isEmpty()) {
                endPeek();
            }
            syncButtonGeometry();
        });
    QObject::connect(
        qApp,
        &QApplication::focusChanged,
        q_ptr,
        [this](QWidget *, QWidget *current) {
            if (current != q_ptr && current != revealButton) {
                endPeek();
            }
        });
    QObject::connect(
        qApp,
        &QApplication::applicationStateChanged,
        q_ptr,
        [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive) {
                endPeek();
            }
        });

    applyVisibility(false);
    refreshPresentation();
}

void ZzPasswordBoxPrivate::refreshPresentation()
{
    const bool visible = isPasswordVisible();
    revealButton->setIconDescriptor(ZzIconDescriptor::fromFontIcon(
        visible ? ZzFontIcon::EyeSlash : ZzFontIcon::Eye));
    revealButton->setAccessibleName(ZzPasswordBox::tr("显示密码"));
    revealButton->setAccessibleDescription(
        ZzPasswordBox::tr("按住时临时显示密码"));
    revealButton->setToolTip(ZzPasswordBox::tr("按住显示密码"));
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
    peekActive = false;
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
    peekActive = true;
    applyVisibility(wasVisible);
    refreshPresentation();
}

void ZzPasswordBoxPrivate::endPeek()
{
    if (!peekActive) {
        return;
    }
    const bool wasVisible = isPasswordVisible();
    peekActive = false;
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
        || (revealMode == ZzPasswordRevealMode::Peek && peekActive);
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
    return revealMode == ZzPasswordRevealMode::Peek
        && !q_ptr->text().isEmpty() && q_ptr->isEnabled();
}

} // namespace ZzFluentUI
