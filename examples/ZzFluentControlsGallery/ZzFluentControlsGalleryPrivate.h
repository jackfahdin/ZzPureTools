#pragma once

#include <QtCore/QPoint>
#include <QtCore/QPointer>

class QHBoxLayout;
class QStandardItemModel;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {

class ZzCalendar;
class ZzCalendarPicker;
class ZzTabWidget;
class ZzThemeController;

} // namespace ZzFluentUI

namespace ZzExamples {

class ZzFluentControlsGallery;

/** @brief 管理画廊的纯展示控件、本地模型和 UI 级交互连接。 */
class ZzFluentControlsGalleryPrivate final
{
public:
    /** @brief 构造首屏控件面并绑定非拥有主题控制器。 */
    ZzFluentControlsGalleryPrivate(
        ZzFluentControlsGallery *q,
        ZzFluentUI::ZzThemeController *themeController);

    /** @brief 构造 System/Light/Dark/HighContrast 分段主题控件。 */
    void buildThemeSelector(QVBoxLayout *rootLayout);

    /** @brief 构造导航、面包屑和列表展示列。 */
    [[nodiscard]] QWidget *buildNavigationColumn(QWidget *parent);

    /** @brief 构造按钮、输入、反馈、进度和 Tab 展示列。 */
    [[nodiscard]] QWidget *buildControlsColumn(QWidget *parent);

    /** @brief 构造 Table、Tree、菜单和 Dialog 展示列。 */
    [[nodiscard]] QWidget *buildDataColumn(QWidget *parent);

    /** @brief 构造只更新本地展示状态的标准命令与状态宿主。 */
    [[nodiscard]] QWidget *buildCommandStatusHost(QWidget *parent);

    /** @brief 构造一个不访问业务对象的窗口模态内容对话框。 */
    void showDialog();

    /** @brief 在指定非拥有目标附近展示教学提示。 */
    void showTeachingTip(QWidget *target);

    /** @brief 为画廊标签容器绑定关闭意图和应用层拖出宿主。 */
    void bindTabHost(ZzFluentUI::ZzTabWidget *tabs);

    /** @brief 创建普通顶层示例宿主并同步移入指定页面。 */
    void showDetachedTab(
        ZzFluentUI::ZzTabWidget *source,
        int index,
        const QPoint &globalPosition);

    ZzFluentControlsGallery *const q_ptr;
    QPointer<ZzFluentUI::ZzThemeController> controller;
    QStandardItemModel *navigationModel = nullptr;
    QStandardItemModel *listModel = nullptr;
    QStandardItemModel *tableModel = nullptr;
    QStandardItemModel *treeModel = nullptr;
    ZzFluentUI::ZzCalendarPicker *datePicker = nullptr;
    ZzFluentUI::ZzCalendar *calendar = nullptr;
};

} // namespace ZzExamples
