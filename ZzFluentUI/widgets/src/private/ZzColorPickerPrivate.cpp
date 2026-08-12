#include "ZzColorPickerPrivate.h"

#include <algorithm>
#include <utility>

#include <QtCore/QAbstractListModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtGui/QPainter>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyleOptionViewItem>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzSpinBox.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int ZzMaximumPaletteColors = 256;
constexpr int ZzVisiblePaletteRows = 3;
constexpr int ZzChannelEditorWidth = 84;
constexpr int ZzSectionSpacing = 10;

/** @brief 把有效颜色规范化到 8 位 RGBA。 */
QColor zzNormalizedColor(const QColor &color)
{
    return color.isValid()
        ? QColor::fromRgba(color.rgba())
        : QColor{};
}

/** @brief 返回适合色板和无障碍展示的确定性颜色文本。 */
QString zzColorText(const QColor &color)
{
    const QColor::NameFormat format = color.alpha() < 255
        ? QColor::HexArgb
        : QColor::HexRgb;
    return color.name(format).toUpper();
}

/** @brief 过滤无效和重复 RGBA 值并限制色板大小。 */
QList<QColor> zzNormalizedPalette(QList<QColor> colors)
{
    QList<QColor> result;
    result.reserve(std::min(
        colors.size(),
        static_cast<qsizetype>(ZzMaximumPaletteColors)));
    QSet<QRgb> seen;
    for (const QColor &candidate : std::as_const(colors)) {
        const QColor color = zzNormalizedColor(candidate);
        if (!color.isValid() || seen.contains(color.rgba())) {
            continue;
        }
        seen.insert(color.rgba());
        result.append(color);
        if (result.size() == ZzMaximumPaletteColors) {
            break;
        }
    }
    return result;
}

} // namespace

/** @brief 保存唯一色板集合并暴露颜色和无障碍展示角色。 */
class ZzColorPaletteModel final : public QAbstractListModel
{
public:
    enum ZzRole : int
    {
        ZzColorRole = Qt::UserRole + 1,
    };

    /** @brief 创建空色板模型。 */
    explicit ZzColorPaletteModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    /** @brief 返回根索引下的色板项数。 */
    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(colors_.size());
    }

    /** @brief 返回颜色值和确定性文本角色。 */
    [[nodiscard]] QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0
            || index.row() >= colors_.size()) {
            return {};
        }
        const QColor color = colors_.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::AccessibleTextRole:
            return zzColorText(color);
        case ZzColorRole:
            return color;
        default:
            return {};
        }
    }

    /** @brief 一次 reset 替换全部颜色，重复集合不发送模型事件。 */
    [[nodiscard]] bool setColors(QList<QColor> colors)
    {
        if (colors_ == colors) {
            return false;
        }
        beginResetModel();
        colors_ = std::move(colors);
        endResetModel();
        return true;
    }

    /** @brief 返回隐式共享颜色列表快照。 */
    [[nodiscard]] QList<QColor> colors() const
    {
        return colors_;
    }

    /** @brief 返回指定模型行颜色。 */
    [[nodiscard]] QColor colorAt(int row) const
    {
        return row >= 0 && row < colors_.size()
            ? colors_.at(row)
            : QColor{};
    }

    /** @brief 按 8 位 RGBA 查找首个完全匹配行。 */
    [[nodiscard]] int rowForColor(const QColor &color) const noexcept
    {
        const QRgb rgba = color.rgba();
        for (int row = 0; row < colors_.size(); ++row) {
            if (colors_.at(row).rgba() == rgba) {
                return row;
            }
        }
        return -1;
    }

private:
    QList<QColor> colors_;
};

/** @brief 只绘制当前可见色板 index 的主题边框和内容色。 */
class ZzColorSwatchDelegate final : public QStyledItemDelegate
{
public:
    /** @brief 绑定非拥有颜色选择器状态。 */
    ZzColorSwatchDelegate(
        ZzColorPickerPrivate *owner,
        QObject *parent)
        : QStyledItemDelegate(parent)
        , owner_(owner)
    {
        Q_ASSERT(owner_ != nullptr);
    }

