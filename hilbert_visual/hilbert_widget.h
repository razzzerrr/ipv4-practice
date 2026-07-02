#pragma once
#include <QWidget>
#include <QImage>
#include <QMouseEvent>
#include <vector>
#include <unordered_map>

// --- ПУНКТ 4 & 8: Структура для отображения принадлежности подсетей компаниям ---
struct NetworkBlock {
    QString rawInput;   // Исходная строка (например, "10.50.0.0/16" или "10.0.1.0-10.0.2.255")
    QString companyName;// Имя компании (Google, Meta, Яндекс, Ростелеком и т.д.)
    QColor color;       // Уникальный цвет компании на карте
};

// Внутреннее представление точки на карте для быстрого рендеринга
struct MapPoint {
    uint32_t index;
    size_t blockId;     // Индекс родительской структуры NetworkBlock
};

class HilbertWidget : public QWidget {
    Q_OBJECT

public:
    HilbertWidget(QWidget* parent = nullptr);

    void clearMap();
    void setGridVisible(bool visible);
    bool saveToPng(const QString& filePath);

    // Динамическое добавление точек из парсера
    void addIpPoint(uint32_t exactIndex, size_t blockId);
    size_t registerNetworkBlock(const QString& input, const QString& company, const QColor& color);
    void refreshMap() { generateHilbertMap(); }

signals:
    void ipHovered(const QString& ipInfo);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QImage m_mapImage;
    bool m_isDbLoaded = false;
    bool m_showGrid = true;

    // Списки структур по требованию куратора
    std::vector<NetworkBlock> m_networkBlocks;       // Список созданных структур (Пункт 4)
    std::vector<MapPoint> m_pointsDb;                // База отображаемых точек
    std::unordered_map<uint32_t, size_t> m_lookUp;   // Быстрый поиск: индекс -> индекс в m_pointsDb

    int m_sectorCounts[8][8];

    void generateHilbertMap();
    QString indexToIpString(uint32_t index);
};