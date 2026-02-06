#include "PlayerOptionsWidget.h"

PlayerOptionsWidget::PlayerOptionsWidget(QWidget* parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
{
    ui.setupUi(this);
}

PlayerOptionsWidget::~PlayerOptionsWidget()
{
}

void PlayerOptionsWidget::playOptionStepBackLong()
{
    emit stepBackLong();
}

void PlayerOptionsWidget::playOptionStepBackShort()
{
    emit stepBackShort();
}

void PlayerOptionsWidget::playOptionStepForwardLong()
{
    emit stepForwardLong();
}

void PlayerOptionsWidget::playOptionStepForwardShort()
{
    emit stepForwardShort();
}

void PlayerOptionsWidget::playOptionPlaySpeedChange(double speed)
{
    emit playSpeedChange(speed);
}

void PlayerOptionsWidget::playOptionPlaySpeedReset()
{
    ui.doubleSpinBoxPlaySpeed->setValue(1.0);
    emit playSpeedReset();
}

void PlayerOptionsWidget::playOptionPlayerABLoopIntervalSideASet()
{
    emit playerABLoopIntervalSideASet();
}


void PlayerOptionsWidget::playOptionPlayerABLoopIntervalSideBSet()
{
    emit playerABLoopIntervalSideBSet();
}


void PlayerOptionsWidget::playOptionPlayerABLoopIntervalRemove()
{
    emit playerABLoopIntervalRemove();
}


