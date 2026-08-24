#include "ZzPanelStackPrivate.h"

#include <algorithm>
#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QIcon>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

namespace {

[[nodiscard]] bool zzHasIcon(const ZzIconDescriptor &descriptor) noexcept
{
    return descriptor.source == ZzIconSource::FontGlyph
        ? descriptor.fontIcon != ZzFontIcon::None
        : !descriptor.resourceId.isEmpty();
}

[[nodiscard]] bool zzSameIconDescriptor(
    const ZzIconDescriptor &first,
    const ZzIconDescriptor &second) noexcept
{
    return first.resourceId == second.resourceId
        && first.mirroredInRightToLeft == second.mirroredInRightToLeft
        && first.source == second.source
        && first.fontIcon == second.fontIcon
        && first.colorMode == second.colorMode
        && first.customColor == second.customColor;
}

} // namespace

/** @brief 固定持有标题、缓存图标、关闭按钮和单个内容页面。 */
class ZzPanelFrame final : public QWidget
{
public:
    ZzPanelFrame(
        QWidget *content,
        const QString &title,
        const ZzIconDescriptor &icon,
        QWidget *parent)
        : QWidget(parent)
        , content_(content)
        , icon_(icon)
        , theme_(this)
    {
        setObjectName(QStringLiteral("zzPanelStackFrame"));
        header_ = new QWidget(this);
        header_->setObjectName(QStringLiteral("zzPanelStackHeader"));
        iconLabel_ = new QLabel(header_);
        iconLabel_->setObjectName(QStringLiteral("zzPanelStackIcon"));
        iconLabel_->setAlignment(Qt::AlignCenter);
        titleLabel_ = new QLabel(title, header_);
        titleLabel_->setObjectName(QStringLiteral("zzPanelStackTitle"));
        closeButton_ = new ZzIconButton(header_);
        closeButton_->setObjectName(
            QStringLiteral("zzPanelStackCloseButton"));
        closeButton_->setIconDescriptor(
            ZzIconDescriptor::fromBundledSvg(ZzBundledSvgIcon::Close));

        auto *headerLayout = new QHBoxLayout(header_);
        headerLayout->addWidget(iconLabel_);
        headerLayout->addWidget(titleLabel_, 1);
        headerLayout->addWidget(closeButton_);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(header_);
        rootLayout->addWidget(content_, 1);
        setTitle(title);
        refreshVisuals();
    }

    [[nodiscard]] ZzIconButton *closeButton() const noexcept
    {
        return closeButton_;
    }

    void setTitle(const QString &title)
    {
        titleLabel_->setText(title);
        closeButton_->setAccessibleName(
            title.isEmpty()
                ? ZzPanelStack::tr("关闭面板")
                : ZzPanelStack::tr("关闭 %1").arg(title));
    }

    void setIconDescriptor(const ZzIconDescriptor &icon)
    {
        icon_ = icon;
        refreshIcon();
    }

    /** @brief 仅在本框架仍直接持有内容时解除父对象并归还所有权。 */
    [[nodiscard]] QWidget *releaseContent()
    {
        if (content_ == nullptr) {
            return nullptr;
        }
        QPointer<QWidget> contentGuard(content_);
        content_ = nullptr;
        if (contentGuard->parentWidget() != this) {
            return nullptr;
        }
        layout()->removeWidget(contentGuard);
        contentGuard->setParent(nullptr);
        if (contentGuard == nullptr || contentGuard->parent() != nullptr) {
            return nullptr;
        }
        return contentGuard.data();
    }

protected:
    void changeEvent(QEvent *event) override
    {
        QWidget::changeEvent(event);
        if (event == nullptr) {
            return;
        }
        switch (event->type()) {
        case QEvent::ApplicationFontChange:
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
        case QEvent::LayoutDirectionChange:
        case QEvent::DevicePixelRatioChange:
            theme_.refreshFallback();
            refreshVisuals();
            break;
        default:
            break;
        }
    }

private:
    void refreshVisuals()
    {
        const std::shared_ptr<const ZzThemeSnapshot> snapshot =
            theme_.snapshot();
        const int headerHeight = qRound(
            snapshot->metric(ZzMetricToken::PanelHeaderHeight));
        const int iconExtent = qRound(
            snapshot->metric(ZzMetricToken::IconSmall));
        const int horizontalPadding = qRound(
            snapshot->metric(ZzMetricToken::HorizontalPadding));
        const int spacing = qRound(
            snapshot->metric(ZzMetricToken::VerticalPadding));
        header_->setFixedHeight(headerHeight);
        iconLabel_->setFixedSize(iconExtent, iconExtent);
        closeButton_->setFixedSize(headerHeight, headerHeight);
        auto *headerLayout = qobject_cast<QHBoxLayout *>(header_->layout());
        Q_ASSERT(headerLayout != nullptr);
        headerLayout->setContentsMargins(
            horizontalPadding,
            0,
            0,
            0);
        headerLayout->setSpacing(spacing);
        titleLabel_->setFont(snapshot->font(ZzTypographyToken::BodyStrong));
        refreshIcon();
    }

