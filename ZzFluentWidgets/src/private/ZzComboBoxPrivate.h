#ifndef ZZCOMBOBOXPRIVATE_H
#define ZZCOMBOBOXPRIVATE_H

#include <QStyledItemDelegate>

#include "ZzDesignTokens.h"

class QComboBox;
class ZzComboBox;
class ZzTheme;

class ZzComboBoxItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ZzComboBoxItemDelegate(ZzTheme* theme, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    ZzTheme* m_theme = nullptr;
};

class ZzComboBoxPrivate
{
public:
    explicit ZzComboBoxPrivate(ZzComboBox* q);

    ZzComboBox* q_ptr = nullptr;
    bool hovered = false;
    bool popupVisible = false;
    ZzComboBoxItemDelegate* itemDelegate = nullptr;
};

#endif // ZZCOMBOBOXPRIVATE_H
