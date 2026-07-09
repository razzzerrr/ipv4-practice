#include "hilbert_widget.h"
#include "hilbert_math.h"
#include <QPainter>
#include <cstring>

// Конструктор: настраиваем трекинг мыши и готовим первичную пустую карту
HilbertWidget::HilbertWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);

    // Сразу выделяем плоский массив под всё пространство IPv4 /24 и заполняем пустотой
    m_lookUp.assign(16777216, -1);

    std::memset(m_sectorCounts, 0, sizeof(m_sectorCounts));
    generateHilbertMap();
}

// Полный сброс состояния виджета при очистке карты (отрабатывает мгновенно)
void HilbertWidget::clearMap() {
    m_networkBlocks.clear();
    m_pointsDb.clear();
    m_lookUp.assign(16777216, -1); // Сброс памяти за пару миллисекунд
    m_isDbLoaded = false;
    std::memset(m_sectorCounts, 0, sizeof(m_sectorCounts));
    generateHilbertMap();
}

// Переключение видимости координатной сетки
void HilbertWidget::setGridVisible(bool visible) {
    m_showGrid = visible;
    update();
}

// Регистрация новой подсети в пуле
size_t HilbertWidget::registerNetworkBlock(const QString& input, const QString& company, const QColor& color) {
    m_networkBlocks.push_back({ input, company, color });
    m_isDbLoaded = true;
    return m_networkBlocks.size() - 1;
}

// Привязка конкретного IP к зарегистрированному блоку сети
void HilbertWidget::addIpPoint(uint32_t exactIndex, size_t blockId) {
    m_pointsDb.push_back({ exactIndex, blockId });
    if (exactIndex < 16777216) {
        m_lookUp[exactIndex] = static_cast<int32_t>(m_pointsDb.size() - 1);
    }
}

// Метод рендеринга КАРТЫ
void HilbertWidget::generateHilbertMap() {
    m_mapImage = QImage(Hilbert::MAP_SIZE, Hilbert::MAP_SIZE, QImage::Format_RGB32);
    std::memset(m_sectorCounts, 0, sizeof(m_sectorCounts));

    if (!m_isDbLoaded) {
        m_mapImage.fill(QColor(240, 242, 245));
    }
    else {
        m_mapImage.fill(QColor(245, 245, 247));

        QPainter imgPainter(&m_mapImage);
        imgPainter.setPen(Qt::NoPen);

        for (const auto& pt : m_pointsDb) {
            uint32_t x = 0, y = 0;
            Hilbert::indexToXY(pt.index, x, y);

            int sX = static_cast<int>(x) / 512;
            int sY = static_cast<int>(y) / 512;
            if (sX >= 0 && sX < 8 && sY >= 0 && sY < 8) {
                m_sectorCounts[sY][sX]++;
            }

            imgPainter.setBrush(m_networkBlocks[pt.blockId].color);
            imgPainter.drawRect(static_cast<int>(x) - 2, static_cast<int>(y) - 2, 5, 5);
        }
        imgPainter.end();
    }

    m_mapPixmap = QPixmap::fromImage(m_mapImage);
    update();
}

