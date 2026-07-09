#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_hilbert_visual.h"

class hilbert_visual : public QMainWindow
{
    Q_OBJECT

public:
    hilbert_visual(QWidget *parent = nullptr);
    ~hilbert_visual();

private:
    Ui::hilbert_visualClass ui;
};

