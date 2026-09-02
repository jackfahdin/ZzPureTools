#include "ZzRollerPickerPrivate.h"

#include <algorithm>
#include <functional>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <QtGui/QGuiApplication>
#include <QtGui/QFontMetrics>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzRoller.h>

namespace ZzFluentUI {

namespace {

constexpr int ZzMinimumColumnWidth = 64;
constexpr int ZzMaximumColumnWidth = 512;
constexpr int ZzPickerVisibleItems = 7;
constexpr int ZzPopupMargin = 8;
constexpr int ZzPopupSpacing = 8;
constexpr int ZzRollerColumnDividerSpacing = 1;
constexpr char ZzForceNonGrabbingEnvironment[] =
    "ZZ_FLUENTUI_ROLLER_PICKER_FORCE_NON_GRABBING";

/** @brief 判断当前平台是否需要绕过 Wayland 的 Popup 输入抓取。 */
[[nodiscard]] bool zzUseNonGrabbingRollerPicker()
{
    const QByteArray forced = qgetenv(ZzForceNonGrabbingEnvironment);
    if (!forced.isEmpty()) {
        return forced != QByteArrayLiteral("0")
            && forced.compare(QByteArrayLiteral("false"), Qt::CaseInsensitive)
                != 0;
    }
    return QGuiApplication::platformName().startsWith(
        QStringLiteral("wayland"),
        Qt::CaseInsensitive);
}

[[nodiscard]] Qt::WindowFlags zzRollerPickerWindowFlags()
{
    if (zzUseNonGrabbingRollerPicker()) {
        return Qt::Tool
            | Qt::FramelessWindowHint
            | Qt::NoDropShadowWindowHint;
    }
    return Qt::Popup;
}

[[nodiscard]] QString zzUniqueColumnKey(QSet<QString> *usedKeys)
{
    QString key;
    do {
        key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } while (usedKeys->contains(key));
    return key;
}

[[nodiscard]] int zzColumnItemCount(const ZzRollerColumn &column)
{
    return static_cast<int>(column.items.size());
}

} // namespace

/** @brief 使用标准菜单面板绘制并把隐藏原因回传给事务管理器。 */
class ZzRollerPickerPopup final : public QFrame
{
public:
    explicit ZzRollerPickerPopup(QWidget *parent)
        : QFrame(parent, zzRollerPickerWindowFlags())
        , nonGrabbing(windowType() != Qt::Popup)
    {
        setFrameShape(QFrame::NoFrame);
        setFocusPolicy(Qt::StrongFocus);
        setAutoFillBackground(false);
    }

    /** @brief 在非抓取模式下开始接收应用级关闭事件。 */
    void beginInteraction()
    {
        if (!nonGrabbing || eventFilterInstalled) {
            return;
        }
        QCoreApplication *application = QCoreApplication::instance();
        if (application == nullptr) {
            return;
        }
        application->installEventFilter(this);
        eventFilterInstalled = true;
    }

    /** @brief 停止接收应用级关闭事件，避免常驻过滤开销。 */
    void endInteraction()
    {
        if (!eventFilterInstalled) {
            return;
        }
        if (QCoreApplication *application = QCoreApplication::instance();
            application != nullptr) {
            application->removeEventFilter(this);
        }
        eventFilterInstalled = false;
    }

