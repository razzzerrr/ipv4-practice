#include "hilbert_widget.h"
#include "hilbert_math.h"
#include <QPainter>
#include <cstring>

// Конструктор: настраиваем трекинг мыши и готовим первичную пустую карту
HilbertWidget::HilbertWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true); // Включаем отслеживание мыши без необходимости зажимать кнопки
    std::memset(m_sectorCounts, 0, sizeof(m_sectorCounts));
    generateHilbertMap();
}

// Полный сброс состояния виджета при очистке карты
void HilbertWidget::clearMap() {
    m_networkBlocks.clear();
    m_pointsDb.clear();
    m_lookUp.clear();
    m_isDbLoaded = false;
    std::memset(m_sectorCounts, 0, sizeof(m_sectorCounts));
    generateHilbertMap(); // Перерисовываем в исходный пустой вид
}

// Переключение видимости координатной сетки / подсетей
void HilbertWidget::setGridVisible(bool visible) {
    m_showGrid = visible;
    generateHilbertMap(); // Перерисовываем карту с сеткой или без
}

// Регистрация новой подсети в пуле (вызывается при парсинге)
size_t HilbertWidget::registerNetworkBlock(const QString& input, const QString& company, const QColor& color) {
    m_networkBlocks.push_back({ input, company, color });
    m_isDbLoaded = true;
    return m_networkBlocks.size() - 1; // Возвращаем индекс блока для связи с точками
}

// Привязка конкретного IP (индекса) к зарегистрированному блоку сети
void HilbertWidget::addIpPoint(uint32_t exactIndex, size_t blockId) {
    m_pointsDb.push_back({ exactIndex, blockId });
    m_lookUp[exactIndex] = m_pointsDb.size() - 1; // Запоминаем позицию для мгновенного поиска под курсором
}

// Главный метод рендеринга карты в буфер QImage
void HilbertWidget::generateHilbertMap() {
    m_mapImage = QImage(Hilbert::MAP_SIZE, Hilbert::MAP_SIZE, QImage::Format_RGB32);
    std::memset(m_sectorCounts, 0, sizeof(m_sectorCounts));

    // Оптимизация: если база пуста, мгновенно заливаем фон вместо 16 млн итераций цикла
    if (!m_isDbLoaded) {
        m_mapImage.fill(QColor(240, 242, 245));
    }
    else {
        m_mapImage.fill(QColor(245, 245, 247)); // Базовый цвет свободного пространства

        QPainter imgPainter(&m_mapImage);
        imgPainter.setPen(Qt::NoPen);

        // Отрисовка всех занятых IP-адресов из базы данных
        for (const auto& pt : m_pointsDb) {
            uint32_t x = 0, y = 0;
            Hilbert::indexToXY(pt.index, x, y); // Переводим одномерный индекс IP в 2D координаты Гильберта

            // Распределяем точку по аналитическим секторам (матрица 8х8)
            int sX = static_cast<int>(x) / 512;
            int sY = static_cast<int>(y) / 512;
            if (sX >= 0 && sX < 8 && sY >= 0 && sY < 8) {
                m_sectorCounts[sY][sX]++;
            }

            // Рисуем узел как квадрат 5x5 пикселей для лучшей визуальной читаемости крупных сетей
            imgPainter.setBrush(m_networkBlocks[pt.blockId].color);
            imgPainter.drawRect(static_cast<int>(x) - 2, static_cast<int>(y) - 2, 5, 5);
        }
        imgPainter.end();
    }

    // Отрисовка координатной сетки поверх карты
    if (m_showGrid) {
        QPainter gridPainter(&m_mapImage);
        gridPainter.setPen(QPen(QColor(100, 100, 100, 90), 2, Qt::DashLine));

        QFont font = gridPainter.font();
        font.setPixelSize(32);
        font.setBold(true);
        gridPainter.setFont(font);

        int gridStep = Hilbert::MAP_SIZE / 8; // Сетка разделяет пространство на 64 крупных блока

        // Чертим линии сетки
        for (uint32_t g = gridStep; g < Hilbert::MAP_SIZE; g += gridStep) {
            gridPainter.drawLine(g, 0, g, Hilbert::MAP_SIZE);
            gridPainter.drawLine(0, g, Hilbert::MAP_SIZE, g);
        }

        // Подписываем базовые маски подсетей (10.X.0.0/14) внутри каждого квадрата
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint32_t centerX = col * gridStep + (gridStep / 2);
                uint32_t centerY = row * gridStep + (gridStep / 2);
                uint32_t centerIndex = Hilbert::xyToIndex(centerX, centerY);
                uint8_t b2 = (centerIndex >> 16) & 0xFF;

                QString label = QString("10.%1/14").arg(b2);
                gridPainter.drawText(col * gridStep + 20, row * gridStep + 50, label);
            }
        }
        gridPainter.end();
    }
    update(); // Вызываем системное событие перерисовки виджета
}

