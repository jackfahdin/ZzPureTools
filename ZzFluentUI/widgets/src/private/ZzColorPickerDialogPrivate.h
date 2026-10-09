#pragma once
#include <QtGui/QColor>
#include "ZzWidgetTheme.h"
class QLabel;
class QWidget;
namespace ZzFluentUI {
class ZzColorPicker;
class ZzColorPickerDialog;
class ZzPushButton;

/** @brief 管理对话框固定装配和每次打开的颜色事务。 */
class ZzColorPickerDialogPrivate final
{
public:
    /** @brief 创建标题、唯一选择器和等宽页脚按钮。 */
    explicit ZzColorPickerDialogPrivate(ZzColorPickerDialog *q);
    /** @brief 更新本地化按钮和默认标题。 */
    void refreshText();
    ZzColorPickerDialog *const q_ptr;
    ZzWidgetTheme theme;
    ZzColorPicker *const picker;
    QLabel *const titleLabel;
    QWidget *const footer;
    ZzPushButton *const acceptButton;
    ZzPushButton *const cancelButton;
    QColor initialColor;
    QColor lastPreview;
    bool finishing = false;
    bool suppressPreview = false;
    bool defaultTitle = true;
};
} // namespace ZzFluentUI
