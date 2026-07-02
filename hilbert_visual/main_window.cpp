#include "main_window.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QCheckBox>
#include <QInputDialog>
#include <map>
#include <vector>

// Конструктор главного окна: подготавливаем ОЗУ-базу уникального владения адресами
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // 16 777 216 байт под трекер (покрывает весь диапазон маски 10.0.0.0/8)
    m_ipOwnership.assign(16777216, 0);
    m_idToCompany.push_back(QString::fromUtf8("Свободно")); // ID = 0 жестко закреплен за нераспределенным пространством

    setupUi(); // Инициализация графического интерфейса

    // Логика связи: отправляем текст-карточку из виджета карты прямо в лейбл сайдбара
    connect(m_hilbertMap, &HilbertWidget::ipHovered, m_ipInfoLabel, &QLabel::setText);

    // Чекбокс сетки: связываем состояние интерфейса со свойством графического движка
    connect(m_gridCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
        m_hilbertMap->setGridVisible(checked);
        });

    // Архитектурная заглушка под будущую сортировку/фильтрацию категорий (пропущена, чтобы не плодить мертвый код)
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        Q_UNUSED(index);
        });

    // Обработчик экспорта в изображение высокого разрешения
    connect(m_saveImageButton, &QPushButton::clicked, this, [=]() {
        QString savePath = QFileDialog::getSaveFileName(this,
            QString::fromUtf8("Сохранить карту как рисунок"), "", "PNG Image (*.png)");
        if (!savePath.isEmpty()) {
            if (m_hilbertMap->saveToPng(savePath)) {
                QMessageBox::information(this, QString::fromUtf8("Успех"), QString::fromUtf8("Карта успешно сохранена!"));
            }
            else {
                QMessageBox::warning(this, QString::fromUtf8("Ошибка"), QString::fromUtf8("Не удалось сохранить изображение."));
            }
        }
        });

    // Очистка всех контейнеров, массивов уникальности и сброс аналитики
    connect(m_clearButton, &QPushButton::clicked, this, [=]() {
        m_hilbertMap->clearMap();
        m_companyStats.clear();
        m_companyToId.clear();
        m_idToCompany.clear();
        m_idToCompany.push_back(QString::fromUtf8("Свободно"));
        m_ipOwnership.assign(16777216, 0);

        m_filterCombo->setCurrentIndex(0);
        m_statusLabel->setText(QString::fromUtf8("Статус: Карта очищена"));
        m_ipInfoLabel->setText(QString::fromUtf8("<b>Информация об IP:</b><br>Наведите курсор на карту..."));
        updateAnalyticsDisplay();
        });

    // Загрузка текстовой базы/логов провайдеров
    connect(m_loadDbButton, &QPushButton::clicked, this, [=]() {
        QString filePath = QFileDialog::getOpenFileName(this,
            QString::fromUtf8("Открыть файл базы адресов"), "", "Logs (*.txt *.log);;All files (*.*)");

        if (filePath.isEmpty()) return;

        QFile file(filePath);
        int loadedCount = 0;

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8); // Корректно читаем кириллицу в именах провайдеров на любых ОС

            QString line;
            while (stream.readLineInto(&line)) {
                QString trimmedLine = line.trimmed();
                if (!trimmedLine.isEmpty() && !trimmedLine.startsWith('#')) {
                    if (parseAndAddLine(trimmedLine)) {
                        loadedCount++;
                    }
                }
            }
            file.close();

            m_hilbertMap->refreshMap(); // Перерисовываем карту ОДИН раз после парсинга всего файла для высокой скорости работы
            updateAnalyticsDisplay();   // Обновляем статистику

            QFileInfo fi(filePath);
            m_statusLabel->setText(QString::fromUtf8("Добавлено объектов: <b>%1</b> из файла %2").arg(loadedCount).arg(fi.fileName()));
        }
        else {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"), QString::fromUtf8("Не удалось открыть файл!"));
        }
        });

    // Диалоговое окно для быстрого ручного тестирования подсетей
    connect(m_manualAddButton, &QPushButton::clicked, this, [=]() {
        bool ok;
        QString input = QInputDialog::getText(this, QString::fromUtf8("Ручной ввод адресов"),
            QString::fromUtf8("Введите подсеть и компанию через ';' или просто адрес:\nПример: 10.5.0.0/16; Google\nИли диапазон: 10.1.1.0-10.1.2.255; Яндекс"),
            QLineEdit::Normal, "10.0.0.0/24; Ростелеком", &ok);

        if (ok && !input.trimmed().isEmpty()) {
            if (parseAndAddLine(input.trimmed())) {
                m_hilbertMap->refreshMap();
                updateAnalyticsDisplay();
                m_statusLabel->setText(QString::fromUtf8("Статус: Элементы добавлены вручную"));
            }
            else {
                QMessageBox::warning(this, QString::fromUtf8("Ошибка"), QString::fromUtf8("Неверный формат ввода подсети!"));
            }
        }
        });
}

