#include "ZzInfoBadgePrivate.h"

#include <QtCore/QString>
#include <QtWidgets/QSizePolicy>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzInfoBadge.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

namespace ZzFluentUI {

ZzInfoBadgePrivate::ZzInfoBadgePrivate(ZzInfoBadge *q)
    : q_ptr(q)
    , theme(q)
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->setAlignment(Qt::AlignCenter);
    q_ptr->setFocusPolicy(Qt::NoFocus);
    q_ptr->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QString ZzInfoBadgePrivate::displayText() const
{
    if (kind != ZzInfoBadgeKind::Number) {
        return {};
    }
    if (value > maximumValue) {
        return QStringLiteral("%1+").arg(maximumValue);
    }
    return QString::number(value);
}

ZzColorToken ZzInfoBadgePrivate::fillToken() const noexcept
{
    switch (severity) {
    case ZzMessageSeverity::Information:
        return ZzColorToken::Information;
    case ZzMessageSeverity::Success:
        return ZzColorToken::Success;
    case ZzMessageSeverity::Warning:
        return ZzColorToken::Warning;
    case ZzMessageSeverity::Error:
        return ZzColorToken::Error;
    }
    return ZzColorToken::Information;
}

void ZzInfoBadgePrivate::refreshPresentation()
{
    const QString visibleText = displayText();
    q_ptr->QLabel::setText(visibleText);
    const QFont caption = theme.snapshot()->font(ZzTypographyToken::Caption);
    if (q_ptr->font() != caption) {
        q_ptr->setFont(caption);
    }

    const QString severityText = [this] {
        switch (severity) {
        case ZzMessageSeverity::Information:
            return ZzInfoBadge::tr("信息");
        case ZzMessageSeverity::Success:
            return ZzInfoBadge::tr("成功");
        case ZzMessageSeverity::Warning:
            return ZzInfoBadge::tr("警告");
        case ZzMessageSeverity::Error:
            return ZzInfoBadge::tr("错误");
        }
        return ZzInfoBadge::tr("信息");
    }();
    QString accessibleName;
    switch (kind) {
    case ZzInfoBadgeKind::Dot:
        accessibleName = ZzInfoBadge::tr("%1状态").arg(severityText);
        break;
    case ZzInfoBadgeKind::Number:
        accessibleName = ZzInfoBadge::tr("%1，%2").arg(
            severityText,
            visibleText);
        break;
    case ZzInfoBadgeKind::Icon:
        accessibleName = ZzInfoBadge::tr("%1图标").arg(severityText);
        break;
    }
    if (q_ptr->accessibleName().isEmpty()
        || q_ptr->accessibleName() == generatedAccessibleName) {
        generatedAccessibleName = accessibleName;
        q_ptr->setAccessibleName(generatedAccessibleName);
    }
    q_ptr->updateGeometry();
    q_ptr->update();
}

} // namespace ZzFluentUI
