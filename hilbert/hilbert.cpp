#include <iostream>
#include <iomanip>
#include <windows.h> 
#include "hilbert_math.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include "hilbert_math.h"

int main(int argc, char* argv[]) {
    // 1. Инициализируем главное Qt-приложение. 
    // Оно управляет всеми окнами, кликами мыши и памятью графики.
    QApplication app(argc, argv);

    // 2. Создаем базовый графический виджет (по сути, чистое окно)
    QWidget window;

    // Задаем окну стартовый размер (например, 800 на 600 пикселей)
    window.resize(800, 600);

    // Меняем заголовок окна
    window.setWindowTitle("Визуализатор IPv4 на кривой Гильберта");

    // 3. Приказываем окну появиться на экране
    window.show();

    // 4. Запускаем бесконечный цикл обработки событий Qt.
    // Программа будет жить до тех пор, пока пользователь не закроет окно на "крестик".
    return app.exec();
}