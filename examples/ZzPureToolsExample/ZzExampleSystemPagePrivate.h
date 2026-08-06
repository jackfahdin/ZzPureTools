#pragma once

#include <QtCore/QString>

#include "ZzExampleSystemPageKind.h"

class QAbstractItemModel;
class QComboBox;
class QLabel;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {
class ZzToggleSwitch;
}

namespace ZzExample {

class ZzExampleSystemPage;

/** @brief 实现系统页快照表格和设置控件树。 */
class ZzExampleSystemPagePrivate final
{
public:
    /** @brief 保存非空 View 观察指针。 */
    explicit ZzExampleSystemPagePrivate(ZzExampleSystemPage *page);

    /** @brief 创建页面骨架、设置控件和只读快照表。 */
    void initialize(
        ZzExampleSystemPageKind kind,
        const QString &title,
        QAbstractItemModel *model);

    /** @brief 无信号地同步 Presenter 提供的设置快照。 */
    void setSettingsSnapshot(
        int themeMode,
        int logLevel,
        bool reducedMotion,
        bool activityDockVisible);

    /** @brief 更新设置操作状态。 */
    void setStatusText(const QString &text);

    /** @brief 创建设置输入控件并连接用户意图。 */
    void buildSettings(QVBoxLayout *layout, QWidget *parent);

    ZzExampleSystemPage *q_ptr = nullptr;
    QComboBox *themeModeBox = nullptr;
    QComboBox *logLevelBox = nullptr;
    ZzFluentUI::ZzToggleSwitch *reducedMotionSwitch = nullptr;
    ZzFluentUI::ZzToggleSwitch *activityDockSwitch = nullptr;
    QLabel *statusLabel = nullptr;
};

} // namespace ZzExample
