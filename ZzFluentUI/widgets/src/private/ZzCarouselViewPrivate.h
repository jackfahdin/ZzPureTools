#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QRect>
#include <QtGui/QAccessible>

class QAbstractItemModel;
class QPainter;
class QStyleOptionViewItem;
class QToolButton;
class QVariantAnimation;

namespace ZzFluentUI {

class ZzCarouselView;

/** @brief 持有轮播视图的固定数量呈现状态和模型观察连接。 */
class ZzCarouselViewPrivate final {
public:
  /** @brief 创建两个箭头按钮、一个 delegate 和一个持久动画。 */
  explicit ZzCarouselViewPrivate(ZzCarouselView *q);

  /** @brief 停止动画并断开外部 model 观察连接。 */
  ~ZzCarouselViewPrivate();

  /** @brief 返回 viewport 中排除指示器边距后的 item 内容区。 */
  [[nodiscard]] QRect contentRect() const;

  /** @brief 返回当前 root 下的行数。 */
  [[nodiscard]] int rowCount() const;

  /** @brief 返回指定 row 的第 0 列索引，越界时返回无效索引。 */
  [[nodiscard]] QModelIndex indexForRow(int row) const;

  /** @brief 判断索引是否属于当前 model、root 和第 0 列。 */
  [[nodiscard]] bool isRootItem(const QModelIndex &index) const;

  /** @brief 返回唯一当前项的派生行号。 */
  [[nodiscard]] int currentRow() const noexcept;

  /** @brief 安装 model 观察连接，不改变 model 所有权。 */
  void connectModel(QAbstractItemModel *model);

  /** @brief 断开上一 model 的全部观察连接。 */
  void disconnectModel();

  /** @brief 在当前索引无效且 model 非空时选择第 0 行。 */
  void initializeCurrent();

  /** @brief 只在派生 row 实际变化时发公开信号。 */
  void synchronizeCurrentRow();

  /** @brief 选择指定行，可选择是否要求 item enabled。 */
  [[nodiscard]] bool navigateTo(int row, int direction, bool requireEnabled);

  /** @brief 按正负一个 row 导航并处理环绕。 */
  [[nodiscard]] bool navigateBy(int delta);

  /** @brief 根据新旧当前索引启动或直接完成唯一过渡。 */
  void startTransition(const QModelIndex &current, const QModelIndex &previous);

  /** @brief 停止动画、释放旧索引并同步终态。 */
  void finishTransition() noexcept;

  /** @brief 绘制背景、最多两个 item 和最多七个指示点。 */
  void paint(QPainter *painter) const;

  /** @brief 根据当前状态更新两个箭头按钮。 */
  void updateButtons();

  /** @brief 根据 viewport 和布局方向放置两个固定按钮。 */
  void updateButtonGeometry();

  /** @brief 更新按钮的标准方向图标。 */
  void updateButtonIcons();

  /** @brief 更新按钮 tooltip 与 accessible name。 */
  void updateButtonText();

  /** @brief 构造指定索引和矩形对应的标准 delegate option。 */
  [[nodiscard]] QStyleOptionViewItem itemOption(const QModelIndex &index,
                                                const QRect &rect) const;

  /** @brief 调用索引对应 delegate 绘制一个 item。 */
  void paintIndex(QPainter *painter, const QModelIndex &index,
                  const QRect &rect) const;

  /** @brief 以固定上限绘制当前位置指示点。 */
  void paintIndicators(QPainter *painter) const;

  ZzCarouselView *const q_ptr;
  QToolButton *const previousButton;
  QToolButton *const nextButton;
  QVariantAnimation *const animation;
  QPersistentModelIndex previousIndex;
  QList<QMetaObject::Connection> modelConnections;
  qreal transitionProgress = 1.0;
  int transitionDirection = 1;
  int pendingDirection = 0;
  int lastReportedRow = -1;
  int animationDurationMilliseconds = 220;
  bool wrapAroundEnabled = false;
  bool changingModelContext = false;
#if QT_CONFIG(accessibility)
  /** @brief 轮播视图专用无障碍接口在 Qt 全局缓存中的标识。 */
  QAccessible::Id accessibleInterfaceId = 0;
#endif
};

} // namespace ZzFluentUI
