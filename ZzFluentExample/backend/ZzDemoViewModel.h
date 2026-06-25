#ifndef ZZDEMOVIEWMODEL_H
#define ZZDEMOVIEWMODEL_H

#include <QObject>
#include <QStringList>

#include "ZzDesignTokens.h"

class ZzDemoViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString selectedItem READ selectedItem NOTIFY selectedItemChanged)
    Q_PROPERTY(int clickCount READ clickCount NOTIFY clickCountChanged)

public:
    explicit ZzDemoViewModel(QObject* parent = nullptr);

    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString selectedItem() const;
    [[nodiscard]] int clickCount() const;
    [[nodiscard]] QStringList comboItems() const;

public slots:
    void onAccentButtonClicked();
    void onStandardButtonClicked();
    void onComboIndexChanged(int index);
    void onThemeModeRequested(ZzThemeMode mode);

signals:
    void statusMessageChanged(const QString& message);
    void selectedItemChanged(const QString& item);
    void clickCountChanged(int count);
    void themeModeApplyRequested(ZzThemeMode mode);

private:
    void setStatusMessage(const QString& message);
    void setSelectedItem(const QString& item);

    QStringList m_comboItems;
    QString m_statusMessage;
    QString m_selectedItem;
    int m_clickCount = 0;
};

#endif // ZZDEMOVIEWMODEL_H
