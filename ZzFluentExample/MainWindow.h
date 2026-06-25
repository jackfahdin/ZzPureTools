#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

class QLabel;
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

    ZzDemoViewModel* m_viewModel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_selectionLabel = nullptr;
    ZzPushButton* m_accentButton = nullptr;
    ZzPushButton* m_standardButton = nullptr;
    ZzComboBox* m_comboBox = nullptr;
};

#endif // MAINWINDOW_H
