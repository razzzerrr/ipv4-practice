#include "hilbert_widget.h"
#include "hilbert_math.h"
#include <QPainter>
#include <QColor>
#include <random> // Современный рандом
#include <QFile>
#include <QTextStream>
#include <QStringList>

HilbertWidget::HilbertWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);

    // 1. Генерируем "тяжелый" лог-файл на 8000 инцидентов прямо при старте
    generateDenseTestFile("heavy_attacks_report.txt", 70000);

    // 2. Сразу же скармливаем его нашему честному парсеру
    loadDatabaseFromFile("heavy_attacks_report.txt");
}

QString HilbertWidget::indexToIpString(uint32_t index) {
    uint8_t b2 = (index >> 16) & 0xFF;
    uint8_t b3 = (index >> 8) & 0xFF;
    uint8_t b4 = index & 0xFF;
    return QString("10.%1.%2.%3").arg(b2).arg(b3).arg(b4);
}

QString HilbertWidget::getCountryName(int code) {
    if (code == 1) return "🇷🇺 Россия";
    if (code == 2) return "🇺🇸 США";
    return "🇨🇳 Китай";
}

QString HilbertWidget::getThreatName(int code) {
    if (code == 1) return "DDoS-Атака";
    if (code == 2) return "Сканирование портов";
    return "Активность ботнета";
}

QColor HilbertWidget::getThreatColor(int code) {
    if (code == 1) return QColor(231, 76, 60);   // Красный
    if (code == 2) return QColor(241, 196, 15);  // Желтый
    return QColor(155, 89, 182);                 // Фиолетовый
}

bool HilbertWidget::loadDatabaseFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    m_database.clear();
    m_lookUp.clear();
    m_isDbLoaded = true;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;

        QStringList parts = line.split(';');
        if (parts.size() < 3) continue;

        QString ipStr = parts[0].trimmed();
        int type = parts[1].toInt();
        int country = parts[2].toInt();

        QStringList ipParts = ipStr.split('.');
        if (ipParts.size() != 4) continue;

        uint8_t b2 = ipParts[1].toUInt();
        uint8_t b3 = ipParts[2].toUInt();
        uint8_t b4 = ipParts[3].toUInt(); // Честно берем точный хвост IP!

        // Собираем точный уникальный индекс для этого конкретного IP
        uint32_t exactIndex = (static_cast<uint32_t>(b2) << 16) |
            (static_cast<uint32_t>(b3) << 8) |
            static_cast<uint32_t>(b4);

        if (exactIndex >= Hilbert::MAP_SIZE * Hilbert::MAP_SIZE) continue;

        // Сохраняем в базу ровно одну запись на один IP
        Threat t;
        t.index = exactIndex;
        t.type = type;
        t.country = country;

        m_database.push_back(t);
        m_lookUp[exactIndex] = m_database.size() - 1;
    }

    file.close();
    generateHilbertMap();
    return true;
}

void HilbertWidget::setFilterType(int typeIndex) {
    m_currentFilterType = typeIndex;
    if (m_isDbLoaded) {
        generateHilbertMap(); // Перерисовываем карту с учетом фильтра
    }
}

void HilbertWidget::clearMap() {
    m_isDbLoaded = false;
    m_database.clear();
    m_lookUp.clear();
    generateHilbertMap();
}

