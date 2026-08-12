#include <ZzFluentUI/ZzKeyBinder.h>

#include <QtCore/QEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QKeyEvent>

#include "private/ZzKeyBinderPrivate.h"

namespace ZzFluentUI {

ZzKeyBinder::ZzKeyBinder(QWidget *parent)
    : QKeySequenceEdit(parent)
    , d_ptr(std::make_unique<ZzKeyBinderPrivate>(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setMaximumSequenceLength(1);
}

ZzKeyBinder::ZzKeyBinder(
    const QKeySequence &sequence,
    QWidget *parent)
    : QKeySequenceEdit(sequence, parent)
    , d_ptr(std::make_unique<ZzKeyBinderPrivate>(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setMaximumSequenceLength(1);
}

ZzKeyBinder::~ZzKeyBinder() = default;

bool ZzKeyBinder::isRecording() const noexcept
{
    return d_ptr->recording;
}

void ZzKeyBinder::startRecording()
{
    d_ptr->beginRecording();
    setFocus(Qt::OtherFocusReason);
}

void ZzKeyBinder::cancelRecording()
{
    d_ptr->cancelRecording();
}

void ZzKeyBinder::focusInEvent(QFocusEvent *event)
{
    QKeySequenceEdit::focusInEvent(event);
    d_ptr->beginRecording();
}

void ZzKeyBinder::focusOutEvent(QFocusEvent *event)
{
    QKeySequenceEdit::focusOutEvent(event);
    d_ptr->acceptRecording();
}

void ZzKeyBinder::keyPressEvent(QKeyEvent *event)
{
    if (event == nullptr) {
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (d_ptr->recording) {
            d_ptr->cancelRecording();
        }
        event->accept();
        return;
    }

    d_ptr->beginRecording();
    if (event->key() == Qt::Key_Backspace) {
        clear();
        event->accept();
        return;
    }
    QKeySequenceEdit::keyPressEvent(event);
}

void ZzKeyBinder::changeEvent(QEvent *event)
{
    QKeySequenceEdit::changeEvent(event);
    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        d_ptr->refreshAccessibleText();
    }
}

} // namespace ZzFluentUI
