#pragma once

#include <QtWidgets/QLabel>

namespace ZzFluentUI {

class ZzInfoBar;

class ZzInfoBarLayout final : public QWidget {
public:
    explicit ZzInfoBarLayout(QWidget* parent);
    void setItems(QWidget* title, QWidget* message, QWidget* action);
    void refreshLayout();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    int naturalWidth() const;
    int horizontalHeight() const;
    int verticalHeight(int width) const;
    void positionItems();
    QWidget* title_ = nullptr;
    QWidget* message_ = nullptr;
    QWidget* action_ = nullptr;
};

class ZzInfoBarMessageLabel final : public QLabel {
public:
    explicit ZzInfoBarMessageLabel(ZzInfoBar* bar, QWidget* parent);
    void setFullText(const QString& text);
    void refreshElision();
    QSize sizeHint() const override;
    int heightForWidth(int width) const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    bool isPopup() const;
    int wrappedHeight(int width) const;
    QString elidedText(int width, int maximumLines) const;
    ZzInfoBar* bar_ = nullptr;
    QString fullText_;
};

} // namespace ZzFluentUI