void HilbertWidget::generateHilbertMap() {
    m_mapImage = QImage(Hilbert::MAP_SIZE, Hilbert::MAP_SIZE, QImage::Format_RGB32);
    uint32_t totalPoints = Hilbert::MAP_SIZE * Hilbert::MAP_SIZE;

    if (!m_isDbLoaded) {
        // Режим мониторинга: рисуем красивый градиентный фон
        for (uint32_t i = 0; i < totalPoints; ++i) {
            uint32_t x = 0, y = 0;
            Hilbert::indexToXY(i, x, y);
            uint8_t green = static_cast<uint8_t>((i * 255) / totalPoints);
            uint8_t blue = static_cast<uint8_t>(255 - green);
            m_mapImage.setPixelColor(x, y, QColor(0, green, blue));
        }
    }
    else {
        m_mapImage.fill(QColor(40, 44, 52)); // Наш темный фон

        // --- ДОБАВЛЯЕМ СЕТКУ ПОДСЕТЕЙ (Рисуем ДО маркеров) ---
        QPainter gridPainter(&m_mapImage);
        // Задаем цвет линии чуть светлее фона (например, 52, 56, 66) и делаем её пунктирной
        gridPainter.setPen(QPen(QColor(55, 60, 72), 1, Qt::DashLine));

        // Разобьем карту 4096х4096 на сетку 8х8 (шаг 512 пикселей)
        int gridStep = Hilbert::MAP_SIZE / 8;
        for (uint32_t g = gridStep; g < Hilbert::MAP_SIZE; g += gridStep) {
            gridPainter.drawLine(g, 0, g, Hilbert::MAP_SIZE); // Вертикальные линии
            gridPainter.drawLine(0, g, Hilbert::MAP_SIZE, g); // Горизонтальные линии
        }
        gridPainter.end();
        // -----------------------------------------------------

        // Дальше идет твой старый код отрисовки "жирных" маркеров
        QPainter imgPainter(&m_mapImage);
        imgPainter.setPen(Qt::NoPen);

        for (const auto& threat : m_database) {
            if (m_currentFilterType != 0 && threat.type != m_currentFilterType) {
                continue;
            }
            uint32_t x = 0, y = 0;
            Hilbert::indexToXY(threat.index, x, y);
            imgPainter.setBrush(getThreatColor(threat.type));
            imgPainter.drawRect(static_cast<int>(x) - 1, static_cast<int>(y) - 1, 3, 3);
        }
        imgPainter.end();
    }

    update(); // Запрашиваем перерисовку виджета на экране
}

