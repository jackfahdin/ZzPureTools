#include <ZzFluentUI/ZzSuggestBox.h>

#include <algorithm>
#include <utility>

#include <QtCore/QModelIndex>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>

#include "private/ZzSuggestBoxPrivate.h"

namespace ZzFluentUI {

ZzSuggestBox::ZzSuggestBox(QWidget *parent)
    : QLineEdit(parent)
    , d_ptr(std::make_unique<ZzSuggestBoxPrivate>(this))
{
    qRegisterMetaType<ZzSuggestion>();
    QLineEdit::setCompleter(d_ptr->completer);
    connect(
        d_ptr->completer,
        qOverload<const QModelIndex &>(&QCompleter::activated),
        this,
        [this](const QModelIndex &index) {
            const ZzSuggestion suggestion =
                ZzSuggestBoxPrivate::suggestionFromIndex(index);
            if (!suggestion.key.isEmpty()) {
                Q_EMIT suggestionActivated(suggestion);
            }
        });
    connect(
        d_ptr->completer,
        qOverload<const QModelIndex &>(&QCompleter::highlighted),
        this,
        [this](const QModelIndex &index) {
            const ZzSuggestion suggestion =
                ZzSuggestBoxPrivate::suggestionFromIndex(index);
            if (!suggestion.key.isEmpty()) {
                Q_EMIT suggestionHighlighted(suggestion);
            }
        });
}

ZzSuggestBox::~ZzSuggestBox() = default;

void ZzSuggestBox::setSuggestions(QList<ZzSuggestion> suggestions)
{
    d_ptr->setSuggestions(std::move(suggestions));
    Q_EMIT suggestionsChanged();
}

QList<ZzSuggestion> ZzSuggestBox::suggestions() const
{
    return d_ptr->suggestions();
}

int ZzSuggestBox::suggestionCount() const noexcept
{
    return d_ptr->suggestionCount();
}

QString ZzSuggestBox::addSuggestion(
    QString text,
    QVariant payload,
    QIcon icon)
{
    return addSuggestion({{}, std::move(text), std::move(icon),
                          std::move(payload), true});
}

QString ZzSuggestBox::addSuggestion(ZzSuggestion suggestion)
{
    const QString key = d_ptr->addSuggestion(std::move(suggestion));
    if (!key.isEmpty()) {
        Q_EMIT suggestionsChanged();
    }
    return key;
}

bool ZzSuggestBox::removeSuggestion(const QString &key)
{
    if (!d_ptr->removeSuggestion(key)) {
        return false;
    }
    Q_EMIT suggestionsChanged();
    return true;
}

bool ZzSuggestBox::removeSuggestionAt(int index)
{
    if (!d_ptr->removeSuggestionAt(index)) {
        return false;
    }
    Q_EMIT suggestionsChanged();
    return true;
}

void ZzSuggestBox::clearSuggestions()
{
    if (d_ptr->clearSuggestions()) {
        Q_EMIT suggestionsChanged();
    }
}

void ZzSuggestBox::setCaseSensitivity(Qt::CaseSensitivity sensitivity)
{
    if (d_ptr->completer->caseSensitivity() == sensitivity) {
        return;
    }
    d_ptr->completer->setCaseSensitivity(sensitivity);
    d_ptr->refreshVisiblePopup();
    Q_EMIT caseSensitivityChanged(sensitivity);
}

Qt::CaseSensitivity ZzSuggestBox::caseSensitivity() const noexcept
{
    return d_ptr->completer->caseSensitivity();
}

void ZzSuggestBox::setFilterMode(Qt::MatchFlag mode)
{
    if (!ZzSuggestBoxPrivate::isSupportedFilterMode(mode)
        || d_ptr->completer->filterMode() == mode) {
        return;
    }
    d_ptr->completer->setFilterMode(mode);
    d_ptr->refreshVisiblePopup();
    Q_EMIT filterModeChanged(mode);
}

Qt::MatchFlag ZzSuggestBox::filterMode() const noexcept
{
    const Qt::MatchFlags flags = d_ptr->completer->filterMode();
    if (flags == Qt::MatchStartsWith) {
        return Qt::MatchStartsWith;
    }
    if (flags == Qt::MatchEndsWith) {
        return Qt::MatchEndsWith;
    }
    return Qt::MatchContains;
}

void ZzSuggestBox::setMaximumVisibleItems(int count)
{
    const int bounded = std::clamp(count, 1, 100);
    if (d_ptr->completer->maxVisibleItems() == bounded) {
        return;
    }
    d_ptr->completer->setMaxVisibleItems(bounded);
    d_ptr->refreshVisiblePopup();
    Q_EMIT maximumVisibleItemsChanged(bounded);
}

int ZzSuggestBox::maximumVisibleItems() const noexcept
{
    return d_ptr->completer->maxVisibleItems();
}

void ZzSuggestBox::showSuggestions()
{
    if (d_ptr->model == nullptr || d_ptr->suggestionCount() == 0) {
        return;
    }
    d_ptr->completer->setCompletionPrefix(text());
    d_ptr->completer->complete();
}

void ZzSuggestBox::hideSuggestions()
{
    d_ptr->popup->hide();
}

bool ZzSuggestBox::isSuggestionPopupVisible() const noexcept
{
    return d_ptr->popup->isVisible();
}

} // namespace ZzFluentUI
