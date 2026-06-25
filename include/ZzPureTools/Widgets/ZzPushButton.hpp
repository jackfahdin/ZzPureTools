#ifndef ZZPURETOOLS_WIDGETS_ZZPUSHBUTTON_HPP
#define ZZPURETOOLS_WIDGETS_ZZPUSHBUTTON_HPP

#include <QPushButton>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Style/ZzStyleDelegate.hpp"

class ZzAnimator;
class ZzPushButtonDelegate;

class ZZ_PURE_TOOLS_EXPORT ZzPushButton : public QPushButton
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

#endif // ZZPURETOOLS_WIDGETS_ZZPUSHBUTTON_HPP
