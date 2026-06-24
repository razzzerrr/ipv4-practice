#include <iostream>
#include <iomanip>
#include <windows.h> 
#include "hilbert_math.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include "hilbert_math.h"
#include "hilbert_widget.h"
#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Запускаем полноценное главное окно вместо одиночного холста
    MainWindow window;
    window.show();

    return app.exec();
}