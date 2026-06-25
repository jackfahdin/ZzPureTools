#ifndef ZZPUSHBUTTON_H
#define ZZPUSHBUTTON_H

#include <QPushButton>

#include "ZzFluentWidgetsGlobal.h"

enum class ZzControlState;

class ZzPushButtonPrivate;

class ZZ_WIDGETS_EXPORT ZzPushButton : public QPushButton
{
    Q_OBJECT

public:
    enum class ZzButtonStyle {
        Standard,
        Accent,
    };
    Q_ENUM(ZzButtonStyle)

    explicit ZzPushButton(QWidget* parent = nullptr);
    explicit ZzPushButton(const QString& text, QWidget* parent = nullptr);
    ~ZzPushButton() override;

    void setButtonStyle(ZzButtonStyle style);
    [[nodiscard]] ZzButtonStyle buttonStyle() const noexcept;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void initialize();
    void bindTheme();
    [[nodiscard]] ZzControlState controlState() const;

    ZzPushButtonPrivate* d_ptr;
};

#endif // ZZPUSHBUTTON_H
