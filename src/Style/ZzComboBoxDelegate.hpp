#ifndef ZZCOMBOBOXDELEGATE_HPP
#define ZZCOMBOBOXDELEGATE_HPP

#include <QStyledItemDelegate>

#include "ZzFluent/Style/ZzStyleDelegate.hpp"

class ZzComboBoxDelegate : public ZzStyleDelegate
{
public:
    void paint(QPainter& painter, const QRectF& rect, const ZzStyleContext& context) const override;
};

class ZzComboBoxItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ZzComboBoxItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif // ZZCOMBOBOXDELEGATE_HPP
