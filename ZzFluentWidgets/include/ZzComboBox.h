#ifndef ZZCOMBOBOX_H
#define ZZCOMBOBOX_H

#include <QComboBox>

#include "ZzFluentWidgetsGlobal.h"

enum class ZzControlState;

class ZzComboBoxPrivate;

class ZZ_WIDGETS_EXPORT ZzComboBox : public QComboBox
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

private:
    void initialize();
    void bindTheme();
    void refreshPopupStyle();
    [[nodiscard]] ZzControlState controlState() const;

    ZzComboBoxPrivate* d_ptr;
};

#endif // ZZCOMBOBOX_H