// Отрисовка вписанного квадрата, динамической сетки и аккуратного текста
void HilbertWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);

    painter.fillRect(rect(), QColor(240, 242, 245));

    int side = qMin(width(), height());
    int xOffset = (width() - side) / 2;
    int yOffset = (height() - side) / 2;

    // 1. Отрисовка карты
    painter.drawPixmap(xOffset, yOffset, side, side, m_mapPixmap);

    // Включаем сглаживание для геометрии
    painter.setRenderHint(QPainter::Antialiasing, true);

    // --- МОДЕРНИЗАЦИЯ: Внешняя рамка карты ТЕПЕРЬ ВСЕГДА НА ЭКРАНЕ ---
    painter.setPen(QPen(QColor(90, 100, 110, 120), 1.5, Qt::SolidLine));
    painter.drawRect(xOffset, yOffset, side, side);

    // 2. Отрисовка адаптивной сетки (только внутренности по флагу)
    if (m_showGrid) {
        // Тонкие аккуратные линии внутренней сетки
        painter.setPen(QPen(QColor(115, 128, 142, 90), 1.0, Qt::DashLine));

        int gridStep = side / 8;

        // Рисуем только внутренние разделители
        for (int i = 1; i < 8; ++i) {
            int pos = i * gridStep;
            painter.drawLine(xOffset + pos, yOffset, xOffset + pos, yOffset + side);
            painter.drawLine(xOffset, yOffset + pos, xOffset + side, yOffset + pos);
        }

        // 3. Шрифт
        QFont font = painter.font();
        int fontSize = qMax(8, side / 46);
        font.setPixelSize(fontSize);
        font.setBold(false);
        font.setWeight(QFont::Normal);
        painter.setFont(font);

        // 4. Отрисовка подписей диапазонов по центру секторов
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint32_t mapGridStep = Hilbert::MAP_SIZE / 8;
                uint32_t mapCenterX = col * mapGridStep + (mapGridStep / 2);
                uint32_t mapCenterY = row * mapGridStep + (mapGridStep / 2);
                uint32_t centerIndex = Hilbert::xyToIndex(mapCenterX, mapCenterY);

                uint8_t b1 = (centerIndex >> 16) & 0xFF;
                QString label = QString("%1.X.X/6").arg(b1);

                QRect cellRect(xOffset + col * gridStep, yOffset + row * gridStep, gridStep, gridStep);

                // Мягкая подложка для читаемости
                painter.setPen(QColor(255, 255, 255, 140));
                painter.drawText(cellRect.translated(1, 1), Qt::AlignCenter, label);

                // Графитовый основной текст
                painter.setPen(QColor(71, 85, 105, 210));
                painter.drawText(cellRect, Qt::AlignCenter, label);
            }
        }
    }
}

// Обработчик движения мыши
void HilbertWidget::mouseMoveEvent(QMouseEvent* event) {
    if (width() == 0 || height() == 0) return;

    int side = qMin(width(), height());
    int xOffset = (width() - side) / 2;
    int yOffset = (height() - side) / 2;

    int mouseX = static_cast<int>(event->position().x());
    int mouseY = static_cast<int>(event->position().y());

    if (mouseX < xOffset || mouseX >= xOffset + side || mouseY < yOffset || mouseY >= yOffset + side) {
        emit ipHovered(QString::fromUtf8("<b>[ Вне карты ]</b><br>Наведите курсор на квадратную карту..."));
        return;
    }

    uint32_t imgX = static_cast<uint32_t>(((mouseX - xOffset) * Hilbert::MAP_SIZE) / side);
    uint32_t imgY = static_cast<uint32_t>(((mouseY - yOffset) * Hilbert::MAP_SIZE) / side);

    if (imgX >= Hilbert::MAP_SIZE) imgX = Hilbert::MAP_SIZE - 1;
    if (imgY >= Hilbert::MAP_SIZE) imgY = Hilbert::MAP_SIZE - 1;

    uint32_t exactIndex = Hilbert::xyToIndex(imgX, imgY);
    QString ip = indexToIpString(exactIndex);
    QString info;

    int foundPtIdx = -1;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            int targetX = static_cast<int>(imgX) + dx;
            int targetY = static_cast<int>(imgY) + dy;
            if (targetX >= 0 && targetX < 4096 && targetY >= 0 && targetY < 4096) {
                uint32_t checkIndex = Hilbert::xyToIndex(targetX, targetY);

                if (checkIndex < 16777216) {
                    int32_t val = m_lookUp[checkIndex];
                    if (val != -1) {
                        foundPtIdx = val;
                        break;
                    }
                }
            }
        }
        if (foundPtIdx != -1) break;
    }

    if (foundPtIdx != -1) {
        const auto& pt = m_pointsDb[foundPtIdx];
        const auto& block = m_networkBlocks[pt.blockId];

        info = QString::fromUtf8("<b>[ УЗЕЛ СЕТИ ]</b><br>"
            "<b>Диапазон /24:</b> <span style='font-size:14px; color:#2980B9;'><b>%1</b></span><br>"
            "<b>Владелец:</b><br><span style='font-size:13px; color:#2C3E50;'><b>%4</b></span><br>"
            "<b>Введено как:</b> %5<br>"
            "<b>Координаты карты:</b> X:%2, Y:%3")
            .arg(ip).arg(imgX).arg(imgY).arg(block.companyName).arg(block.rawInput);
    }
    else {
        info = QString::fromUtf8("<b>[ Свободное пространство ]</b><br>"
            "<b>Текущая подсеть:</b> %1<br>"
            "<b>Статус:</b> Не распределен (Свободен)")
            .arg(ip);
    }

    if (m_showGrid) {
        int sX = static_cast<int>(imgX) / 512;
        int sY = static_cast<int>(imgY) / 512;
        if (sX >= 0 && sX < 8 && sY >= 0 && sY < 8) {
            int countInSector = m_sectorCounts[sY][sX];
            uint32_t sectorCenterIdx = Hilbert::xyToIndex(sX * 512 + 256, sY * 512 + 256);
            uint8_t b1 = (sectorCenterIdx >> 16) & 0xFF;

            info += QString::fromUtf8("<br><hr><b>[ СЕКТОР АНАЛИТИКИ ]</b><br>"
                "<b>Магистраль:</b> %1.X.X/6<br>"
                "<b>Занято подсетей /24:</b> <span style='color:#27AE60;'><b>%2</b></span> из 262144")
                .arg(b1).arg(countInSector);
        }
    }
    emit ipHovered(info);
}

