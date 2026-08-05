#pragma once

#include <limits>

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace ZzFluentUI {

class ZzRoller;

/** @brief 保存滚轮值快照、尺寸缓存和短生命周期输入状态。 */
class ZzRollerPrivate final
{
public:
    /**
     * @brief 初始化无按钮 SpinBox、空范围和固定垂直尺寸策略。
     * @param q 非空、非拥有的公开滚轮。
     */
    explicit ZzRollerPrivate(ZzRoller *q);

    /** @brief 返回行号是否落在当前文本集合中。 */
    [[nodiscard]] bool isValidIndex(int index) const noexcept;

    /** @brief 按 wrapping 设置把离散步数映射为最终有效行。 */
    [[nodiscard]] int steppedIndex(int start, int steps) const noexcept;

    /** @brief 应用一次用户行变化，实际变化时返回 true。 */
    [[nodiscard]] bool applyUserStep(int steps);

    /** @brief 应用一次用户绝对行变化，实际变化时返回 true。 */
    [[nodiscard]] bool applyUserIndex(int index);

    /** @brief 重建最长文本像素宽度缓存并通知布局。 */
    void refreshTextWidth();

    /** @brief 当前展示文本变化时发送一次自定义通知。 */
    void notifyCurrentTextIfNeeded();

    /** @brief 把局部纵坐标映射为中心行相对偏移。 */
    [[nodiscard]] int rowOffsetAt(int y) const noexcept;

    ZzRoller *const q_ptr;
    QStringList items;
    QString lastCurrentText;
    int itemHeight = 36;
    int visibleItemCount = 5;
    int longestTextWidth = 0;
    int wheelRemainder = 0;
    int hoverOffset = std::numeric_limits<int>::max();
    int dragStartY = 0;
    int dragStartIndex = -1;
    bool dragging = false;
    bool dragMoved = false;
};

} // namespace ZzFluentUI
