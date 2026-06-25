#ifndef ZZFLUENT_WIDGETS_ZZPUSHBUTTON_HPP
#define ZZFLUENT_WIDGETS_ZZPUSHBUTTON_HPP

#include <QPushButton>

#include "ZzFluent/Core/ZzFluentGlobal.hpp"
#include "ZzFluent/Style/ZzStyleDelegate.hpp"

class ZzAnimator;
class ZzPushButtonDelegate;

class ZZ_FLUENT_EXPORT ZzPushButton : public QPushButton
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
    void changeEvent(QEvent* event) override;

private:
    void initialize();
    void bindTheme();
    [[nodiscard]] ZzStyleContext buildStyleContext() const;

    ZzButtonStyle m_buttonStyle = ZzButtonStyle::Standard;
    ZzAnimator* m_animator = nullptr;
    ZzPushButtonDelegate* m_delegate = nullptr;
};

#endif // ZZFLUENT_WIDGETS_ZZPUSHBUTTON_HPP
