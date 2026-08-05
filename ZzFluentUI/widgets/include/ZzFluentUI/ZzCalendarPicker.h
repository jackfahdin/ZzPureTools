#pragma once

#include <memory>

#include <QtWidgets/QDateEdit>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzCalendar;
class ZzCalendarPickerPrivate;

/**
 * @brief 使用 ZzCalendar 弹层的 Fluent 日期编辑器。
 *
 * 输入法、文本选择、步进、校验、日期范围和可访问 SpinBox 语义
 * 由 QDateEdit 提供；本类只固定默认弹层实现。
 */
class ZZ_FLUENT_UI_EXPORT ZzCalendarPicker final : public QDateEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzCalendarPicker)

public:
    /**
     * @brief 创建可键盘编辑并可弹出日历的日期选择器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzCalendarPicker(QWidget *parent = nullptr);

    /** @brief 销毁私有装配状态，日历由 QDateEdit 所有。 */
    ~ZzCalendarPicker() override;

    /**
     * @brief 返回当前唯一的 Zz 日历弹层。
     * @return 非空、非拥有指针，生命周期不超过本选择器。
     */
    [[nodiscard]] ZzCalendar *calendar() const noexcept;

private:
    std::unique_ptr<ZzCalendarPickerPrivate> d_ptr;
};

} // namespace ZzFluentUI