// Интеллектуальный разборщик форматов ввода (CIDR, Диапазоны, Одиночные IP)
bool MainWindow::parseAndAddLine(const QString& line) {
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith('#')) return false;

    QStringList parts = trimmed.split(';');
    QString ipExpression = parts[0].trimmed();
    QString companyName = (parts.size() > 1) ? parts[1].trimmed() : QString::fromUtf8("Неизвестный провайдер");

    // Выделение и сохранение сквозного числового ID для новых компаний (экономим память в трекере)
    if (m_companyToId.find(companyName) == m_companyToId.end()) {
        if (m_idToCompany.size() >= 255) return false; // Защита емкости uint8_t
        uint8_t newId = static_cast<uint8_t>(m_idToCompany.size());
        m_companyToId[companyName] = newId;
        m_idToCompany.push_back(companyName);
    }
    uint8_t currentCompanyId = m_companyToId[companyName];

    // Лямбда для парсинга октетов и генерации 24-битного смещения внутри сети 10.0.0.0/8
    auto lambdaIpToUint = [](const QString& ipStr) -> uint32_t {
        QStringList octets = ipStr.split('.');
        if (octets.size() != 4) return 0xFFFFFFFF;
        uint8_t b2 = octets[1].toUInt();
        uint8_t b3 = octets[2].toUInt();
        uint8_t b4 = octets[3].toUInt();
        return (static_cast<uint32_t>(b2) << 16) | (static_cast<uint32_t>(b3) << 8) | b4;
        };

    // Автоматическая генерация уникальной палитры для провайдеров
    static std::map<QString, QColor> companyColors;
    if (companyColors.find(companyName) == companyColors.end()) {
        static const QColor palette[] = {
            QColor(46, 204, 113), QColor(52, 152, 219), QColor(231, 76, 60),
            QColor(155, 89, 182), QColor(241, 196, 15), QColor(230, 126, 34),
            QColor(26, 188, 156), QColor(242, 121, 180)
        };
        companyColors[companyName] = palette[companyColors.size() % 8];
    }
    QColor currentCompanyColor = companyColors[companyName];
    size_t blockId = m_hilbertMap->registerNetworkBlock(ipExpression, companyName, currentCompanyColor);

    // Арбитр владения IP: предотвращает наложения подсетей и дубликаты (исключает статистику > 100%)
    auto assignIpSecure = [this, currentCompanyId, companyName, blockId](uint32_t idx) {
        if (idx >= 16777216) return;

        uint8_t oldCompanyId = m_ipOwnership[idx];

        // Защита: этот адрес уже принадлежит нам, повторно не суммируем
        if (oldCompanyId == currentCompanyId) return;

        // Перезапись: адрес перекуплен/переназначен. Вычитаем единицу у старого владельца
        if (oldCompanyId != 0) {
            QString oldCompanyName = m_idToCompany[oldCompanyId];
            if (m_companyStats[oldCompanyName] > 0) {
                m_companyStats[oldCompanyName]--;
            }
        }

        // Регистрируем чистый адрес на нового владельца
        m_companyStats[companyName]++;
        m_ipOwnership[idx] = currentCompanyId;
        m_hilbertMap->addIpPoint(idx, blockId);
        };

    // Анализ сценария 1: Работа с CIDR нотацией (напр. /16, /24) через битовые сдвиги масок
    if (ipExpression.contains('/')) {
        QStringList cidrParts = ipExpression.split('/');
        if (cidrParts.size() != 2) return false;

        uint32_t baseIdx = lambdaIpToUint(cidrParts[0].trimmed());
        int maskLength = cidrParts[1].toInt();
        if (baseIdx == 0xFFFFFFFF || maskLength < 8 || maskLength > 32) return false;

        int hostBits = 32 - maskLength;
        uint32_t totalAddresses = (1 << hostBits);
        uint32_t startIdx = baseIdx & ~((1 << hostBits) - 1);
        uint32_t endIdx = startIdx + totalAddresses;

        for (uint32_t idx = startIdx; idx < endIdx; ++idx) {
            assignIpSecure(idx);
        }
        return true;
    }
    // Анализ сценария 2: Прямой диапазон адресов через дефис
    else if (ipExpression.contains('-')) {
        QStringList rangeParts = ipExpression.split('-');
        if (rangeParts.size() != 2) return false;

        uint32_t startIdx = lambdaIpToUint(rangeParts[0].trimmed());
        uint32_t endIdx = lambdaIpToUint(rangeParts[1].trimmed());
        if (startIdx == 0xFFFFFFFF || endIdx == 0xFFFFFFFF || startIdx > endIdx) return false;

        for (uint32_t idx = startIdx; idx <= endIdx; ++idx) {
            assignIpSecure(idx);
        }
        return true;
    }
    // Анализ сценария 3: Единичный хост / целевой IP адрес
    else {
        uint32_t idx = lambdaIpToUint(ipExpression);
        if (idx == 0xFFFFFFFF) return false;

        assignIpSecure(idx);
        return true;
    }
}

