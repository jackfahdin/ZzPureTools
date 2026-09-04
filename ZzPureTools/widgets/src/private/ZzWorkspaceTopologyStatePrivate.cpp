#include "ZzWorkspaceTopologyStatePrivate.h"

#include <algorithm>

#include <QtCore/QHash>
#include <QtCore/QSet>

namespace ZzPureTools {
namespace {

using ZzState = ZzWorkspaceTopologyStatePrivate;

[[nodiscard]] bool validSize(const QSize &size) noexcept
{
    return size == QSize() ||
        (size.isValid()
            && size.width() <= ZzState::MaximumPayloadSize
            && size.height() <= ZzState::MaximumPayloadSize);
}

[[nodiscard]] bool validRect(const QRect &rect) noexcept
{
    return rect == QRect()
        || (rect.isValid()
            && rect.width() <= ZzState::MaximumPayloadSize
            && rect.height() <= ZzState::MaximumPayloadSize);
}

[[nodiscard]] bool validTitleMode(ZzWorkspaceTitleMode mode) noexcept
{
    switch (mode) {
    case ZzWorkspaceTitleMode::Application:
    case ZzWorkspaceTitleMode::CurrentTab:
    case ZzWorkspaceTitleMode::CurrentTabAndApplication:
    case ZzWorkspaceTitleMode::Custom:
        return true;
    }
    return false;
}

[[nodiscard]] bool validClosePolicy(ZzWindowClosePolicy policy) noexcept
{
    switch (policy) {
    case ZzWindowClosePolicy::Allow:
    case ZzWindowClosePolicy::Deny:
    case ZzWindowClosePolicy::Delegate:
        return true;
    }
    return false;
}

[[nodiscard]] bool validString(const QString &value, bool allowEmpty) noexcept
{
    return value.size() <= ZzState::MaximumStringLength
        && (allowEmpty || !value.isEmpty());
}

} // namespace

bool ZzWorkspaceTopologyStatePrivate::ZzWindowState::operator==(
    const ZzWindowState &other) const noexcept
{
    return windowId == other.windowId
        && configuration.title == other.configuration.title
        && configuration.titleMode == other.configuration.titleMode
        && configuration.closePolicy == other.configuration.closePolicy
        && configuration.alwaysOnTop == other.configuration.alwaysOnTop
        && configuration.minimumSize == other.configuration.minimumSize
        && configuration.maximumSize == other.configuration.maximumSize
        && configuration.initialGeometry == other.configuration.initialGeometry
        && geometry == other.geometry && screenName == other.screenName
        && visible == other.visible && maximized == other.maximized
        && alwaysOnTop == other.alwaysOnTop && treeDepth == other.treeDepth
        && workspaceState == other.workspaceState && pages == other.pages;
}

bool ZzWorkspaceTopologyStatePrivate::operator==(
    const ZzWorkspaceTopologyStatePrivate &other) const noexcept
{
    return windows == other.windows;
}

bool ZzWorkspaceTopologyStatePrivate::isValid() const noexcept
{
    return validate(*this);
}

bool ZzWorkspaceTopologyStatePrivate::validate(
    const ZzWorkspaceTopologyStatePrivate &state) noexcept
{
    if (state.windows.isEmpty() || state.windows.size() > MaximumWindows) {
        return false;
    }

    QSet<QUuid> windowIds;
    QSet<ZzFluentUI::ZzWorkspacePageId> pageIds;
    QSet<QString> layoutKeys;
    int pageCount = 0;
    for (const ZzWindowState &window : state.windows) {
        if (window.windowId.isNull() || windowIds.contains(window.windowId)
            || !validString(window.configuration.title, true)
            || !validTitleMode(window.configuration.titleMode)
            || !validClosePolicy(window.configuration.closePolicy)
            || !validString(window.screenName, true)
            || window.workspaceState.size() > MaximumWorkspaceStateSize
            || window.treeDepth < 1 || window.treeDepth > MaximumTreeDepth
            || !window.geometry.isValid()
            || window.geometry.width() <= 0 || window.geometry.height() <= 0
            || !validRect(window.configuration.initialGeometry)
            || !validSize(window.configuration.minimumSize)
            || !validSize(window.configuration.maximumSize)
            || (window.configuration.minimumSize.width() > 0
                && window.configuration.maximumSize.width() > 0
                && window.configuration.minimumSize.width()
                    > window.configuration.maximumSize.width())
            || (window.configuration.minimumSize.height() > 0
                && window.configuration.maximumSize.height() > 0
                && window.configuration.minimumSize.height()
                    > window.configuration.maximumSize.height())) {
            return false;
        }
        windowIds.insert(window.windowId);

        QSet<QString> groups;
        QHash<QString, QSet<int>> groupIndexes;
        if (window.pages.size() > MaximumPages) {
            return false;
        }
        for (const ZzPageState &page : window.pages) {
            ++pageCount;
            if (pageCount > MaximumPages || !page.pageId.isValid()
                || pageIds.contains(page.pageId)
                || page.windowId != window.windowId
                || !validString(page.layoutKey, true)
                || (!page.layoutKey.isEmpty()
                    && layoutKeys.contains(page.layoutKey))
                || !validString(page.groupId, false) || page.index < 0
                || page.index > MaximumPages
                || groupIndexes[page.groupId].contains(page.index)
                || page.origins.size() > MaximumOriginDepth) {
                return false;
            }
            pageIds.insert(page.pageId);
            if (!page.layoutKey.isEmpty()) {
                layoutKeys.insert(page.layoutKey);
            }
            groups.insert(page.groupId);
            groupIndexes[page.groupId].insert(page.index);
            for (const ZzPageOrigin &origin : page.origins) {
                if (origin.windowId.isNull() || !validString(origin.groupId, false)
                    || origin.index < 0 || origin.index > MaximumPages) {
                    return false;
                }
            }
        }
        if (groups.size() > MaximumGroupsPerWindow) {
            return false;
        }
    }
    return true;
}

} // namespace ZzPureTools
