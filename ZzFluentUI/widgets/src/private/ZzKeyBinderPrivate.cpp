#include "ZzKeyBinderPrivate.h"

#include <QtCore/QObject>

#include <ZzFluentUI/ZzKeyBinder.h>

namespace ZzFluentUI {

ZzKeyBinderPrivate::ZzKeyBinderPrivate(ZzKeyBinder *q)
    : q_ptr(q)
{
    Q_ASSERT(q_ptr != nullptr);
    QObject::connect(
        q_ptr,
        &QKeySequenceEdit::editingFinished,
        q_ptr,
        [this] {
            acceptRecording();
        });
    refreshAccessibleText();
}

void ZzKeyBinderPrivate::beginRecording()
{
    if (recording || !q_ptr->isEnabled()) {
        return;
    }
    sequenceBeforeRecording = q_ptr->keySequence();
    recording = true;
    Q_EMIT q_ptr->recordingChanged(true);
}

void ZzKeyBinderPrivate::acceptRecording()
{
    if (!recording) {
        return;
    }
    const QKeySequence acceptedSequence = q_ptr->keySequence();
    recording = false;
    sequenceBeforeRecording = {};
    Q_EMIT q_ptr->recordingChanged(false);
    Q_EMIT q_ptr->recordingAccepted(acceptedSequence);
}

void ZzKeyBinderPrivate::cancelRecording()
{
    if (!recording) {
        return;
    }
    const QKeySequence restoredSequence = sequenceBeforeRecording;
    recording = false;
    sequenceBeforeRecording = {};
    q_ptr->setKeySequence(restoredSequence);
    Q_EMIT q_ptr->recordingChanged(false);
    Q_EMIT q_ptr->recordingCanceled();
}

void ZzKeyBinderPrivate::refreshAccessibleText()
{
    const QString instruction = ZzKeyBinder::tr(
        "按下快捷键；Escape 取消；Backspace 清除");
    q_ptr->setAccessibleDescription(instruction);
    q_ptr->setToolTip(instruction);
}

} // namespace ZzFluentUI