// Сборка и инициализация интерфейса Qt (слои Layout, кнопки, сайдбар)
void MainWindow::setupUi() {
    setWindowTitle(QString::fromUtf8("Служба визуализации адресного пространства IPv4"));
    resize(1200, 850);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // Добавление кастомного виджета карты Гильберта (занимает приоритетное пространство окна)
    m_hilbertMap = new HilbertWidget(this);
    m_hilbertMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_hilbertMap, 3);

    // Формирование боковой панели управления (Sidebar)
    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(310);

    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(10, 0, 10, 0);
    sidebarLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel(QString::fromUtf8("ПАНЕЛЬ УПРАВЛЕНИЯ"), m_sidebar);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    sidebarLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QString::fromUtf8("Статус: Ожидание данных"), m_sidebar);
    sidebarLayout->addWidget(m_statusLabel);

    m_gridCheckbox = new QCheckBox(QString::fromUtf8("Отображать координатную сетку подсетей"), m_sidebar);
    m_gridCheckbox->setChecked(true);
    sidebarLayout->addWidget(m_gridCheckbox);

    QFrame* line1 = new QFrame(m_sidebar);
    line1->setFrameShape(QFrame::HLine);
    sidebarLayout->addWidget(line1);

    QLabel* filterLabel = new QLabel(QString::fromUtf8("Фильтр по категориям владельцев:"), m_sidebar);
    sidebarLayout->addWidget(filterLabel);

    m_filterCombo = new QComboBox(m_sidebar);
    m_filterCombo->addItem(QString::fromUtf8("Показать все подсети"));
    m_filterCombo->addItem(QString::fromUtf8("Крупные IT-Корпорации"));
    m_filterCombo->addItem(QString::fromUtf8("Государственные узлы"));
    m_filterCombo->addItem(QString::fromUtf8("Коммерческие провайдеры"));
    sidebarLayout->addWidget(m_filterCombo);

    QFrame* line2 = new QFrame(m_sidebar);
    line2->setFrameShape(QFrame::HLine);
    sidebarLayout->addWidget(line2);

    // Настройка верхнего малого экрана (Динамическая информация под мышкой)
    m_ipInfoLabel = new QLabel(QString::fromUtf8("<b>Информация об IP:</b><br>Наведите курсор на карту..."), m_sidebar);
    m_ipInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_ipInfoLabel->setMinimumHeight(60);
    m_ipInfoLabel->setWordWrap(true);
    m_ipInfoLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_ipInfoLabel->setMargin(8);
    sidebarLayout->addWidget(m_ipInfoLabel);

    // Настройка нижнего большого экрана (Глобальная аналитика всей базы данных)
    m_analyticsLabel = new QLabel(QString::fromUtf8("<b>Аналитика:</b><br>База данных пуста. Загрузите файл..."), m_sidebar);
    m_analyticsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_analyticsLabel->setMinimumHeight(200);
    m_analyticsLabel->setWordWrap(true);
    m_analyticsLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_analyticsLabel->setMargin(8);
    sidebarLayout->addWidget(m_analyticsLabel);

    sidebarLayout->addStretch(); // Пружина, сдвигающая кнопки управления к нижнему краю окна

    // Функциональные кнопки
    m_manualAddButton = new QPushButton(QString::fromUtf8("Добавить диапазон / CIDR вручную"), m_sidebar);
    m_manualAddButton->setMinimumHeight(35);
    sidebarLayout->addWidget(m_manualAddButton);

    m_loadDbButton = new QPushButton(QString::fromUtf8("Загрузить/Добавить файл подсетей"), m_sidebar);
    m_loadDbButton->setMinimumHeight(40);
    sidebarLayout->addWidget(m_loadDbButton);

    m_saveImageButton = new QPushButton(QString::fromUtf8("Сохранить карту в PNG (4096x4096)"), m_sidebar);
    m_saveImageButton->setMinimumHeight(35);
    sidebarLayout->addWidget(m_saveImageButton);

    m_clearButton = new QPushButton(QString::fromUtf8("Полная очистка карты"), m_sidebar);
    m_clearButton->setMinimumHeight(30);
    sidebarLayout->addWidget(m_clearButton);

    mainLayout->addWidget(m_sidebar, 1);
}

