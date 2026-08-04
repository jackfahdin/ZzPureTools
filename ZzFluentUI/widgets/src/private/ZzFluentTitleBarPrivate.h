#pragma once

#include <QtCore/QString>
#include <QtGui/QIcon>

class QLabel;
class QToolButton;

namespace ZzFluentUI {

class ZzFluentTitleBar;

/** @brief 管理标题栏纯视觉子控件、翻译和状态，不执行窗口命令。 */
class ZzFluentTitleBarPrivate final
{
public:
    /** @brief 创建标题、图标和三个意图按钮。 */
    explicit ZzFluentTitleBarPrivate(ZzFluentTitleBar *q);

    /** @brief 刷新图标、tooltip、accessible name 和标题。 */
    void refreshPresentation();

    ZzFluentTitleBar *const q_ptr;
    QLabel *iconLabel = nullptr;
    QLabel *titleLabel = nullptr;
    QToolButton *minimizeButton = nullptr;
    QToolButton *maximizeButton = nullptr;
    QToolButton *closeButton = nullptr;
    QString title;
    QIcon windowIcon;
    bool maximized = false;
    bool systemButtonsVisible = true;
};

} // namespace ZzFluentUI