    void refreshIcon()
    {
        const bool hasIcon = zzHasIcon(icon_);
        iconLabel_->setVisible(hasIcon);
        if (!hasIcon) {
            iconLabel_->clear();
            return;
        }
        auto *fluentStyle = qobject_cast<ZzFluentStyle *>(style());
        if (fluentStyle == nullptr) {
            iconLabel_->clear();
            return;
        }
        const QPalette::ColorGroup group = isEnabled()
            ? QPalette::Normal
            : QPalette::Disabled;
        const QColor paletteColor = palette().color(
            group,
            QPalette::WindowText);
        iconLabel_->setPixmap(fluentStyle->iconPixmap(
            icon_,
            iconLabel_->size(),
            devicePixelRatioF(),
            paletteColor,
            layoutDirection()));
    }

    QPointer<QWidget> content_;
    QWidget *header_ = nullptr;
    QLabel *iconLabel_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    ZzIconButton *closeButton_ = nullptr;
    ZzIconDescriptor icon_;
    ZzWidgetTheme theme_;
};

ZzPanelStackPrivate::ZzPanelStackPrivate(ZzPanelStack *publicObject)
    : q_ptr(publicObject)
{
    Q_ASSERT(q_ptr != nullptr);
    const ZzWidgetTheme theme(q_ptr);
    const std::shared_ptr<const ZzThemeSnapshot> snapshot = theme.snapshot();
    splitter = new QSplitter(Qt::Vertical, q_ptr);
    splitter->setObjectName(QStringLiteral("zzPanelStackSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setOpaqueResize(true);
    splitter->setHandleWidth(qRound(
        snapshot->metric(ZzMetricToken::PanelSplitterExtent)));
    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(splitter);
    QObject::connect(
        splitter,
        &QSplitter::splitterMoved,
        q_ptr,
        [this] { handleSplitterMoved(); });
}

ZzPanelStackPrivate::~ZzPanelStackPrivate()
{
    for (const ZzPanelRecord &record : std::as_const(panels)) {
        QObject::disconnect(record.destroyedConnection);
    }
}

bool ZzPanelStackPrivate::addPanel(
    QWidget *content,
    const QString &title,
    const ZzIconDescriptor &icon)
{
    if (content == nullptr
        || content->parent() != nullptr
        || content->thread() != q_ptr->thread()
        || indexOf(content) >= 0) {
        return false;
    }

    QPointer<QWidget> contentGuard(content);
    const QMetaObject::Connection destroyedConnection = QObject::connect(
        content,
        &QObject::destroyed,
        q_ptr,
        [this, identity = content] { removeDestroyedPanel(identity); });
    QPointer<ZzPanelFrame> frameGuard = new ZzPanelFrame(
        content,
        title,
        icon,
        splitter);
    if (contentGuard.isNull() || frameGuard.isNull()) {
        QObject::disconnect(destroyedConnection);
        if (frameGuard != nullptr) {
            frameGuard->deleteLater();
        }
        return false;
    }

    ZzPanelRecord record;
    record.content = content;
    record.identity = content;
    record.frame = frameGuard.data();
    record.title = title;
    record.icon = icon;
    record.destroyedConnection = destroyedConnection;
    panels.append(record);
    splitter->addWidget(frameGuard.data());
    QObject::connect(
        frameGuard->closeButton(),
        &QToolButton::clicked,
        q_ptr,
        [this, identity = content] {
            const int index = indexOfIdentity(identity);
            if (index < 0 || panels.at(index).content == nullptr) {
                return;
            }
            Q_EMIT q_ptr->panelCloseRequested(
                panels.at(index).content.data());
        });
    applyRememberedSizes();
    if (!updateCurrentPanel(content)) {
        return false;
    }
    return !contentGuard.isNull() && indexOf(contentGuard.data()) >= 0;
}

QWidget *ZzPanelStackPrivate::takePanel(QWidget *content)
{
    const int index = indexOf(content);
    if (index < 0) {
        return nullptr;
    }
    const bool wasCurrent = currentPanel.data() == content;
    ZzPanelRecord record = panels.takeAt(index);
    QObject::disconnect(record.destroyedConnection);
    QPointer<ZzPanelStack> stackGuard(q_ptr);
    QPointer<ZzPanelFrame> frameGuard(record.frame);
    QPointer<QWidget> releasedContent;
    if (frameGuard != nullptr) {
        releasedContent = frameGuard->releaseContent();
    }
    if (stackGuard == nullptr) {
        return releasedContent.data();
    }
    if (frameGuard != nullptr) {
        delete frameGuard.data();
    }
    if (stackGuard == nullptr) {
        return releasedContent.data();
    }
    applyRememberedSizes();
    if (wasCurrent) {
        if (!updateCurrentPanel(firstVisiblePanel())) {
            return releasedContent.data();
        }
    }
    return releasedContent.data();
}

QList<QWidget *> ZzPanelStackPrivate::allPanels() const
{
    QList<QWidget *> result;
    result.reserve(panels.size());
    for (const ZzPanelRecord &record : panels) {
        if (record.content != nullptr) {
            result.append(record.content.data());
        }
    }
    return result;
}

QList<QWidget *> ZzPanelStackPrivate::visiblePanels() const
{
    QList<QWidget *> result;
    result.reserve(panels.size());
    for (const ZzPanelRecord &record : panels) {
        if (record.content != nullptr
            && record.frame != nullptr
            && !record.frame->isHidden()) {
            result.append(record.content.data());
        }
    }
    return result;
}

bool ZzPanelStackPrivate::movePanel(
    QWidget *content,
    int targetIndex)
{
    const int sourceIndex = indexOf(content);
    if (sourceIndex < 0
        || targetIndex < 0
        || targetIndex >= panels.size()) {
        return false;
    }
    if (sourceIndex == targetIndex) {
        return true;
    }
    ZzPanelFrame *const frame = panels.at(sourceIndex).frame;
    panels.move(sourceIndex, targetIndex);
    splitter->insertWidget(targetIndex, frame);
    applyRememberedSizes();
    Q_EMIT q_ptr->panelMoved(content, targetIndex);
    return true;
}

bool ZzPanelStackPrivate::setPanelVisible(
    QWidget *content,
    bool visible)
{
    const int index = indexOf(content);
    if (index < 0) {
        return false;
    }
    ZzPanelRecord &record = panels[index];
    const bool wasVisible = !record.frame->isHidden();
    if (wasVisible == visible) {
        return true;
    }
    record.frame->setVisible(visible);
    applyRememberedSizes();

    QWidget *nextCurrent = currentPanel.data();
    if (!visible && nextCurrent == content) {
        nextCurrent = firstVisiblePanel();
    } else if (visible && nextCurrent == nullptr) {
        nextCurrent = content;
    }
    const bool currentChanged = nextCurrent != currentPanel.data();
    currentPanel = nextCurrent;
    const quint64 notificationRevision = currentNotificationRevision;

    QPointer<ZzPanelStack> stackGuard(q_ptr);
    Q_EMIT q_ptr->panelVisibilityChanged(content, visible);
    if (stackGuard.isNull()) {
        return true;
    }
    if (currentChanged
        && notificationRevision == currentNotificationRevision) {
        if (!notifyCurrentPanelChanged(currentPanel.data())) {
            return true;
        }
    }
    Q_EMIT q_ptr->panelSizesChanged(panelSizes());
    return true;
}

bool ZzPanelStackPrivate::isPanelVisible(QWidget *content) const
{
    const int index = indexOf(content);
    return index >= 0 && !panels.at(index).frame->isHidden();
}

bool ZzPanelStackPrivate::setCurrentPanel(QWidget *content)
{
    if (indexOf(content) < 0) {
        return false;
    }
    QPointer<ZzPanelStack> stackGuard(q_ptr);
    if (!isPanelVisible(content)) {
        if (!setPanelVisible(content, true)
            || stackGuard.isNull()) {
            return false;
        }
    }
    return updateCurrentPanel(content)
        && currentPanel.data() == content;
}

bool ZzPanelStackPrivate::setPanelTitle(
    QWidget *content,
    const QString &title)
{
    const int index = indexOf(content);
    if (index < 0) {
        return false;
    }
    ZzPanelRecord &record = panels[index];
    if (record.title == title) {
        return true;
    }
    record.title = title;
    record.frame->setTitle(title);
    return true;
}

QString ZzPanelStackPrivate::panelTitle(QWidget *content) const
{
    const int index = indexOf(content);
    return index >= 0 ? panels.at(index).title : QString{};
}

bool ZzPanelStackPrivate::setPanelIconDescriptor(
    QWidget *content,
    const ZzIconDescriptor &icon)
{
    const int index = indexOf(content);
    if (index < 0) {
        return false;
    }
    ZzPanelRecord &record = panels[index];
    if (zzSameIconDescriptor(record.icon, icon)) {
        return true;
    }
    record.icon = icon;
    record.frame->setIconDescriptor(icon);
    return true;
}

QList<int> ZzPanelStackPrivate::panelSizes() const
{
    QList<int> result;
    result.reserve(panels.size());
    for (const ZzPanelRecord &record : panels) {
        if (record.content != nullptr
            && record.frame != nullptr
            && !record.frame->isHidden()) {
            result.append(record.lastNonZeroSize);
        }
    }
    return result;
}

bool ZzPanelStackPrivate::setPanelSizes(const QList<int> &sizes)
{
    if (sizes.size() != visiblePanels().size()
        || std::any_of(sizes.cbegin(), sizes.cend(), [](int size) {
               return size <= 0;
           })) {
        return false;
    }
    if (panelSizes() == sizes) {
        return true;
    }
    qsizetype visibleIndex = 0;
    for (ZzPanelRecord &record : panels) {
        if (record.content != nullptr
            && record.frame != nullptr
            && !record.frame->isHidden()) {
            record.lastNonZeroSize = sizes.at(visibleIndex);
            ++visibleIndex;
        }
    }
    applyRememberedSizes();
    Q_EMIT q_ptr->panelSizesChanged(sizes);
    return true;
}

int ZzPanelStackPrivate::indexOf(QWidget *content) const noexcept
{
    if (content == nullptr) {
        return -1;
    }
    for (int index = 0; index < panels.size(); ++index) {
        if (panels.at(index).content.data() == content) {
            return index;
        }
    }
    return -1;
}

int ZzPanelStackPrivate::indexOfIdentity(QWidget *identity) const noexcept
{
    for (int index = 0; index < panels.size(); ++index) {
        if (panels.at(index).identity == identity) {
            return index;
        }
    }
    return -1;
}

void ZzPanelStackPrivate::captureVisibleSizes()
{
    const QList<int> sizes = splitter->sizes();
    const qsizetype limit = std::min(sizes.size(), panels.size());
    for (qsizetype index = 0; index < limit; ++index) {
        ZzPanelRecord &record = panels[index];
        if (record.frame != nullptr
            && !record.frame->isHidden()
            && sizes.at(index) > 0) {
            record.lastNonZeroSize = sizes.at(index);
        }
    }
}

void ZzPanelStackPrivate::applyRememberedSizes()
{
    QList<int> sizes;
    sizes.reserve(panels.size());
    for (const ZzPanelRecord &record : std::as_const(panels)) {
        sizes.append(
            record.frame != nullptr && !record.frame->isHidden()
                ? record.lastNonZeroSize
                : 0);
    }
    applyingSizes = true;
    splitter->setSizes(sizes);
    applyingSizes = false;
}

void ZzPanelStackPrivate::removeDestroyedPanel(QWidget *identity)
{
    const int index = indexOfIdentity(identity);
    if (index < 0) {
        return;
    }
    const bool currentWasDestroyed = currentPanel.isNull()
        || currentPanel.data() == identity;
    ZzPanelRecord record = panels.takeAt(index);
    const bool wasVisible = record.frame != nullptr
        && !record.frame->isHidden();
    QObject::disconnect(record.destroyedConnection);
    if (record.frame != nullptr) {
        record.frame->hide();
        record.frame->setParent(nullptr);
        record.frame->deleteLater();
    }
    applyRememberedSizes();
    if (currentWasDestroyed) {
        if (!updateCurrentPanel(firstVisiblePanel())) {
            return;
        }
    }
    if (wasVisible) {
        Q_EMIT q_ptr->panelSizesChanged(panelSizes());
    }
}

QWidget *ZzPanelStackPrivate::firstVisiblePanel() const noexcept
{
    for (const ZzPanelRecord &record : panels) {
        if (record.content != nullptr
            && record.frame != nullptr
            && !record.frame->isHidden()) {
            return record.content.data();
        }
    }
    return nullptr;
}

bool ZzPanelStackPrivate::updateCurrentPanel(QWidget *content)
{
    if (currentPanel.data() == content) {
        return true;
    }
    currentPanel = content;
    return notifyCurrentPanelChanged(content);
}

bool ZzPanelStackPrivate::notifyCurrentPanelChanged(QWidget *content)
{
    QPointer<ZzPanelStack> stackGuard(q_ptr);
    ++currentNotificationRevision;
    Q_EMIT q_ptr->currentPanelChanged(content);
    return !stackGuard.isNull();
}

void ZzPanelStackPrivate::handleSplitterMoved()
{
    if (applyingSizes) {
        return;
    }
    const QList<int> before = q_ptr->panelSizes();
    captureVisibleSizes();
    const QList<int> after = q_ptr->panelSizes();
    if (before != after) {
        Q_EMIT q_ptr->panelSizesChanged(after);
    }
}

} // namespace ZzFluentUI
