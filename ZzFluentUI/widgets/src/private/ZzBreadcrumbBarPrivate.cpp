#include "ZzBreadcrumbBarPrivate.h"

#include <QtGui/QFont>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzBreadcrumbBar.h>

namespace ZzFluentUI {

namespace {

constexpr auto zzBreadcrumbIndexProperty = "zzBreadcrumbIndex";
constexpr auto zzBreadcrumbCurrentProperty = "zzBreadcrumbCurrent";

} // namespace

ZzBreadcrumbBarPrivate::ZzBreadcrumbBarPrivate(ZzBreadcrumbBar *q)
    : q_ptr(q)
    , layout(new QHBoxLayout(q))
{
    Q_ASSERT(q_ptr != nullptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
}

void ZzBreadcrumbBarPrivate::rebuild()
{
    buttons.clear();
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (qsizetype visual = 0; visual < items.size(); ++visual) {
        const int logicalIndex = static_cast<int>(visual);
        auto *button = new QToolButton(q_ptr);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setText(items.at(logicalIndex));
        button->setToolTip(items.at(logicalIndex));
        button->setAccessibleName(items.at(logicalIndex));
        button->setProperty(zzBreadcrumbIndexProperty, logicalIndex);
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QObject::connect(
            button,
            &QToolButton::clicked,
            q_ptr,
            [this, button] {
                Q_EMIT q_ptr->indexRequested(
                    button->property(
                        zzBreadcrumbIndexProperty).toInt());
                updateCurrentState();
            });
        layout->addWidget(button, 1);
        buttons.append(button);

        if (visual + 1 < items.size()) {
            auto *separator = new QLabel(QStringLiteral("/"), q_ptr);
            separator->setAlignment(Qt::AlignCenter);
            separator->setAttribute(Qt::WA_TransparentForMouseEvents);
            separator->setAccessibleName(QString());
            layout->addWidget(separator);
        }
    }
    updateCurrentState();
}

void ZzBreadcrumbBarPrivate::updateCurrentState()
{
    for (QToolButton *button : buttons) {
        const bool current =
            button->property(zzBreadcrumbIndexProperty).toInt()
            == currentIndex;
        button->setProperty(zzBreadcrumbCurrentProperty, current);
        QFont presentationFont = button->font();
        presentationFont.setBold(current);
        button->setFont(presentationFont);
    }
}

} // namespace ZzFluentUI
