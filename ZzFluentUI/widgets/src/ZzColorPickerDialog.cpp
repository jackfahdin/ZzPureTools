#include <ZzFluentUI/ZzColorPickerDialog.h>

#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QPainter>

#include "private/ZzColorPickerDialogPrivate.h"

namespace ZzFluentUI {

ZzColorPickerDialog::ZzColorPickerDialog(QWidget *parent)
    : QDialog(parent)
    , d_ptr(std::make_unique<ZzColorPickerDialogPrivate>(this))
{
    resize(sizeHint());
}

ZzColorPickerDialog::~ZzColorPickerDialog() = default;

QString ZzColorPickerDialog::title() const
{
    return windowTitle();
}

void ZzColorPickerDialog::setTitle(const QString &value)
{
    d_ptr->defaultTitle = false;
    setWindowTitle(value);
}

QColor ZzColorPickerDialog::currentColor() const
{
    return d_ptr->picker->currentColor();
}

void ZzColorPickerDialog::setCurrentColor(QColor color)
{
    const QPointer<ZzColorPickerDialog> guard(this);
    d_ptr->picker->setCurrentColor(color);
    if (guard && !isVisible()) d_ptr->initialColor = currentColor();
}

ZzColorPicker *ZzColorPickerDialog::colorPicker() const noexcept
{
    return d_ptr->picker;
}

QSize ZzColorPickerDialog::sizeHint() const
{
    return QDialog::sizeHint().expandedTo(QSize(480, 0));
}

void ZzColorPickerDialog::setVisible(bool visible)
{
    if (visible && !isVisible()) d_ptr->initialColor = currentColor();
    QDialog::setVisible(visible);
}

void ZzColorPickerDialog::done(int result)
{
    if (d_ptr->finishing) return;
    d_ptr->finishing = true;
    const QPointer<ZzColorPickerDialog> guard(this);
    const bool accepted = result == QDialog::Accepted;
    d_ptr->suppressPreview = !accepted;
    if (QWidget *focused = focusWidget()) focused->clearFocus();
    if (!guard) return;
    d_ptr->picker->commitPendingEdits();
    if (!guard) return;
    if (!accepted) {
        const QColor initial = d_ptr->initialColor;
        d_ptr->picker->setCurrentColor(initial);
        if (!guard) return;
        d_ptr->suppressPreview = false;
        if (d_ptr->lastPreview != initial) {
            d_ptr->lastPreview = initial;
            Q_EMIT currentColorChanged(initial);
            if (!guard) return;
        }
    }
    const QColor selected = currentColor();
    QDialog::done(result);
    if (!guard) return;
    if (accepted) Q_EMIT colorSelected(selected);
    if (guard) d_ptr->finishing = false;
}

void ZzColorPickerDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (!d_ptr) return;
    if (event->type() == QEvent::LanguageChange) d_ptr->refreshText();
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        d_ptr->theme.refreshFallback();
        update();
    }
}

void ZzColorPickerDialog::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const auto theme = d_ptr->theme.snapshot();
    painter.fillRect(rect(), theme->color(ZzColorToken::Surface));
    const QRect footer = d_ptr->footer->geometry();
    painter.fillRect(footer, theme->color(ZzColorToken::SurfaceSecondary));
    painter.setPen(theme->color(ZzColorToken::ControlStroke));
    painter.drawLine(footer.topLeft(), footer.topRight());
}

} // namespace ZzFluentUI
