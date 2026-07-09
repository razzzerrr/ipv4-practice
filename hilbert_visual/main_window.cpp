#include "main_window.h"
#include "ip_parser.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QInputDialog>
#include <QHostAddress>
#include <QStringList>
#include <QHeaderView> // Необходим для настройки заголовков таблицы

// Конструктор главного окна: подготавливаем ОЗУ-базу уникального владения адресами
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // 16 777 216 ячеек под трекер (покрывает абсолютно всё мировое пространство IPv4 на уровне подсетей /24)
    m_ipOwnership.assign(16777216, 0);
    m_idToCompany.push_back(QString::fromUtf8("Свободно")); // ID = 0 жестко закреплен за нераспределенным пространством

    setupUi(); // Инициализация графического интерфейса

    // Логика связи: отправляем текст-карточку из виджета карты прямо в лейбл сайдбара
    connect(m_hilbertMap, &HilbertWidget::ipHovered, m_ipInfoLabel, &QLabel::setText);

    // Чекбокс сетки: связываем состояние интерфейса со свойством графического движка
    connect(m_gridCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
        m_hilbertMap->setGridVisible(checked);
        });

    // Умный динамический поиск по таблице (без перезагрузки данных)
    connect(m_searchEdit, &QLineEdit::textChanged, this, [=](const QString& text) {
        for (int i = 0; i < m_analyticsTable->rowCount(); ++i) {
            QTableWidgetItem* item = m_analyticsTable->item(i, 0); // Проверяем колонку "Компания"
            if (item) {
                bool matches = item->text().contains(text, Qt::CaseInsensitive);
                m_analyticsTable->setRowHidden(i, !matches); // Скрываем строку, если нет совпадений
            }
        }
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

        m_searchEdit->clear();
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
            stream.setEncoding(QStringConverter::Utf8); // Корректно читаем кириллицу

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

            m_hilbertMap->refreshMap(); // Перерисовываем карту ОДИН раз после парсинга файла
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
            QString::fromUtf8("Введите подсеть и компанию через ';' или просто адрес:\nПример: 8.8.8.0/24; Google\nИли диапазон: 185.0.0.0-185.5.255.255; Яндекс\nИли сокращенный CIDR: 192./16; Локалка"),
            QLineEdit::Normal, "192.168.0.0/16; Локальные сети", &ok);

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

// Интеллектуальный разборщик форматов ввода глобального IPv4 пространства
bool MainWindow::parseAndAddLine(const QString& line) {
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith('#')) return false;

    QStringList parts = trimmed.split(';');
    if (parts.isEmpty()) return false;

    QString ipExpression = parts[0].trimmed();
    QString companyName = (parts.size() > 1) ? parts[1].trimmed() : QString::fromUtf8("Неизвестный провайдер");

    // Выделение и сохранение числового ID для новых компаний (защита ОЗУ)
    if (m_companyToId.find(companyName) == m_companyToId.end()) {
        if (m_idToCompany.size() >= 255) return false; // Ограничение емкости uint8_t
        uint8_t newId = static_cast<uint8_t>(m_idToCompany.size());
        m_companyToId[companyName] = newId;
        m_idToCompany.push_back(companyName);
    }
    uint8_t currentCompanyId = m_companyToId[companyName];

    // Автоматическая генерация уникальной палитры цветов для провайдеров
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

    // Арбитр безопасного владения адресами (исключает наложения подсетей)
    auto assignIpSecure = [this, currentCompanyId, companyName, blockId](uint32_t subnetIdx) {
        if (subnetIdx >= 16777216) return;

        uint8_t oldCompanyId = m_ipOwnership[subnetIdx];
        if (oldCompanyId == currentCompanyId) return; // Уже принадлежит нам

        // Перезапись: если адрес перекуплен, вычитаем у старого владельца
        if (oldCompanyId != 0) {
            QString oldCompanyName = m_idToCompany[oldCompanyId];
            if (m_companyStats[oldCompanyName] > 0) {
                m_companyStats[oldCompanyName]--;
            }
        }

        m_companyStats[companyName]++;
        m_ipOwnership[subnetIdx] = currentCompanyId;
        m_hilbertMap->addIpPoint(subnetIdx, blockId);
        };

    // --- МОДЕРНИЗАЦИЯ: Делегируем весь кривой парсинг нашему новому IpParser ---
    bool parseSuccess = false;
    std::vector<uint32_t> blocks24 = IpParser::parseStringTo24Blocks(ipExpression, parseSuccess);

    if (!parseSuccess || blocks24.empty()) {
        return false; // Запись битая или не прошла валидацию QHostAddress внутри парсера
    }

    // Спокойно прокручиваем все /24 индексы, которые сгенерировал парсер
    for (uint32_t idx : blocks24) {
        assignIpSecure(idx);
    }

    return true;
}

