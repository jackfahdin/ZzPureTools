#pragma once

#include <functional>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzInfoBarHost.h>

class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QVariantAnimation;

namespace ZzFluentUI {

class ZzInfoBarPopupSurface final : public QWidget {
public:
    explicit ZzInfoBarPopupSurface(QWidget* parent);
    void setInfoBar(ZzInfoBar* bar);
    void moveTo(const QPoint& target, bool animate, int duration);
    void startEnter(ZzInfoBarHost::Position position);
    void startLeave(std::function<void()> finished);
    void cancelLeave();
    void deactivateForQueue();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QPointF currentOffset(const QRectF& panelRect) const;
    void finishAnimations();
    void updateInfoBarGeometry();
    QPointer<ZzInfoBar> bar_;
    QPointer<QGraphicsOpacityEffect> opacity_;
    QVariantAnimation* animation_ = nullptr;
    QPropertyAnimation* positionAnimation_ = nullptr;
    QPointF enterDirection_;
    QPointF leaveOffset_;
    std::function<void()> leaveFinished_;
    qreal progress_ = 0;
    bool leaving_ = false;
};

} // namespace ZzFluentUI
