#include "main_window.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();

    // Передача данных от карты к текстовой панели
    connect(m_hilbertMap, &HilbertWidget::ipHovered, m_ipInfoLabel, &QLabel::setText);

    // Логика кнопки "Загрузить базу" через выбор файла
    connect(m_loadDbButton, &QPushButton::clicked, this, [=]() {
        QString filePath = QFileDialog::getOpenFileName(
            this,
            "Открыть файл базы угроз",
            "",
            "Лог-файлы (*.txt *.log);;Все файлы (*.*)"
        );

        if (!filePath.isEmpty()) {
            if (m_hilbertMap->loadDatabaseFromFile(filePath)) {
                QFileInfo fileInfo(filePath);
                m_statusLabel->setText(QString("Статус: <b>Загружен %1</b>").arg(fileInfo.fileName()));
                m_statusLabel->setStyleSheet("color: #27AE60;");
            }
            else {
                m_statusLabel->setText("Статус: <span style='color:#E74C3C;'>Ошибка чтения файла!</span>");
            }
        }
        });

    // Логика переключения фильтра в выпадающем списке
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        m_hilbertMap->setFilterType(index);
        });

    // Логика очистки
    connect(m_clearButton, &QPushButton::clicked, this, [=]() {
        m_hilbertMap->clearMap();
        m_filterCombo->setCurrentIndex(0); // Сбрасываем фильтр в положение "Все"
        m_statusLabel->setText("Статус: Ожидание загрузки базы");
        m_statusLabel->setStyleSheet("color: #7F8C8D; font-style: italic;");
        m_ipInfoLabel->setText("<b>Информация об IP:</b><br>Наведите курсор на карту...");
        });
}

void MainWindow::setupUi() {
    setWindowTitle("Служба визуализации IPv4 пространства");
    resize(1150, 800);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // Карта Гильберта
    m_hilbertMap = new HilbertWidget(this);
    m_hilbertMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_hilbertMap, 3);

    // Боковая панель
    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(290);
    m_sidebar->setObjectName("sidebarPanel"); // Имя для QSS

    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(10, 10, 10, 10);
    sidebarLayout->setSpacing(15);

    // Заголовок сайдбара
    QLabel* titleLabel = new QLabel("ПАНЕЛЬ УПРАВЛЕНИЯ", m_sidebar);
    titleLabel->setObjectName("sidebarTitle");
    sidebarLayout->addWidget(titleLabel);

    // Статус загрузки
    m_statusLabel = new QLabel("Статус: Ожидание загрузки базы", m_sidebar);
    m_statusLabel->setObjectName("statusLabel");
    sidebarLayout->addWidget(m_statusLabel);

    QFrame* line1 = new QFrame(m_sidebar);
    line1->setObjectName("separatorLine");
    line1->setFrameShape(QFrame::HLine);
    sidebarLayout->addWidget(line1);

    // --- БЛОК ФИЛЬТРАЦИИ ---
    QLabel* filterLabel = new QLabel("Фильтр по типу угроз:", m_sidebar);
    filterLabel->setObjectName("filterLabel");
    sidebarLayout->addWidget(filterLabel);

    m_filterCombo = new QComboBox(m_sidebar);
    m_filterCombo->addItem("🔍 Отображать всё");
    m_filterCombo->addItem("🔴 Только DDoS-Атаки");
    m_filterCombo->addItem("🟡 Только сканирование портов");
    m_filterCombo->addItem("🟣 Только активность ботнетов");
    sidebarLayout->addWidget(m_filterCombo);

    QFrame* line2 = new QFrame(m_sidebar);
    line2->setObjectName("separatorLine");
    line2->setFrameShape(QFrame::HLine);
    sidebarLayout->addWidget(line2);

    // Инфо-блок (куда выводится IP под курсором)
    m_ipInfoLabel = new QLabel("<b>Информация об IP:</b><br>Наведите курсор на карту...", m_sidebar);
    m_ipInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_ipInfoLabel->setObjectName("infoLabel"); // Имя для QSS
    m_ipInfoLabel->setMinimumHeight(180);
    m_ipInfoLabel->setWordWrap(true); // Чтобы длинный текст не обрезался
    sidebarLayout->addWidget(m_ipInfoLabel);

    sidebarLayout->addStretch();

    // Кнопка загрузки
    m_loadDbButton = new QPushButton("Загрузить базу адресов", m_sidebar);
    m_loadDbButton->setMinimumHeight(40);
    m_loadDbButton->setObjectName("loadButton");
    sidebarLayout->addWidget(m_loadDbButton);

    // Кнопка очистки
    m_clearButton = new QPushButton("Очистить карту", m_sidebar);
    m_clearButton->setMinimumHeight(30);
    m_clearButton->setObjectName("clearButton");
    sidebarLayout->addWidget(m_clearButton);

    mainLayout->addWidget(m_sidebar, 1);

    // =========================================================================
    // 🎨 ЕДИНАЯ ТАБЛИЦА СТИЛЕЙ (QSS) ДЛЯ СОВРЕМЕННОГО ТЁМНОГО ИНТЕРФЕЙСА
    // =========================================================================
    this->setStyleSheet(R"(
        /* Главное окно и задний фон */
        QMainWindow, QWidget#centralWidget {
            background-color: #1e222b;
        }

        /* Боковая панель */
        QWidget#sidebarPanel {
            background-color: #21252b;
            border: 1px solid #2d3139;
            border-radius: 6px;
        }

        /* Заголовок панели */
        QLabel#sidebarTitle {
            font-size: 13px;
            font-weight: bold;
            color: #4b5263;
            letter-spacing: 1px;
        }

        /* Обычные метки текста */
        QLabel, QLabel#filterLabel {
            color: #abb2bf;
            font-family: "Segoe UI", sans-serif;
            font-size: 13px;
        }

        /* Статус-бар */
        QLabel#statusLabel {
            color: #5c6370;
            font-style: italic;
        }

        /* Разделительные линии */
        QFrame#separatorLine {
            background-color: #2d3139;
            max-height: 1px;
            border: none;
        }

        /* Выпадающий список (Фильтр) */
        QComboBox {
            background-color: #282c34;
            color: #abb2bf;
            border: 1px solid #3e4451;
            border-radius: 4px;
            padding: 6px 10px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #282c34;
            color: #abb2bf;
            selection-background-color: #3e4451;
            border: 1px solid #3e4451;
        }

        /* Окно информации об IP */
        QLabel#infoLabel {
            background-color: #282c34;
            border: 1px solid #2d3139;
            border-radius: 4px;
            padding: 12px;
            color: #abb2bf;
            font-family: "Consolas", "Courier New", monospace; /* Моноширинный шрифт для IP */
            font-size: 13px;
        }

        /* Основная кнопка (Загрузка) */
        QPushButton#loadButton {
            background-color: #2ecc71;
            color: #ffffff;
            font-weight: bold;
            border: none;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton#loadButton:hover {
            background-color: #27ae60;
        }
        QPushButton#loadButton:pressed {
            background-color: #219653;
        }

        /* Второстепенная кнопка (Очистить) */
        QPushButton#clearButton {
            background-color: #3a3f4b;
            color: #abb2bf;
            border: none;
            border-radius: 4px;
            font-size: 12px;
        }
        QPushButton#clearButton:hover {
            background-color: #4b5263;
            color: #ffffff;
        }
        QPushButton#clearButton:pressed {
            background-color: #2c313c;
        }
    )");
}