// Сборка и инициализация интерфейса Qt (слои Layout, кнопки, сайдбар)
void MainWindow::setupUi() {
    setWindowTitle(QString::fromUtf8("Служба глобальной визуализации интернет-пространства IPv4"));
    resize(1250, 850);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    m_hilbertMap = new HilbertWidget(this);
    m_hilbertMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_hilbertMap, 3);

    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(500);

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

    m_ipInfoLabel = new QLabel(QString::fromUtf8("<b>Информация об IP:</b><br>Наведите курсор на карту..."), m_sidebar);
    m_ipInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_ipInfoLabel->setMinimumHeight(75);
    m_ipInfoLabel->setWordWrap(true);
    m_ipInfoLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_ipInfoLabel->setMargin(8);
    sidebarLayout->addWidget(m_ipInfoLabel);

    QFrame* line2 = new QFrame(m_sidebar);
    line2->setFrameShape(QFrame::HLine);
    sidebarLayout->addWidget(line2);

    QLabel* tableTitleLabel = new QLabel(QString::fromUtf8("<b>ГЛОБАЛЬНОЕ РАСПРЕДЕЛЕНИЕ ДОЛЕЙ:</b>"), m_sidebar);
    sidebarLayout->addWidget(tableTitleLabel);

    QHBoxLayout* tableAndSearchLayout = new QHBoxLayout();
    tableAndSearchLayout->setSpacing(10);

    QVBoxLayout* searchContainerLayout = new QVBoxLayout();
    QLabel* searchLabel = new QLabel(QString::fromUtf8("<b>Поиск:</b>"), m_sidebar);
    m_searchEdit = new QLineEdit(m_sidebar);
    m_searchEdit->setPlaceholderText(QString::fromUtf8("Компания..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(120);

    searchContainerLayout->addWidget(searchLabel);
    searchContainerLayout->addWidget(m_searchEdit);
    searchContainerLayout->addStretch();
    tableAndSearchLayout->addLayout(searchContainerLayout);

    m_analyticsTable = new QTableWidget(m_sidebar);
    m_analyticsTable->setColumnCount(3);

    QStringList headers;
    headers << QString::fromUtf8("Компания") << QString::fromUtf8("Подсети") << QString::fromUtf8("Доля %");
    m_analyticsTable->setHorizontalHeaderLabels(headers);

    m_analyticsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_analyticsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_analyticsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_analyticsTable->verticalHeader()->setVisible(false);
    m_analyticsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_analyticsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_analyticsTable->setSortingEnabled(true);
    m_analyticsTable->setMinimumHeight(250);

    m_analyticsTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_analyticsTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    tableAndSearchLayout->addWidget(m_analyticsTable, 1);
    sidebarLayout->addLayout(tableAndSearchLayout);

    sidebarLayout->addStretch();

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

// Расчет емкостей, долей глобального рынка и заполнение интерактивной таблицы
void MainWindow::updateAnalyticsDisplay() {
    m_analyticsTable->setSortingEnabled(false);
    m_analyticsTable->setRowCount(0);

    if (m_companyStats.empty()) {
        m_analyticsTable->setSortingEnabled(true);
        return;
    }

    int row = 0;
    for (auto const& [name, count] : m_companyStats) {
        if (count == 0) continue;

        m_analyticsTable->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        m_analyticsTable->setItem(row, 0, nameItem);

        QTableWidgetItem* countItem = new QTableWidgetItem();
        countItem->setData(Qt::DisplayRole, static_cast<uint32_t>(count));
        m_analyticsTable->setItem(row, 1, countItem);

        double percent = (static_cast<double>(count) / 16777216.0) * 100.0;
        QTableWidgetItem* percentItem = new QTableWidgetItem();
        percentItem->setData(Qt::DisplayRole, qRound(percent * 10000.0) / 10000.0);
        m_analyticsTable->setItem(row, 2, percentItem);

        row++;
    }

    m_analyticsTable->setSortingEnabled(true);
    m_analyticsTable->sortByColumn(1, Qt::DescendingOrder);

    QString searchText = m_searchEdit->text();
    if (!searchText.isEmpty()) {
        for (int i = 0; i < m_analyticsTable->rowCount(); ++i) {
            QTableWidgetItem* item = m_analyticsTable->item(i, 0);
            if (item) {
                m_analyticsTable->setRowHidden(i, !item->text().contains(searchText, Qt::CaseInsensitive));
            }
        }
    }
}