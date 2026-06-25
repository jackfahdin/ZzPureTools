#include "ZzDemoViewModel.h"

#include "ZzDesignTokens.h"

ZzDemoViewModel::ZzDemoViewModel(QObject* parent)
    : QObject(parent)
    , m_comboItems({
          QStringLiteral("Fluent UI"),
          QStringLiteral("Qt Widgets"),
          QStringLiteral("Cross Platform"),
          QStringLiteral("Theme System"),
      })
    , m_statusMessage(QStringLiteral("Ready"))
    , m_selectedItem(m_comboItems.value(0))
{
}

QString ZzDemoViewModel::statusMessage() const
{
    return m_statusMessage;
}

QString ZzDemoViewModel::selectedItem() const
{
    return m_selectedItem;
}

int ZzDemoViewModel::clickCount() const
{
    return m_clickCount;
}

QStringList ZzDemoViewModel::comboItems() const
{
    return m_comboItems;
}

void ZzDemoViewModel::onAccentButtonClicked()
{
    ++m_clickCount;
    setStatusMessage(QStringLiteral("Accent button clicked %1 time(s)").arg(m_clickCount));
    emit clickCountChanged(m_clickCount);
}

void ZzDemoViewModel::onStandardButtonClicked()
{
    setStatusMessage(QStringLiteral("Standard button clicked at selection: %1").arg(m_selectedItem));
}

void ZzDemoViewModel::onComboIndexChanged(int index)
{
    if (index < 0 || index >= m_comboItems.size()) {
        return;
    }

    setSelectedItem(m_comboItems.at(index));
    setStatusMessage(QStringLiteral("Combo selection changed to: %1").arg(m_selectedItem));
}

void ZzDemoViewModel::onThemeModeRequested(ZzThemeMode mode)
{
    emit themeModeApplyRequested(mode);
    switch (mode) {
    case ZzThemeMode::Light:
        setStatusMessage(QStringLiteral("Theme switched to Light"));
        break;
    case ZzThemeMode::Dark:
        setStatusMessage(QStringLiteral("Theme switched to Dark"));
        break;
    case ZzThemeMode::System:
    default:
        setStatusMessage(QStringLiteral("Theme follows system"));
        break;
    }
}

void ZzDemoViewModel::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged(m_statusMessage);
}

void ZzDemoViewModel::setSelectedItem(const QString& item)
{
    if (m_selectedItem == item) {
        return;
    }

    m_selectedItem = item;
    emit selectedItemChanged(m_selectedItem);
}
