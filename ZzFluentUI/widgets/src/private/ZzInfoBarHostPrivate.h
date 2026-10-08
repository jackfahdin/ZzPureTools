#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <ZzFluentUI/ZzInfoBarHost.h>

class QTimer;

namespace ZzFluentUI {

class ZzInfoBarPopupSurface;

class ZzInfoBarHostPrivate {
public:
    struct ZzInfoBarHostEntry {
        QPointer<ZzInfoBar> bar;
        QPointer<ZzInfoBarPopupSurface> surface;
        ZzInfoBarHost::Position position = ZzInfoBarHost::TopRight;
        QPointer<QTimer> timer;
        QElapsedTimer elapsed;
        int timeout = 0;
        int remaining = 0;
        bool active = false;
        bool closing = false;
        bool hovered = false;
    };
    using Entry = ZzInfoBarHostEntry;

    ZzInfoBarHostPrivate(ZzInfoBarHost* host, QWidget* target);
    Entry* findEntry(ZzInfoBar* bar) const;
    void startTimer(Entry* entry);
    void pauseTimer(Entry* entry);
    void resumeTimer(Entry* entry);
    void removeEntry(Entry* entry, bool preserveBar = false);
    int surfaceWidth() const;
    int surfaceHeight(const Entry* entry, int width) const;
    QVector<Entry*> activeEntries(ZzInfoBarHost::Position position) const;
    int horizontalPosition(ZzInfoBarHost::Position position, int width) const;
    void fitActiveEntries(ZzInfoBarHost::Position position);
    void repositionPosition(ZzInfoBarHost::Position position, bool animate);
    bool activatePending(ZzInfoBarHost::Position position, bool animateExisting);
    void scheduleReposition(bool animate = false);
    void repositionAll(bool animate = false);

    ZzInfoBarHost* q = nullptr;
    QPointer<QWidget> target;
    QVector<Entry*> entries;
    int margin = 24;
    int spacing = 8;
    int maximumWidth = 360;
    int defaultTimeout = 4500;
    bool repositionScheduled = false;
    bool animateReposition = false;
    bool repositioning = false;
};

} // namespace ZzFluentUI
