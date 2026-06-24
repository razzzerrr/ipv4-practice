#pragma once
#include <QWidget>
#include <QImage>
#include <QMouseEvent>
#include <vector>
#include <unordered_map> // Для сверхбыстрого поиска хостов

// Структура нашей базы данных угроз
struct Threat {
    uint32_t index;
    int type;       // 1 = DDoS (Красный), 2 = Сканирование (Желтый), 3 = Ботнет (Фиолетовый)
    int country;    // 1 = Россия, 2 = США, 3 = Китай
};

class HilbertWidget : public QWidget {
    Q_OBJECT

public:
    HilbertWidget(QWidget* parent = nullptr);
    bool loadDatabaseFromFile(const QString& filePath);
    void clearMap();
    void setFilterType(int typeIndex); // Метод для изменения фильтра атак
    void generateDenseTestFile(const QString& filePath, int recordsCount);

signals:
    void ipHovered(const QString& ipInfo);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QImage m_mapImage;
    bool m_isDbLoaded = false;
    int m_currentFilterType = 0; // 0 = Все, 1 = DDoS, 2 = Скан, 3 = Ботнет

    std::vector<Threat> m_database;                  // Сама база записей
    std::unordered_map<uint32_t, size_t> m_lookUp;   // Таблица быстрого поиска: [Индекс] -> [Позиция в базе]

    void generateHilbertMap();
    QString indexToIpString(uint32_t index);
    QString getCountryName(int code);
    QString getThreatName(int code);
    QColor getThreatColor(int code);
};