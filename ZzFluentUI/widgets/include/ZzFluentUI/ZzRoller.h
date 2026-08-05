#pragma once

#include <memory>

#include <QtCore/QStringList>
#include <QtWidgets/QSpinBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

QT_BEGIN_NAMESPACE
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace ZzFluentUI {

class ZzRollerPrivate;

/**
 * @brief 以标准 SpinBox 索引语义展示固定可见行的 Fluent 滚轮。
 *
 * QSpinBox 的 value/range 是当前行的唯一真值。控件只绘制当前行附近
 * 的固定数量文本，不维护像素滚动状态，也不创建动画或计时器。
 */
class ZZ_FLUENT_UI_EXPORT ZzRoller final : public QSpinBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRoller)
    Q_PROPERTY(QStringList items READ items WRITE setItems
                   NOTIFY itemsChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex
                   WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText
                   NOTIFY currentTextChanged)
    Q_PROPERTY(int itemHeight READ itemHeight WRITE setItemHeight
                   NOTIFY itemHeightChanged)
    Q_PROPERTY(int visibleItemCount READ visibleItemCount
                   WRITE setVisibleItemCount
                       NOTIFY visibleItemCountChanged)

public:
    /**
     * @brief 创建默认高 36px、显示 5 行且集合为空的滚轮。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzRoller(QWidget *parent = nullptr);

    /** @brief 销毁一次性私有状态和 QSpinBox 内部编辑器。 */
    ~ZzRoller() override;

    /**
     * @brief 一次性替换全部文本项并收敛当前索引。
     * @param items 新的值语义文本快照；重复文本和空文本均合法。
     */
    void setItems(QStringList items);

    /** @brief 返回当前全部文本项副本。 */
    [[nodiscard]] QStringList items() const;

    /** @brief 返回当前文本项数量。 */
    [[nodiscard]] int itemCount() const noexcept;

    /**
     * @brief 在末尾追加一条文本项。
     * @param text 待追加的展示文本。
     */
    void addItem(QString text);

    /**
     * @brief 在指定位置插入文本项并保留原选择的逻辑项。
     * @param index 允许取 0 至 itemCount。
     * @param text 待插入的展示文本。
     * @return 位置有效并完成插入时返回 true。
     */
    [[nodiscard]] bool insertItem(int index, QString text);

    /**
     * @brief 删除指定位置的文本项并收敛当前索引。
     * @param index 允许取 0 至 itemCount - 1。
     * @return 位置有效并完成删除时返回 true。
     */
    [[nodiscard]] bool removeItem(int index);

    /**
     * @brief 改写指定位置的展示文本。
     * @param index 允许取 0 至 itemCount - 1。
     * @param text 新的展示文本。
     * @return 位置有效且文本实际变化时返回 true。
     */
    [[nodiscard]] bool setItemText(int index, QString text);

    /** @brief 清空全部文本项；集合已空时不发变化信号。 */
    void clearItems();

    /**
     * @brief 返回指定位置的展示文本。
     * @param index 从零开始的逻辑行号。
     * @return 行号有效时返回文本，否则返回空字符串。
     */
    [[nodiscard]] QString itemText(int index) const;

    /**
     * @brief 设置当前逻辑行；无效行号保持原值。
     * @param index 非空集合中的有效行号。
     */
    void setCurrentIndex(int index);

    /** @brief 返回当前逻辑行；空集合返回 -1。 */
    [[nodiscard]] int currentIndex() const noexcept;

    /**
     * @brief 选择第一条完全匹配的文本项。
     * @param text 要匹配的完整文本。
     * @return 找到文本时返回 true，否则保持原值并返回 false。
     */
    [[nodiscard]] bool setCurrentText(const QString &text);

    /** @brief 返回当前行文本；空集合返回空字符串。 */
    [[nodiscard]] QString currentText() const;

    /**
     * @brief 设置单行逻辑高度。
     * @param height 自动收敛到 24 至 96。
     */
    void setItemHeight(int height);

    /** @brief 返回单行逻辑高度。 */
    [[nodiscard]] int itemHeight() const noexcept;

    /**
     * @brief 设置同时绘制的奇数行数量。
     * @param count 自动收敛到 3 至 9，偶数向上取奇数。
     */
    void setVisibleItemCount(int count);

    /** @brief 返回 3 至 9 范围内的奇数可见行数。 */
    [[nodiscard]] int visibleItemCount() const noexcept;

    /** @brief 返回由最长文本缓存和固定可见行计算的稳定尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回不小于 96px 且能容纳可见行的最小尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /** @brief 文本集合发生有效替换、插入、删除或改写后发出。 */
    void itemsChanged();

    /**
     * @brief 当前逻辑行实际变化后发出。
     * @param index 新行号；集合变空时为 -1。
     */
    void currentIndexChanged(int index);

    /**
     * @brief 当前展示文本实际变化后发出。
     * @param text 新文本；集合变空时为空字符串。
     */
    void currentTextChanged(const QString &text);

    /**
     * @brief 单行高度实际变化后发出。
     * @param height 收敛后的逻辑像素高度。
     */
    void itemHeightChanged(int height);

    /**
     * @brief 可见行数实际变化后发出。
     * @param count 收敛后的奇数行数。
     */
    void visibleItemCountChanged(int count);

    /**
     * @brief 用户通过键盘、滚轮、点击或拖动完成一次有效选择后发出。
     * @param index 最终逻辑行号。
     * @param text 最终展示文本。
     */
    void activated(int index, const QString &text);

protected:
    /** @brief 把 QSpinBox 数值映射为对应文本，供编辑器和无障碍使用。 */
    [[nodiscard]] QString textFromValue(int value) const override;

    /** @brief 把完全匹配文本映射为首个逻辑行。 */
    [[nodiscard]] int valueFromText(const QString &text) const override;

    /** @brief 绘制标准 SpinBox 外框和固定数量的滚轮文本行。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 处理离散导航键并把确认/取消键传播给父 popup。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 累积标准滚轮增量并执行无动画离散步进。 */
    void wheelEvent(QWheelEvent *event) override;

    /** @brief 开始左键点击或离散拖动事务。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 根据跨越的完整行数更新拖动选择。 */
    void mouseMoveEvent(QMouseEvent *event) override;

    /** @brief 完成点击或拖动并最多发出一次用户意图。 */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /** @brief 清理 hover 行，不创建延迟任务。 */
    void leaveEvent(QEvent *event) override;

    /** @brief 在字体、样式或 palette 变化后刷新尺寸缓存。 */
    void changeEvent(QEvent *event) override;

private:
    friend class ZzRollerPrivate;
    std::unique_ptr<ZzRollerPrivate> d_ptr;
};

} // namespace ZzFluentUI