    std::function<void()> acceptRequested;
    std::function<void()> cancelRequested;
    std::function<void()> hidden;

protected:
    /** @brief 在非抓取窗口中复刻 Popup 的外部关闭语义。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!nonGrabbing || !eventFilterInstalled || !isVisible()
            || event == nullptr) {
            return QFrame::eventFilter(watched, event);
        }

        if (event->type() == QEvent::KeyPress) {
            const auto *keyEvent = static_cast<const QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                endInteraction();
                if (cancelRequested) {
                    cancelRequested();
                }
                return true;
            }
        }

        QWidget *watchedWidget = qobject_cast<QWidget *>(watched);
        const bool insidePopup = watchedWidget != nullptr
            && (watchedWidget == this || isAncestorOf(watchedWidget));
        if (event->type() == QEvent::MouseButtonPress && !insidePopup) {
            endInteraction();
            if (cancelRequested) {
                cancelRequested();
            }
            return true;
        }

        if (event->type() == QEvent::WindowDeactivate && insidePopup) {
            endInteraction();
            if (cancelRequested) {
                cancelRequested();
            }
        }
        return QFrame::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setClipRegion(event->region());
        QStyleOption option;
        option.initFrom(this);
        option.rect = rect();
        // PE_PanelMenu is the single Fluent surface for the popup.  Child
        // rollers and buttons paint only their contents on top of it.
        style()->drawPrimitive(
            QStyle::PE_PanelMenu,
            &option,
            &painter,
            this);
        // One subtle divider per column boundary; rollers themselves are
        // content-only and therefore do not contribute competing frames.
        const QList<ZzRoller *> columns = findChildren<ZzRoller *>();
        if (!columns.isEmpty()) {
            const QPen pen(palette().color(QPalette::Midlight));
            painter.setPen(pen);
            for (int i = 1; i < columns.size(); ++i) {
                const QRect previous = QRect(
                    columns.at(i - 1)->mapTo(this, QPoint(0, 0)),
                    columns.at(i - 1)->size());
                const QRect current = QRect(
                    columns.at(i)->mapTo(this, QPoint(0, 0)),
                    columns.at(i)->size());
                const int x = (previous.right() + current.left()) / 2;
                painter.drawLine(x, previous.top(), x, previous.bottom());
            }
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            event->accept();
            if (cancelRequested) {
                cancelRequested();
            }
            return;
        }
        if (event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Return) {
            event->accept();
            if (acceptRequested) {
                acceptRequested();
            }
            return;
        }
        QFrame::keyPressEvent(event);
    }

    void hideEvent(QHideEvent *event) override
    {
        endInteraction();
        QFrame::hideEvent(event);
        if (hidden) {
            hidden();
        }
    }

private:
    const bool nonGrabbing;
    bool eventFilterInstalled = false;
};

ZzRollerPickerPrivate::ZzRollerPickerPrivate(ZzRollerPicker *q)
    : q_ptr(q)
    , popup(new ZzRollerPickerPopup(q))
    , rollerHost(new QWidget(popup))
    , rollerLayout(new QHBoxLayout(rollerHost))
    , popupLayout(new QVBoxLayout(popup))
    , buttonBox(new QDialogButtonBox(
          QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
          Qt::Horizontal,
          popup))
    , okButton(buttonBox->button(QDialogButtonBox::Ok))
    , cancelButton(buttonBox->button(QDialogButtonBox::Cancel))
{
    rollerHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    rollerLayout->setContentsMargins(0, 0, 0, 0);
    // Reserve a real pixel between columns for the single popup divider.
    rollerLayout->setSpacing(ZzRollerColumnDividerSpacing);
    popupLayout->setContentsMargins(
        ZzPopupMargin,
        ZzPopupMargin,
        ZzPopupMargin,
        ZzPopupMargin);
    popupLayout->setSpacing(ZzPopupSpacing);
    popupLayout->addWidget(rollerHost);
    popupLayout->addWidget(buttonBox);

    QObject::connect(
        q_ptr,
        &QPushButton::clicked,
        q_ptr,
        [this]() { showPopup(); });
    QObject::connect(
        buttonBox,
        &QDialogButtonBox::accepted,
        q_ptr,
        [this]() { acceptPopup(); });
    QObject::connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        q_ptr,
        [this]() { cancelPopup(); });

    popup->acceptRequested = [this]() { acceptPopup(); };
    popup->cancelRequested = [this]() { cancelPopup(); };
    popup->hidden = [this]() { handleExternalHide(); };
    popup->hide();
    refreshSummary();
}

ZzRollerPickerPrivate::~ZzRollerPickerPrivate()
{
    if (popup == nullptr) {
        return;
    }
    popup->acceptRequested = {};
    popup->cancelRequested = {};
    popup->hidden = {};
    delete popup;
    popup = nullptr;
}

bool ZzRollerPickerPrivate::setColumns(
    QList<ZzRollerColumn> replacement)
{
    replacement = normalizeColumns(std::move(replacement));
    if (columns == replacement) {
        return false;
    }
    if (popupActive) {
        cancelPopup();
    }

    const QList<int> oldIndexes = currentIndexes();
    const QStringList oldTexts = currentTexts();
    columns = std::move(replacement);
    rebuildRollers();
    refreshSummary();
    Q_EMIT q_ptr->columnsChanged();
    if (oldIndexes != currentIndexes() || oldTexts != currentTexts()) {
        emitSelectionChanged();
    }
    return true;
}

QString ZzRollerPickerPrivate::insertColumn(
    int index,
    ZzRollerColumn column)
{
    if (index < 0 || index > columns.size()) {
        return {};
    }
    if (popupActive) {
        cancelPopup();
    }

    QSet<QString> usedKeys;
    usedKeys.reserve(columns.size() + 1);
    for (const ZzRollerColumn &existing : std::as_const(columns)) {
        usedKeys.insert(existing.key);
    }
    if (column.key.isEmpty() || usedKeys.contains(column.key)) {
        column.key = zzUniqueColumnKey(&usedKeys);
    }
    column.minimumWidth = std::clamp(
        column.minimumWidth,
        ZzMinimumColumnWidth,
        ZzMaximumColumnWidth);
    column.currentIndex = column.items.isEmpty()
        ? -1
        : std::clamp(
              column.currentIndex,
              0,
              zzColumnItemCount(column) - 1);

    const QList<int> oldIndexes = currentIndexes();
    const QStringList oldTexts = currentTexts();
    const QString normalizedKey = column.key;
    columns.insert(index, std::move(column));
    rebuildRollers();
    refreshSummary();
    Q_EMIT q_ptr->columnsChanged();
    if (oldIndexes != currentIndexes() || oldTexts != currentTexts()) {
        emitSelectionChanged();
    }
    return normalizedKey;
}

bool ZzRollerPickerPrivate::removeColumnAt(int index)
{
    if (index < 0 || index >= columns.size()) {
        return false;
    }
    if (popupActive) {
        cancelPopup();
    }

    const QList<int> oldIndexes = currentIndexes();
    const QStringList oldTexts = currentTexts();
    columns.removeAt(index);
    rebuildRollers();
    refreshSummary();
    Q_EMIT q_ptr->columnsChanged();
    if (oldIndexes != currentIndexes() || oldTexts != currentTexts()) {
        emitSelectionChanged();
    }
    return true;
}

bool ZzRollerPickerPrivate::setColumnItems(
    int column,
    QStringList items)
{
    if (column < 0 || column >= columns.size()
        || columns.at(column).items == items) {
        return false;
    }
    if (popupActive) {
        cancelPopup();
    }

    const QList<int> oldIndexes = currentIndexes();
    const QStringList oldTexts = currentTexts();
    columns[column].items = std::move(items);
    columns[column].currentIndex = columns.at(column).items.isEmpty()
        ? -1
        : std::clamp(
              columns.at(column).currentIndex,
              0,
              zzColumnItemCount(columns.at(column)) - 1);

    suppressSelectionSignals = true;
    rollers.at(column)->setItems(columns.at(column).items);
    rollers.at(column)->setCurrentIndex(columns.at(column).currentIndex);
    columns[column].currentIndex = rollers.at(column)->currentIndex();
    suppressSelectionSignals = false;
    refreshSummary();
    Q_EMIT q_ptr->columnsChanged();
    if (oldIndexes != currentIndexes() || oldTexts != currentTexts()) {
        emitSelectionChanged();
    }
    return true;
}

bool ZzRollerPickerPrivate::setCurrentIndex(int column, int index)
{
    if (column < 0 || column >= columns.size()
        || index < 0 || index >= columns.at(column).items.size()
        || columns.at(column).currentIndex == index) {
        return false;
    }
    rollers.at(column)->setCurrentIndex(index);
    return true;
}

bool ZzRollerPickerPrivate::setCurrentIndexes(
    const QList<int> &indexes)
{
    QList<int> targets = currentIndexes();
    const int limit = std::min(
        static_cast<int>(indexes.size()),
        static_cast<int>(columns.size()));
    for (int column = 0; column < limit; ++column) {
        const int index = indexes.at(column);
        if (index >= 0 && index < columns.at(column).items.size()) {
            targets[column] = index;
        }
    }
    if (targets == currentIndexes()) {
        return false;
    }

    suppressSelectionSignals = true;
    for (int column = 0; column < targets.size(); ++column) {
        rollers.at(column)->setCurrentIndex(targets.at(column));
        columns[column].currentIndex = rollers.at(column)->currentIndex();
    }
    suppressSelectionSignals = false;
    refreshSummary();
    emitSelectionChanged();
    return true;
}

QList<int> ZzRollerPickerPrivate::currentIndexes() const
{
    QList<int> result;
    result.reserve(columns.size());
    for (const ZzRollerColumn &column : columns) {
        result.append(column.currentIndex);
    }
    return result;
}

QStringList ZzRollerPickerPrivate::currentTexts() const
{
    QStringList result;
    result.reserve(columns.size());
    for (const ZzRollerColumn &column : columns) {
        const bool valid = column.currentIndex >= 0
            && column.currentIndex < column.items.size();
        result.append(valid
                          ? column.items.at(column.currentIndex)
                          : QString{});
    }
    return result;
}

QString ZzRollerPickerPrivate::summaryText() const
{
    QStringList nonEmpty;
    const QStringList texts = currentTexts();
    nonEmpty.reserve(texts.size());
    for (const QString &text : texts) {
        if (!text.isEmpty()) {
            nonEmpty.append(text);
        }
    }
    return nonEmpty.join(QStringLiteral(" / "));
}

void ZzRollerPickerPrivate::showPopup()
{
    if (popupActive || !q_ptr->isEnabled() || columns.isEmpty()) {
        return;
    }
    openSnapshot = currentIndexes();
    popupActive = true;
    popup->setLayoutDirection(q_ptr->layoutDirection());
    popup->setAccessibleName(q_ptr->accessibleName());
    preparePopupGeometry();
    popup->show();
    popup->beginInteraction();
    Q_EMIT q_ptr->popupVisibleChanged(true);

    const auto firstSelectable = std::find_if(
        rollers.cbegin(),
        rollers.cend(),
        [](const ZzRoller *roller) { return roller->itemCount() > 0; });
    if (firstSelectable != rollers.cend()) {
        (*firstSelectable)->setFocus(Qt::PopupFocusReason);
    } else if (okButton != nullptr) {
        okButton->setFocus(Qt::PopupFocusReason);
    }
}

void ZzRollerPickerPrivate::acceptPopup()
{
    if (!popupActive || closingPopup) {
        return;
    }
    const QList<int> indexes = currentIndexes();
    const QStringList texts = currentTexts();
    closingPopup = true;
    popupActive = false;
    popup->endInteraction();
    popup->hide();
    openSnapshot.clear();
    closingPopup = false;
    q_ptr->setFocus(Qt::PopupFocusReason);
    Q_EMIT q_ptr->popupVisibleChanged(false);
    Q_EMIT q_ptr->selectionAccepted(indexes, texts);
}

void ZzRollerPickerPrivate::cancelPopup()
{
    if (!popupActive || closingPopup) {
        return;
    }
    restoreSnapshot();
    closingPopup = true;
    popupActive = false;
    popup->endInteraction();
    popup->hide();
    openSnapshot.clear();
    closingPopup = false;
    q_ptr->setFocus(Qt::PopupFocusReason);
    Q_EMIT q_ptr->popupVisibleChanged(false);
    Q_EMIT q_ptr->selectionCanceled();
}

void ZzRollerPickerPrivate::handleExternalHide()
{
    if (!popupActive || closingPopup) {
        return;
    }
    restoreSnapshot();
    popupActive = false;
    popup->endInteraction();
    openSnapshot.clear();
    q_ptr->setFocus(Qt::PopupFocusReason);
    Q_EMIT q_ptr->popupVisibleChanged(false);
    Q_EMIT q_ptr->selectionCanceled();
}

QList<ZzRollerColumn> ZzRollerPickerPrivate::normalizeColumns(
    QList<ZzRollerColumn> input) const
{
    QSet<QString> usedKeys;
    usedKeys.reserve(input.size());
    for (ZzRollerColumn &column : input) {
        if (column.key.isEmpty() || usedKeys.contains(column.key)) {
            column.key = zzUniqueColumnKey(&usedKeys);
        }
        usedKeys.insert(column.key);
        column.minimumWidth = std::clamp(
            column.minimumWidth,
            ZzMinimumColumnWidth,
            ZzMaximumColumnWidth);
        column.currentIndex = column.items.isEmpty()
            ? -1
            : std::clamp(
                  column.currentIndex,
                  0,
                  zzColumnItemCount(column) - 1);
    }
    return input;
}

void ZzRollerPickerPrivate::rebuildRollers()
{
    while (QLayoutItem *item = rollerLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    rollers.clear();
    rollers.reserve(columns.size());

    for (int column = 0; column < columns.size(); ++column) {
        auto *roller = new ZzRoller(rollerHost);
        roller->setFrame(false);
        roller->setVisibleItemCount(ZzPickerVisibleItems);
        roller->setMinimumWidth(columns.at(column).minimumWidth);
        roller->setItems(columns.at(column).items);
        roller->setCurrentIndex(columns.at(column).currentIndex);
        roller->setWrapping(columns.at(column).wrapping);
        rollerLayout->addWidget(roller);
        rollers.append(roller);

        QObject::connect(
            roller,
            &ZzRoller::currentIndexChanged,
            q_ptr,
            [this, column](int index) {
                if (column < 0 || column >= columns.size()) {
                    return;
                }
                columns[column].currentIndex = index;
                if (suppressSelectionSignals) {
                    return;
                }
                refreshSummary();
                emitSelectionChanged();
            });
        QObject::connect(
            roller,
            &ZzRoller::activated,
            q_ptr,
            [this, column](int index, const QString &text) {
                if (!suppressSelectionSignals) {
                    Q_EMIT q_ptr->selectionActivated(
                        column,
                        index,
                        text);
                }
            });
    }
    rollerHost->updateGeometry();
    popup->updateGeometry();
}

void ZzRollerPickerPrivate::preparePopupGeometry()
{
    if (okButton != nullptr) {
        okButton->setIcon(okButton->style()->standardIcon(
            QStyle::SP_DialogOkButton,
            nullptr,
            okButton));
    }
    if (cancelButton != nullptr) {
        cancelButton->setIcon(cancelButton->style()->standardIcon(
            QStyle::SP_DialogCancelButton,
            nullptr,
            cancelButton));
    }

    QSize desired = popup->sizeHint();
    // Keep every column readable while honoring the trigger width.  The
    // layout's size hint already accounts for margins, spacing and buttons;
    // this pass only raises the width when content or a caller supplied
    // minimum would otherwise be clipped.
    QFontMetrics metrics(popup->font());
    int columnsWidth = 0;
    for (const ZzRollerColumn &column : columns) {
        int contentWidth = 0;
        for (const QString &item : column.items) {
            contentWidth = std::max(contentWidth, metrics.horizontalAdvance(item));
        }
        columnsWidth += std::max(column.minimumWidth, contentWidth + 24);
    }
    if (columns.size() > 1) {
        columnsWidth += static_cast<int>(columns.size() - 1)
            * rollerLayout->spacing();
    }
    const int horizontalMargins = popupLayout->contentsMargins().left()
        + popupLayout->contentsMargins().right();
    const int contentMinimum = columnsWidth + horizontalMargins;
    desired.setWidth(std::max({desired.width(), contentMinimum, q_ptr->width()}));
    const QRect anchor(
        q_ptr->mapToGlobal(QPoint(0, 0)),
        q_ptr->size());
    QScreen *screen = QGuiApplication::screenAt(anchor.center());
    if (screen == nullptr && q_ptr->windowHandle() != nullptr) {
        screen = q_ptr->windowHandle()->screen();
    }
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        popup->resize(desired);
        popup->move(anchor.bottomLeft() + QPoint(0, 1));
        return;
    }

    const QRect available = screen->availableGeometry();
    desired = desired.boundedTo(available.size());
    const QRect visualAnchor = QStyle::visualRect(
        q_ptr->layoutDirection(), anchor,
        QRect(0, 0, desired.width(), 1));
    int x = visualAnchor.left();
    const int below = anchor.bottom() + 1;
    const int above = anchor.top() - desired.height();
    int y = below;
    if (below + desired.height() - 1 > available.bottom()
        && above >= available.top()) {
        y = above;
    }
    x = std::clamp(
        x,
        available.left(),
        available.right() - desired.width() + 1);
    y = std::clamp(
        y,
        available.top(),
        available.bottom() - desired.height() + 1);
    popup->setGeometry(QRect(QPoint(x, y), desired));
}

void ZzRollerPickerPrivate::refreshSummary()
{
    const QString summary = summaryText();
    if (q_ptr->QPushButton::text() == summary) {
        return;
    }
    q_ptr->QPushButton::setText(summary);
    q_ptr->updateGeometry();
    Q_EMIT q_ptr->currentTextChanged(summary);
}

void ZzRollerPickerPrivate::restoreSnapshot()
{
    if (openSnapshot.size() != columns.size()
        || openSnapshot == currentIndexes()) {
        return;
    }
    suppressSelectionSignals = true;
    for (int column = 0; column < columns.size(); ++column) {
        const int index = openSnapshot.at(column);
        if (index >= 0 && index < columns.at(column).items.size()) {
            rollers.at(column)->setCurrentIndex(index);
        }
        columns[column].currentIndex = rollers.at(column)->currentIndex();
    }
    suppressSelectionSignals = false;
    refreshSummary();
    emitSelectionChanged();
}

void ZzRollerPickerPrivate::emitSelectionChanged()
{
    Q_EMIT q_ptr->currentSelectionChanged(
        currentIndexes(),
        currentTexts());
}

} // namespace ZzFluentUI
