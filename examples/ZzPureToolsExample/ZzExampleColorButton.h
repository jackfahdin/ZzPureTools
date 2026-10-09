#pragma once

#include <QEvent>
#include <QPainter>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>
#include <functional>

namespace ZzExample {

/** @brief 复用现有颜色编辑器，使用参考页的紧凑色块按钮。 */
class ZzExampleColorButton final : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(QColor selectedColor READ selectedColor WRITE setSelectedColor NOTIFY selectedColorChanged)
public:
    explicit ZzExampleColorButton(QWidget *parent)
        : QToolButton(parent)
    {
        setFixedSize(68, 32);
        QObject::connect(this, &QToolButton::clicked, this, [this] {
            if (!popup_) {
                popup_ = new QWidget(this, Qt::Popup);
                auto *layout = new QVBoxLayout(popup_);
                picker_ = new ZzFluentUI::ZzColorPicker(popup_);
                picker_->setAlphaEnabled(true);
                layout->addWidget(picker_);
                QObject::connect(picker_, &ZzFluentUI::ZzColorPicker::currentColorChanged, this,
                    &ZzExampleColorButton::setSelectedColor);
            }
            {
                const QSignalBlocker blocker(picker_);
                picker_->setCurrentColor(selectedColor_);
            }
            popup_->adjustSize();
            const QRect available = screen()->availableGeometry();
            QPoint position = mapToGlobal(QPoint(0, height()));
            position.setX(
                qMax(available.left(), qMin(position.x(), available.right() - popup_->width() + 1)));
            if (position.y() + popup_->height() > available.bottom())
                position.setY(qMax(available.top(), mapToGlobal(QPoint()).y() - popup_->height()));
            popup_->move(position);
            popup_->show();
        });
    }
    void setSelectedColor(const QColor &color)
    {
        if (selectedColor_ == color)
            return;
        selectedColor_ = color;
        setToolTip(color.name(QColor::HexArgb));
        update();
        Q_EMIT selectedColorChanged(color);
    }
    [[nodiscard]] QColor selectedColor() const { return selectedColor_; }
    std::function<QColor()> themeColor;
Q_SIGNALS:
    void selectedColorChanged(const QColor &color);

protected:
    void changeEvent(QEvent *event) override
    {
        QToolButton::changeEvent(event);
        if (themeColor && (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)) {
            const QSignalBlocker blocker(this);
            setSelectedColor(themeColor());
        }
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QStyleOptionToolButton option;
        initStyleOption(&option);
        style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, this);
        const int arrowWidth = style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &option, this);
        const QRect arrow(rect().right() - arrowWidth - 2, 0, arrowWidth, height());
        QRect swatch = rect().adjusted(8, 5, -8, -5);
        swatch.setRight(arrow.left() - 5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(selectedColor_);
        painter.drawRoundedRect(swatch, 3, 3);
        painter.setFont(ZzFluentUI::ZzSegoeIconFont::font(11));
        painter.setPen(
            palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
        painter.drawText(arrow.translated(0, isDown() ? 2 : 0), Qt::AlignCenter, QChar(0xe70d));
    }

private:
    QColor selectedColor_;
    QWidget *popup_ = nullptr;
    ZzFluentUI::ZzColorPicker *picker_ = nullptr;
};

} // namespace ZzExample
