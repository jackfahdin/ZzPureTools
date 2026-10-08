#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>

#include <ZzFluentUI/ZzInfoBar.h>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QToolButton;
class QVariantAnimation;

namespace ZzFluentUI {

class ZzInfoBarLayout;
class ZzInfoBarMessageLabel;

class ZzInfoBarPrivate {
public:
    ZzInfoBarLayout* panel = nullptr;
    QWidget* icon = nullptr;
    QLabel* titleLabel = nullptr;
    ZzInfoBarMessageLabel* messageLabel = nullptr;
    QPushButton* actionButton = nullptr;
    QToolButton* closeButton = nullptr;
    QWidget* closeArea = nullptr;
    QWidget* actionContainer = nullptr;
    QHBoxLayout* actionLayout = nullptr;
    QPointer<QWidget> actionWidget;
    QMetaObject::Connection actionDestroyed;
    QVariantAnimation* heightAnimation = nullptr;
    ZzInfoBar::Severity severity = ZzInfoBar::Informational;
    QString title;
    QString message;
    QString actionButtonText;
    bool open = false;
    bool closable = true;
    bool iconVisible = true;
    bool animationEnabled = true;
    int animationDuration = 167;
};

} // namespace ZzFluentUI
