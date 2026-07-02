#pragma once
#include <QMainWindow>
#include <QLabel>
#include <map>
#include <vector>
#include <QPushButton>
#include <QComboBox> 
#include <QCheckBox>
#include "hilbert_widget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);

private:
    void setupUi();
    void updateAnalyticsDisplay();              // Метод пересчета и вывода статистики
    bool parseAndAddLine(const QString& line);   // Метод интеллектуального парсинга подсетей

    // --- ГРАФИКА И ИНТЕРФЕЙС ---
    HilbertWidget* m_hilbertMap;
    QWidget* m_sidebar;
    QLabel* m_statusLabel;
    QLabel* m_ipInfoLabel;       // Информационный экран для мышки
    QLabel* m_analyticsLabel;    // Выделенный экран для общей аналитики
    QComboBox* m_filterCombo;
    QCheckBox* m_gridCheckbox;
    QPushButton* m_manualAddButton;
    QPushButton* m_saveImageButton;
    QPushButton* m_loadDbButton;
    QPushButton* m_clearButton;

    // --- УМНЫЙ ХРАНИТЕЛЬ СОСТОЯНИЯ АДРЕСОВ (Защита от дубликатов > 100%) ---
    std::map<QString, uint32_t> m_companyStats; // Имя компании -> количество реально занятых IP
    std::vector<uint8_t> m_ipOwnership;         // Карта владения на 16.7 млн элементов (хранит ID компании)
    std::map<QString, uint8_t> m_companyToId;   // Быстрый поиск: Имя компании -> её числовой ID
    std::vector<QString> m_idToCompany;         // Обратный поиск: Числовой ID -> Имя компании
};