    /** @brief 绘制内容色块、边框和非纯颜色选择反馈。 */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        if (painter == nullptr || !index.isValid()) {
            return;
        }
        const auto snapshot = owner_->theme.snapshot();
        const int extent = qMax(
            1,
            qCeil(snapshot->metric(ZzMetricToken::ColorSwatchExtent)));
        const int side = std::min(
            {extent, option.rect.width(), option.rect.height()});
        QRect swatch(
            option.rect.center().x() - side / 2,
            option.rect.center().y() - side / 2,
            side,
            side);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool focused = option.state.testFlag(QStyle::State_HasFocus);
        const qreal radius = snapshot->metric(
            ZzMetricToken::CornerRadiusSmall);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(snapshot->color(ZzColorToken::SurfaceSecondary));
        painter->drawRoundedRect(swatch, radius, radius);
        painter->setBrush(
            index.data(ZzColorPaletteModel::ZzColorRole).value<QColor>());
        painter->drawRoundedRect(swatch, radius, radius);
        const QColor stroke = selected || focused
            ? snapshot->color(ZzColorToken::FocusStroke)
            : snapshot->color(ZzColorToken::ControlStroke);
        const qreal strokeWidth = selected || focused
            ? snapshot->metric(ZzMetricToken::FocusStrokeWidth)
            : snapshot->metric(ZzMetricToken::StrokeThin);
        QPen pen(stroke, strokeWidth);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        const qreal inset = strokeWidth / 2.0;
        painter->drawRoundedRect(
            QRectF(swatch).adjusted(inset, inset, -inset, -inset),
            radius,
            radius);
        painter->restore();
    }

    /** @brief 返回具名色块边长与间距之和。 */
    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &,
        const QModelIndex &) const override
    {
        const auto snapshot = owner_->theme.snapshot();
        const int side = qMax(
            1,
            qCeil(snapshot->metric(ZzMetricToken::ColorSwatchExtent)
                  + snapshot->metric(ZzMetricToken::ColorSwatchGap)));
        return {side, side};
    }

private:
    ZzColorPickerPrivate *const owner_;
};

