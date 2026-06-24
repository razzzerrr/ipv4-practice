#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox> // Добавили инклуд списков
#include "hilbert_widget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);

private:
    HilbertWidget* m_hilbertMap;
    QWidget* m_sidebar;

    QLabel* m_statusLabel;
    QLabel* m_ipInfoLabel;
    QComboBox* m_filterCombo; // Наш будущий фильтр
    QPushButton* m_loadDbButton;
    QPushButton* m_clearButton;

    void setupUi();
};