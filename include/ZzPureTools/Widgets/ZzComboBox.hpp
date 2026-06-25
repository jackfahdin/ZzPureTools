#ifndef ZZPURETOOLS_WIDGETS_ZZCOMBOBOX_HPP
#define ZZPURETOOLS_WIDGETS_ZZCOMBOBOX_HPP

#include <QComboBox>

#include "ZzPureTools/Core/ZzPureToolsGlobal.hpp"
#include "ZzPureTools/Style/ZzStyleDelegate.hpp"

class ZzAnimator;
class ZzComboBoxDelegate;
class ZzComboBoxItemDelegate;

class ZZ_PURE_TOOLS_EXPORT ZzComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit ZzComboBox(QWidget* parent = nullptr);
    ~ZzComboBox() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showPopup() override;
    void hidePopup() override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void initialize();
    void bindTheme();
    void refreshPopupStyle();
    [[nodiscard]] ZzStyleContext buildStyleContext() const;

    bool m_popupVisible = false;
    ZzAnimator* m_animator = nullptr;
    ZzComboBoxDelegate* m_delegate = nullptr;
    ZzComboBoxItemDelegate* m_itemDelegate = nullptr;
};

#endif // ZZPURETOOLS_WIDGETS_ZZCOMBOBOX_HPP
