#pragma once
#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <vector>

// —труктура дл¤ отображени¤ принадлежности подсетей компани¤м
struct NetworkBlock {
    QString rawInput;     // »сходна¤ строка (например, "10.50.0.0/16")
    QString companyName;  // »м¤ компании (Google, яндекс...)
    QColor color;         // ”никальный цвет компании на карте
};

// ¬нутреннее представление точки на карте дл¤ быстрого рендеринга
struct MapPoint {
    uint32_t index;
    size_t blockId;       // ID родительской структуры NetworkBlock
};

class HilbertWidget : public QWidget {
    Q_OBJECT

public:
    HilbertWidget(QWidget* parent = nullptr);

    void clearMap();
    void setGridVisible(bool visible);
    bool saveToPng(const QString& filePath);

    // ƒинамическое добавление точек из парсера
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
    QPixmap m_mapPixmap; // јппаратно оптимизированный кэш дл¤ быстрой отрисовки карты
    bool m_isDbLoaded = false;
    bool m_showGrid = true;

    std::vector<NetworkBlock> m_networkBlocks;   // —писок созданных структур
    std::vector<MapPoint> m_pointsDb;            // Ѕаза отображаемых точек
    std::vector<int32_t> m_lookUp;               // ѕлоский массив O(1) дл¤ мгновенной очистки и поиска

    int m_sectorCounts[8][8];

    void generateHilbertMap();
    QString indexToIpString(uint32_t index);
};