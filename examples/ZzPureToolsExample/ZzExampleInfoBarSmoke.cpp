#include "ZzExampleInfoBarSmoke.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <ZzFluentUI/ZzInfoBar.h>
#include <ZzFluentUI/ZzInfoBarHost.h>
#include <ZzFluentUI/ZzThemeController.h>

namespace ZzExample {
bool ZzExampleInfoBarSmoke::isPageReady(const QWidget& window, ZzFluentUI::ZzThemeController* theme)
{
    using ZzFluentUI::ZzInfoBar;
    using ZzFluentUI::ZzInfoBarHost;
    auto* page = window.findChild<QWidget*>(QStringLiteral("zzExampleInfoBarPage"));
    if (!page || !theme)
        return false;
    auto* inlineBar = page->findChild<ZzInfoBar*>(QStringLiteral("infoBarInline0"));
    auto* preview = page->findChild<ZzInfoBar*>(QStringLiteral("infoBarPreview"));
    auto* host = page->findChild<ZzInfoBarHost*>(QStringLiteral("infoBarHost"));
    auto* title = page->findChild<QLineEdit*>(QStringLiteral("infoBarTitle"));
    auto* message = page->findChild<QPlainTextEdit*>(QStringLiteral("infoBarMessage"));
    auto* severity = page->findChild<QComboBox*>(QStringLiteral("infoBarSeverity"));
    auto* custom = page->findChild<QCheckBox*>(QStringLiteral("infoBarCustomAction"));
    auto* open = page->findChild<QCheckBox*>(QStringLiteral("infoBarOpen"));
    auto* reset = page->findChild<QPushButton*>(QStringLiteral("infoBarReset"));
    auto* reopen = page->findChild<QPushButton*>(QStringLiteral("infoBarReopen"));
    auto* popup = page->findChild<QPushButton*>(QStringLiteral("infoBarShowPopup"));
    auto* dismiss = page->findChild<QPushButton*>(QStringLiteral("infoBarDismissAll"));
    if (!inlineBar || !preview || !host || !title || !message || !severity || !custom || !open
        || !reset || !reopen || !popup || !dismiss || host->target() != &window)
        return false;

    const QString originalMessage = inlineBar->message();
    const QString originalAction = inlineBar->actionButtonText();
    inlineBar->actionButton()->click();
    if (inlineBar->message() == originalMessage || !inlineBar->actionButtonText().isEmpty())
        return false;
    inlineBar->setAnimationEnabled(false);
    auto* close = inlineBar->findChild<QToolButton*>();
    if (!close)
        return false;
    close->click();
    if (inlineBar->isOpen())
        return false;
    reopen->click();
    if (!inlineBar->isOpen())
        return false;
    inlineBar->setMessage(originalMessage);
    inlineBar->setActionButtonText(originalAction);
    inlineBar->setAnimationEnabled(true);

    title->setText(QStringLiteral("Edited title"));
    message->setPlainText(QStringLiteral("Edited message"));
    severity->setCurrentIndex(ZzInfoBar::Warning);
    custom->click();
    if (preview->title() != title->text() || preview->message() != message->toPlainText()
        || preview->severity() != ZzInfoBar::Warning || !preview->actionWidget())
        return false;
    custom->click();
    if (preview->actionWidget())
        return false;
    open->click();
    if (preview->isOpen())
        return false;
    reset->click();
    if (!preview->isOpen() || preview->severity() != ZzInfoBar::Informational
        || preview->title() == QStringLiteral("Edited title"))
        return false;

    int shown = 0;
    int closed = 0;
    const bool reducedMotion = theme->reducedMotion();
    theme->setReducedMotion(true);
    const auto connection = QObject::connect(host, &ZzInfoBarHost::infoBarShown, page,
        [&shown](ZzInfoBar*, ZzInfoBarHost::ZzInfoBarPosition) { ++shown; });
    popup->click();
    const auto closedConnection = QObject::connect(host, &ZzInfoBarHost::infoBarClosed, page,
        [&closed](ZzInfoBar*, ZzInfoBarHost::ZzInfoBarPosition) { ++closed; });
    dismiss->click();
    QObject::disconnect(connection);
    QObject::disconnect(closedConnection);
    theme->setReducedMotion(reducedMotion);
    return shown == 1 && closed == 1;
}
} // namespace ZzExample