// Экспорт карты в PNG (рамка на картинке тоже будет всегда)
bool HilbertWidget::saveToPng(const QString& filePath) {
    QImage exportImage(Hilbert::MAP_SIZE, Hilbert::MAP_SIZE, QImage::Format_RGB32);
    exportImage.fill(QColor(245, 245, 247));

    QPainter painter(&exportImage);

    for (const auto& pt : m_pointsDb) {
        uint32_t x = 0, y = 0;
        Hilbert::indexToXY(pt.index, x, y);
        painter.setPen(m_networkBlocks[pt.blockId].color);
        painter.drawPoint(static_cast<int>(x), static_cast<int>(y));
    }

    // Сохраняем внешнюю рамку в PNG при любом раскладе
    painter.setPen(QPen(QColor(90, 100, 110, 140), 3, Qt::SolidLine));
    painter.drawRect(0, 0, Hilbert::MAP_SIZE - 1, Hilbert::MAP_SIZE - 1);

    if (m_showGrid) {
        painter.setPen(QPen(QColor(100, 100, 100, 80), 2, Qt::DashLine));
        QFont font = painter.font();
        font.setPixelSize(85);
        font.setBold(false);
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
                uint8_t b1 = (centerIndex >> 16) & 0xFF;
                QString label = QString("%1.X.X/6").arg(b1);

                QRect cellRect(col * gridStep, row * gridStep, gridStep, gridStep);
                painter.setPen(QColor(70, 80, 95, 220));
                painter.drawText(cellRect, Qt::AlignCenter, label);
            }
        }
    }
    painter.end();
    return exportImage.save(filePath, "PNG");
}

// Вспомогательный конвертер индекса в строку IP
QString HilbertWidget::indexToIpString(uint32_t index) {
    uint8_t b1 = (index >> 16) & 0xFF;
    uint8_t b2 = (index >> 8) & 0xFF;
    uint8_t b3 = index & 0xFF;
    return QString("%1.%2.%3.0/24").arg(b1).arg(b2).arg(b3);
}