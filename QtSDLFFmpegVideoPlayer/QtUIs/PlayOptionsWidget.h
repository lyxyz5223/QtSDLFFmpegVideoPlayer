#pragma once
#include <QWidget>
#include <QDialog>
#include "ui_PlayOptionsWidget.h"

class PlayOptionsWidget : public QDialog
{
    Q_OBJECT
public:
    PlayOptionsWidget(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~PlayOptionsWidget();

private:
    Ui::PlayOptionsClass ui;

};