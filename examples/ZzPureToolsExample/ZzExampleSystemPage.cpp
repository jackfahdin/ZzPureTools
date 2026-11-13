#include "ZzExampleSystemPage.h"

#include "ZzExampleSystemPagePrivate.h"
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>

#include "ZzExampleLanguageController.h"

namespace ZzExample {

ZzExampleSystemPage::ZzExampleSystemPage(
    ZzExampleSystemPageKind kind,
    const QString &title,
    QAbstractItemModel *model,
    QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzExampleSystemPagePrivate>(this))
{
    ZzExampleTranslationScope translationScope(this);
    d_ptr->initialize(kind, title.trimmed(), model);
}

ZzExampleSystemPage::~ZzExampleSystemPage() = default;

void ZzExampleSystemPage::setLanguageSelection(const QString &language)
{
    if (!d_ptr->languageBox) return;
    const QSignalBlocker blocker(d_ptr->languageBox);
    d_ptr->languageBox->setCurrentIndex(d_ptr->languageBox->findData(language));
}

void ZzExampleSystemPage::setSettingsSnapshot(
    int themeMode,
    const QColor &accentColor,
    int logLevel,
    bool reducedMotion,
    bool activityDockVisible)
{
    d_ptr->setSettingsSnapshot(
        themeMode, accentColor, logLevel, reducedMotion, activityDockVisible);
}

void ZzExampleSystemPage::setStatusText(const QString &text)
{
    d_ptr->setStatusText(text);
}

} // namespace ZzExample