/** @brief 用主题棋盘和唯一当前颜色展示 alpha 合成结果。 */
class ZzColorPreviewWidget final : public QWidget
{
public:
    /** @brief 绑定非拥有颜色选择器状态。 */
    ZzColorPreviewWidget(
        ZzColorPickerPrivate *owner,
        QWidget *parent)
        : QWidget(parent)
        , owner_(owner)
    {
        Q_ASSERT(owner_ != nullptr);
        setObjectName(QStringLiteral("zzColorPreview"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    /** @brief 返回不随内容变化的稳定预览尺寸。 */
    [[nodiscard]] QSize sizeHint() const override
    {
        return {160, 52};
    }

protected:
    /** @brief 以矩形块绘制主题棋盘、当前颜色和外框。 */
    void paintEvent(QPaintEvent *) override
    {
        const auto snapshot = owner_->theme.snapshot();
        QPainter painter(this);
        const QRect content = rect().adjusted(1, 1, -1, -1);
        constexpr int tileExtent = 8;
        for (int y = content.top(); y <= content.bottom(); y += tileExtent) {
            for (int x = content.left(); x <= content.right(); x += tileExtent) {
                const bool alternate =
                    ((x - content.left()) / tileExtent
                     + (y - content.top()) / tileExtent)
                    % 2 != 0;
                painter.fillRect(
                    QRect(x, y, tileExtent, tileExtent).intersected(content),
                    snapshot->color(
                        alternate
                            ? ZzColorToken::SurfaceSecondary
                            : ZzColorToken::Surface));
            }
        }
        painter.fillRect(content, owner_->currentColor);
        QPen pen(
            snapshot->color(ZzColorToken::ControlStroke),
            snapshot->metric(ZzMetricToken::StrokeThin));
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

private:
    ZzColorPickerPrivate *const owner_;
};

ZzColorPickerPrivate::ZzColorPickerPrivate(ZzColorPicker *q)
    : q_ptr(q)
    , theme(q)
    , paletteModel(new ZzColorPaletteModel(q))
    , paletteView(new QListView(q))
    , swatchDelegate(new ZzColorSwatchDelegate(this, paletteView))
    , preview(new ZzColorPreviewWidget(this, q))
    , redLabel(new QLabel(q))
    , greenLabel(new QLabel(q))
    , blueLabel(new QLabel(q))
    , alphaLabel(new QLabel(q))
    , hexLabel(new QLabel(q))
    , redSpinBox(new ZzSpinBox(q))
    , greenSpinBox(new ZzSpinBox(q))
    , blueSpinBox(new ZzSpinBox(q))
    , alphaSpinBox(new ZzSpinBox(q))
    , hexEditor(new QLineEdit(q))
    , hexValidator(new QRegularExpressionValidator(q))
{
    Q_ASSERT(q_ptr != nullptr);
    paletteView->setObjectName(QStringLiteral("zzColorPaletteView"));
    redSpinBox->setObjectName(QStringLiteral("zzRedSpinBox"));
    greenSpinBox->setObjectName(QStringLiteral("zzGreenSpinBox"));
    blueSpinBox->setObjectName(QStringLiteral("zzBlueSpinBox"));
    alphaSpinBox->setObjectName(QStringLiteral("zzAlphaSpinBox"));
    hexEditor->setObjectName(QStringLiteral("zzHexColorEditor"));

    paletteView->setModel(paletteModel);
    paletteView->setItemDelegate(swatchDelegate);
    paletteView->setViewMode(QListView::IconMode);
    paletteView->setFlow(QListView::LeftToRight);
    paletteView->setWrapping(true);
    paletteView->setResizeMode(QListView::Adjust);
    paletteView->setMovement(QListView::Static);
    paletteView->setUniformItemSizes(true);
    paletteView->setSelectionMode(QAbstractItemView::SingleSelection);
    paletteView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    paletteView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    for (ZzSpinBox *spinBox : {
             redSpinBox,
             greenSpinBox,
             blueSpinBox,
             alphaSpinBox}) {
        spinBox->setRange(0, 255);
        spinBox->setFixedWidth(ZzChannelEditorWidth);
        QObject::connect(
            spinBox,
            &QSpinBox::valueChanged,
            q_ptr,
            [this] {
                commitChannelEditors();
            });
    }
    hexEditor->setValidator(hexValidator);
    hexEditor->setClearButtonEnabled(false);
    QObject::connect(
        hexEditor,
        &QLineEdit::editingFinished,
        q_ptr,
        [this] {
            commitHexEditor();
        });

    const auto applyPaletteIndex = [this](const QModelIndex &index) {
        if (syncing || !index.isValid()) {
            return;
        }
        q_ptr->setCurrentColor(paletteModel->colorAt(index.row()));
    };
    QObject::connect(
        paletteView,
        &QListView::clicked,
        q_ptr,
        applyPaletteIndex);
    QObject::connect(
        paletteView,
        &QListView::activated,
        q_ptr,
        applyPaletteIndex);
    QObject::connect(
        paletteView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        q_ptr,
        [applyPaletteIndex](
            const QModelIndex &current,
            const QModelIndex &) {
            applyPaletteIndex(current);
        });

    auto *layout = new QVBoxLayout(q_ptr);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(ZzSectionSpacing);
    layout->addWidget(preview);
    layout->addWidget(paletteView);
    auto *editorLayout = new QGridLayout;
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setHorizontalSpacing(8);
    editorLayout->setVerticalSpacing(4);
    editorLayout->addWidget(redLabel, 0, 0);
    editorLayout->addWidget(greenLabel, 0, 1);
    editorLayout->addWidget(blueLabel, 0, 2);
    editorLayout->addWidget(alphaLabel, 0, 3);
    editorLayout->addWidget(redSpinBox, 1, 0);
    editorLayout->addWidget(greenSpinBox, 1, 1);
    editorLayout->addWidget(blueSpinBox, 1, 2);
    editorLayout->addWidget(alphaSpinBox, 1, 3);
    editorLayout->setColumnStretch(4, 1);
    layout->addLayout(editorLayout);
    layout->addWidget(hexLabel);
    layout->addWidget(hexEditor);

    static_cast<void>(paletteModel->setColors(defaultPaletteColors()));
    refreshAccessibleText();
    syncAlphaPresentation();
    refreshTheme();
}

ZzColorPickerPrivate::~ZzColorPickerPrivate() = default;

bool ZzColorPickerPrivate::applyCurrentColor(QColor color)
{
    color = zzNormalizedColor(color);
    if (!color.isValid() || color.rgba() == currentColor.rgba()) {
        return false;
    }
    currentColor = color;
    syncDerivedState();
    return true;
}

bool ZzColorPickerPrivate::applyPaletteColors(QList<QColor> colors)
{
    const bool changed = paletteModel->setColors(
        zzNormalizedPalette(std::move(colors)));
    if (changed) {
        syncDerivedState();
    }
    return changed;
}

QList<QColor> ZzColorPickerPrivate::paletteColors() const
{
    return paletteModel->colors();
}

int ZzColorPickerPrivate::paletteColorCount() const noexcept
{
    return paletteModel->rowCount();
}

QList<QColor> ZzColorPickerPrivate::defaultPaletteColors()
{
    static const QList<QColor> colors{
        QColor::fromRgb(0, 120, 212),
        QColor::fromRgb(0, 90, 158),
        QColor::fromRgb(43, 136, 216),
        QColor::fromRgb(80, 230, 255),
        QColor::fromRgb(16, 124, 16),
        QColor::fromRgb(73, 130, 5),
        QColor::fromRgb(0, 183, 195),
        QColor::fromRgb(3, 131, 135),
        QColor::fromRgb(255, 185, 0),
        QColor::fromRgb(247, 99, 12),
        QColor::fromRgb(209, 52, 56),
        QColor::fromRgb(232, 17, 35),
        QColor::fromRgb(136, 23, 152),
        QColor::fromRgb(194, 57, 179),
        QColor::fromRgb(135, 100, 184),
        QColor::fromRgb(92, 45, 145),
        QColor::fromRgb(142, 86, 46),
        QColor::fromRgb(202, 80, 16),
        QColor::fromRgb(105, 121, 126),
        QColor::fromRgb(76, 74, 72),
        QColor::fromRgb(255, 255, 255),
        QColor::fromRgb(210, 208, 206),
        QColor::fromRgb(96, 94, 92),
        QColor::fromRgb(0, 0, 0)};
    return colors;
}

void ZzColorPickerPrivate::syncDerivedState()
{
    const bool wasSyncing = syncing;
    syncing = true;
    const QSignalBlocker redBlocker(redSpinBox);
    const QSignalBlocker greenBlocker(greenSpinBox);
    const QSignalBlocker blueBlocker(blueSpinBox);
    const QSignalBlocker alphaBlocker(alphaSpinBox);
    const QSignalBlocker hexBlocker(hexEditor);
    redSpinBox->setValue(currentColor.red());
    greenSpinBox->setValue(currentColor.green());
    blueSpinBox->setValue(currentColor.blue());
    alphaSpinBox->setValue(currentColor.alpha());
    hexEditor->setText(currentColor.name(
        alphaEnabled ? QColor::HexArgb : QColor::HexRgb).toUpper());

    const int row = paletteModel->rowForColor(currentColor);
    if (row >= 0) {
        paletteView->setCurrentIndex(paletteModel->index(row, 0));
    } else {
        paletteView->clearSelection();
        paletteView->setCurrentIndex({});
    }
    preview->update();
    paletteView->viewport()->update();
    syncing = wasSyncing;
}

void ZzColorPickerPrivate::commitChannelEditors()
{
    if (syncing) {
        return;
    }
    QColor color = currentColor;
    color.setRed(redSpinBox->value());
    color.setGreen(greenSpinBox->value());
    color.setBlue(blueSpinBox->value());
    if (alphaEnabled) {
        color.setAlpha(alphaSpinBox->value());
    }
    q_ptr->setCurrentColor(color);
}

void ZzColorPickerPrivate::commitHexEditor()
{
    if (syncing) {
        return;
    }
    const QString text = hexEditor->text();
    const int expectedLength = alphaEnabled ? 9 : 7;
    const QColor parsed = QColor::fromString(text);
    if (text.size() != expectedLength || !parsed.isValid()) {
        syncDerivedState();
        return;
    }
    QColor color = parsed;
    if (!alphaEnabled) {
        color.setAlpha(currentColor.alpha());
    }
    q_ptr->setCurrentColor(color);
    syncDerivedState();
}

void ZzColorPickerPrivate::syncAlphaPresentation()
{
    alphaLabel->setVisible(alphaEnabled);
    alphaSpinBox->setVisible(alphaEnabled);
    hexValidator->setRegularExpression(QRegularExpression(
        alphaEnabled
            ? QStringLiteral("^#[0-9A-Fa-f]{8}$")
            : QStringLiteral("^#[0-9A-Fa-f]{6}$")));
    syncDerivedState();
}

void ZzColorPickerPrivate::refreshAccessibleText()
{
    redLabel->setText(ZzColorPicker::tr("红色"));
    greenLabel->setText(ZzColorPicker::tr("绿色"));
    blueLabel->setText(ZzColorPicker::tr("蓝色"));
    alphaLabel->setText(ZzColorPicker::tr("透明度"));
    hexLabel->setText(ZzColorPicker::tr("十六进制颜色"));
    paletteView->setAccessibleName(ZzColorPicker::tr("颜色色板"));
    preview->setAccessibleName(ZzColorPicker::tr("当前颜色预览"));
    redSpinBox->setAccessibleName(redLabel->text());
    greenSpinBox->setAccessibleName(greenLabel->text());
    blueSpinBox->setAccessibleName(blueLabel->text());
    alphaSpinBox->setAccessibleName(alphaLabel->text());
    hexEditor->setAccessibleName(hexLabel->text());
}

void ZzColorPickerPrivate::refreshTheme()
{
    theme.refreshFallback();
    syncPaletteMetrics();
    preview->update();
    paletteView->viewport()->update();
}

void ZzColorPickerPrivate::syncPaletteMetrics()
{
    const auto snapshot = theme.snapshot();
    const int gridExtent = qMax(
        1,
        qCeil(snapshot->metric(ZzMetricToken::ColorSwatchExtent)
              + snapshot->metric(ZzMetricToken::ColorSwatchGap)));
    paletteView->setGridSize({gridExtent, gridExtent});
    paletteView->setFixedHeight(
        ZzVisiblePaletteRows * gridExtent
        + 2 * paletteView->frameWidth());
    paletteView->doItemsLayout();
}

} // namespace ZzFluentUI