// Расчет емкостей, долей рынка в процентах и обновление экрана аналитики
void MainWindow::updateAnalyticsDisplay() {
    if (m_companyStats.empty()) {
        m_analyticsLabel->setText(QString::fromUtf8("<b>Аналитика:</b><br>База данных пуста. Загрузите файл..."));
        return;
    }

    uint32_t totalAllocated = 0;
    for (auto const& [name, count] : m_companyStats) {
        totalAllocated += count;
    }

    QString statsText = QString::fromUtf8("<b>АНАЛИТИКА АДРЕСНОГО ПРОСТРАНСТВА:</b><br>");
    statsText += QString::fromUtf8("Всего занято: <b>%1</b> адресов<br>---<br><b>РАСПРЕДЕЛЕНИЕ ДОЛЕЙ:</b><br>").arg(totalAllocated);

    // Расчет процентного соотношения от физического лимита адресной матрицы 10.0.0.0/8 (16 777 216 адресов)
    for (auto const& [name, count] : m_companyStats) {
        if (count == 0) continue;

        double percent = (static_cast<double>(count) / 16777216.0) * 100.0;
        statsText += QString::fromUtf8("• <b>Компания %1</b>:<br>  └ %2 IP (%3%)<br>")
            .arg(name)
            .arg(count)
            .arg(percent, 0, 'f', 4); // Выводим с точностью в 4 знака для мелких подсетей
    }
    m_analyticsLabel->setText(statsText);
}