void HilbertWidget::mouseMoveEvent(QMouseEvent* event) {
    if (width() == 0 || height() == 0) return;

    // Переводим экранные координаты мыши в координаты нашей карты (координаты пикселей картинки)
    uint32_t imgX = static_cast<uint32_t>((event->position().x() * Hilbert::MAP_SIZE) / width());
    uint32_t imgY = static_cast<uint32_t>((event->position().y() * Hilbert::MAP_SIZE) / height());

    if (imgX >= Hilbert::MAP_SIZE) imgX = Hilbert::MAP_SIZE - 1;
    if (imgY >= Hilbert::MAP_SIZE) imgY = Hilbert::MAP_SIZE - 1;

    QString info;

    if (!m_isDbLoaded) {
        // Если база не загружена, декодируем точный IP под курсором для режима сканирования
        uint32_t exactIndex = Hilbert::xyToIndex(imgX, imgY);
        QString ip = indexToIpString(exactIndex);
        info = QString("<b>Координаты:</b> X:%1, Y:%2<br><b>IP:</b> %3<br><i>Сканирование пространства...</i>")
            .arg(imgX).arg(imgY).arg(ip);
    }
    else {
        int foundDbIndex = -1;
        uint32_t detectedIndex = 0;

        // УМНОЕ СКАНИРОВАНИЕ: ищем угрозу в радиусе 1 пикселя вокруг курсора (область 3х3)
        // Это позволяет легко "цеплять" мышкой маркеры размера 3х3
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int targetX = static_cast<int>(imgX) + dx;
                int targetY = static_cast<int>(imgY) + dy;

                // Проверяем, что не вылетели за границы матрицы
                if (targetX >= 0 && targetX < static_cast<int>(Hilbert::MAP_SIZE) &&
                    targetY >= 0 && targetY < static_cast<int>(Hilbert::MAP_SIZE)) {

                    uint32_t checkIndex = Hilbert::xyToIndex(static_cast<uint32_t>(targetX), static_cast<uint32_t>(targetY));

                    // Ищем элемент в ассоциативном массиве через итератор
                    auto it = m_lookUp.find(checkIndex);

                    // Если it != end(), значит ключ успешно найден!
                    if (it != m_lookUp.end()) {
                        // it->second — это сохраненный индекс в m_database (тип size_t)
                        // Приводим его к int явно через static_cast, чтобы успокоить компилятор и убрать варнинг
                        int dbIdx = static_cast<int>(it->second);
                        const Threat& t = m_database[dbIdx];

                        // Мышка должна реагировать на угрозу ТОЛЬКО если она проходит через фильтр
                        if (m_currentFilterType == 0 || t.type == m_currentFilterType) {
                            foundDbIndex = dbIdx;
                            detectedIndex = checkIndex;
                            break; // Нашли видимую угрозу, выходим из внутреннего цикла
                        }
                    }
                }
            }
            if (foundDbIndex != -1) break; // Выходим из внешнего цикла
        }

        // Выводим информацию в зависимости от того, поймали мы маркер или нет
        if (foundDbIndex != -1) {
            const Threat& t = m_database[foundDbIndex];
            QString ip = indexToIpString(detectedIndex); // Истинный IP-адрес из файла!

            info = QString("<b>[ ИНЦИДЕНТ ИБ ]</b><br>"
                "<b>IP:</b> <span style='font-size:15px; color:#E74C3C;'><b>%1</b></span><br>"
                "<b>Координаты:</b> X:%2, Y:%3<br>"
                "<b>Источник:</b> %4<br>"
                "<b>Тип активности:</b><br><span style='color:#E74C3C;'>%5</span>")
                .arg(ip).arg(imgX).arg(imgY).arg(getCountryName(t.country)).arg(getThreatName(t.type));
        }
        else {
            // Если в этой области ничего нет (или скрыто фильтром) — место безопасно
            uint32_t exactIndex = Hilbert::xyToIndex(imgX, imgY);
            QString ip = indexToIpString(exactIndex);
            info = QString("<b>[ МОНИТОРИНГ ]</b><br>"
                "<b>IP:</b> <span style='font-size:14px; color:#27AE60;'><b>%1</b></span><br>"
                "<b>Координаты:</b> X:%2, Y:%3<br>"
                "<b>Статус:</b> <span style='color:#27AE60;'>Безопасно</span>")
                .arg(ip).arg(imgX).arg(imgY);
        }
    }

    emit ipHovered(info); // Отправляем данные в MainWindow для отображения в сайдбаре
}

void HilbertWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    // Включаем сглаживание, чтобы при изменении размеров окна карта выглядела четко
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(rect(), m_mapImage);
}

void HilbertWidget::generateDenseTestFile(const QString& filePath, int recordsCount) {
    QFile file(filePath);
    // Открываем файл на запись. Если он уже есть, он просто перезапишется
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);

    // Инициализируем генератор случайных чисел, который у тебя уже подключен через <random>
    std::random_device rd;
    std::mt19937 gen(rd());

    // Генерируем октеты так, чтобы они ГАРАНТИРОВАННО легли в сетку 512x512
    std::uniform_int_distribution<> disB2(0, 255); // Строго от 0 до 3!
    std::uniform_int_distribution<> disB3(0, 255);
    std::uniform_int_distribution<> disB4(0, 255);

    std::uniform_int_distribution<> disType(1, 3);    // Три типа угроз
    std::uniform_int_distribution<> disCountry(1, 3); // Три страны

    out << "# Автоматически сгенерированная база инцидентов ИБ\n";
    out << "# Формат: IP; Тип_Угрозы; Код_Страны\n";

    for (int i = 0; i < recordsCount; ++i) {
        int b2 = disB2(gen);
        int b3 = disB3(gen);
        int b4 = disB4(gen);
        int type = disType(gen);
        int country = disCountry(gen);

        // Записываем строку формата: 10.2.145.67;1;2
        out << QString("10.%1.%2.%3;%4;%5\n")
            .arg(b2).arg(b3).arg(b4).arg(type).arg(country);
    }

    file.close();
}