// Обработчик движения мыши: вычисляет IP под курсором и отправляет данные в UI
void HilbertWidget::mouseMoveEvent(QMouseEvent* event) {
    if (width() == 0 || height() == 0) return;

    // Масштабируем координаты окна под реальный размер координатной карты (4096х4096)
    uint32_t imgX = static_cast<uint32_t>((event->position().x() * Hilbert::MAP_SIZE) / width());
    uint32_t imgY = static_cast<uint32_t>((event->position().y() * Hilbert::MAP_SIZE) / height());

    if (imgX >= Hilbert::MAP_SIZE) imgX = Hilbert::MAP_SIZE - 1;
    if (imgY >= Hilbert::MAP_SIZE) imgY = Hilbert::MAP_SIZE - 1;

    uint32_t exactIndex = Hilbert::xyToIndex(imgX, imgY);
    QString ip = indexToIpString(exactIndex);
    QString info;

    // Реализация радиуса зацепления (матрица 5х5 вокруг курсора), чтобы легче попадать по точкам
    int foundPtIdx = -1;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            int targetX = static_cast<int>(imgX) + dx;
            int targetY = static_cast<int>(imgY) + dy;
            if (targetX >= 0 && targetX < 4096 && targetY >= 0 && targetY < 4096) {
                uint32_t checkIndex = Hilbert::xyToIndex(targetX, targetY);
                auto it = m_lookUp.find(checkIndex);
                if (it != m_lookUp.end()) {
                    foundPtIdx = it->second;
                    break;
                }
            }
        }
        if (foundPtIdx != -1) break;
    }

    // Формируем HTML-карточку для вывода в информационную панель интерфейса
    if (foundPtIdx != -1) {
        const auto& pt = m_pointsDb[foundPtIdx];
        const auto& block = m_networkBlocks[pt.blockId];

        info = QString::fromUtf8("<b>[ УЗЕЛ СЕТИ ]</b><br>"
            "<b>IP-Адрес:</b> <span style='font-size:15px; color:#2980B9;'><b>%1</b></span><br>"
            "<b>Владелец подсети:</b><br><span style='font-size:14px; color:#2C3E50;'><b>%4</b></span><br>"
            "<b>Введено как:</b> %5<br>"
            "<b>Координаты карты:</b> X:%2, Y:%3")
            .arg(ip).arg(imgX).arg(imgY).arg(block.companyName).arg(block.rawInput);
    }
    else {
        info = QString::fromUtf8("<b>[ Свободное пространство ]</b><br>"
            "<b>Текущий IP:</b> %1<br>"
            "<b>Статус:</b> Не распределен (Свободен)")
            .arg(ip);
    }

    // Если включена сетка, добавляем в подсказку расширенную локальную аналитику сектора
    if (m_showGrid) {
        int sX = static_cast<int>(imgX) / 512;
        int sY = static_cast<int>(imgY) / 512;
        if (sX >= 0 && sX < 8 && sY >= 0 && sY < 8) {
            int countInSector = m_sectorCounts[sY][sX];
            uint32_t sectorCenterIdx = Hilbert::xyToIndex(sX * 512 + 256, sY * 512 + 256);
            uint8_t b2 = (sectorCenterIdx >> 16) & 0xFF;

            info += QString::fromUtf8("<br><hr><b>[ СЕКТОР АНАЛИТИКИ ]</b><br>"
                "<b>Подсеть:</b> 10.%1/14<br>"
                "<b>Заняно узлов компанией:</b> <span style='color:#27AE60;'><b>%2</b></span>")
                .arg(b2).arg(countInSector);
        }
    }
    emit ipHovered(info); // Передаем сформированную строку через сигнал-слот в MainWindow
}

// Экспорт карты в PNG файл сверхвысокого разрешения (честный попиксельный рендер 1-к-1)
bool HilbertWidget::saveToPng(const QString& filePath) {
    QImage exportImage(Hilbert::MAP_SIZE, Hilbert::MAP_SIZE, QImage::Format_RGB32);
    exportImage.fill(QColor(245, 245, 247));

    QPainter painter(&exportImage);

    // В файле сохраняем точки строго размером в 1 пиксель для максимальной точности
    for (const auto& pt : m_pointsDb) {
        uint32_t x = 0, y = 0;
        Hilbert::indexToXY(pt.index, x, y);
        painter.setPen(m_networkBlocks[pt.blockId].color);
        painter.drawPoint(static_cast<int>(x), static_cast<int>(y));
    }

    // Переносим разметку сетки на экспортируемый файл
    if (m_showGrid) {
        painter.setPen(QPen(QColor(100, 100, 100, 70), 2, Qt::DashLine));
        QFont font = painter.font();
        font.setPixelSize(32);
        font.setBold(true);
        painter.setFont(font);

        int gridStep = Hilbert::MAP_SIZE / 8;
        for (uint32_t g = gridStep; g < Hilbert::MAP_SIZE; g += gridStep) {
            painter.drawLine(g, 0, g, Hilbert::MAP_SIZE);
            painter.drawLine(0, g, Hilbert::MAP_SIZE, g);
        }
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint32_t centerX = col * gridStep + (gridStep / 2);
                uint32_t centerY = row * gridStep + (gridStep / 2);
                uint32_t centerIndex = Hilbert::xyToIndex(centerX, centerY);
                uint8_t b2 = (centerIndex >> 16) & 0xFF;
                QString label = QString("10.%1/14").arg(b2);
                painter.drawText(col * gridStep + 20, row * gridStep + 50, label);
            }
        }
    }
    painter.end();
    return exportImage.save(filePath, "PNG");
}

// Отрисовка буферизированного изображения на экране приложения с автоматическим масштабированием
void HilbertWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawImage(rect(), m_mapImage);
}

// Конвертер внутреннего индекса карты обратно в читаемый строковый IP-формат
QString HilbertWidget::indexToIpString(uint32_t index) {
    uint8_t b4 = index & 0xFF;
    uint8_t b3 = (index >> 8) & 0xFF;
    uint8_t b2 = (index >> 16) & 0xFF;
    return QString("10.%1.%2.%3").arg(b2).arg(b3).arg(b4);
}