#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QWidget>

class QLabel;
class QGroupBox;
class ZzComboBox;
class ZzDemoViewModel;
class ZzPushButton;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void buildUi();
    void bindViewModel();
    void bindTheme();

    [[nodiscard]] QGroupBox* createSection(const QString& title);

    ZzDemoViewModel* m_viewModel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_selectionLabel = nullptr;
    QLabel* m_themeLabel = nullptr;
    QLabel* m_clickCountLabel = nullptr;
    ZzPushButton* m_accentButton = nullptr;
    ZzPushButton* m_standardButton = nullptr;
    ZzComboBox* m_comboBox = nullptr;
};

#endif  // MAINWINDOW_HPP
