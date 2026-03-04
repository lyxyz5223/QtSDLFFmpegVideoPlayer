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

void PlayerOptionsWidget::videoOptionVideoBrightnessReset()
{
    ui.horizontalSliderVideoBrightness->setValue(0);
    emit videoBrightnessReset();
}

void PlayerOptionsWidget::videoOptionVideoContrastReset()
{
    ui.horizontalSliderVideoContrast->setValue(0);
    emit videoContrastReset();
}

void PlayerOptionsWidget::videoOptionVideoSaturationReset()
{
    ui.horizontalSliderVideoSaturation->setValue(0);
    emit videoSaturationReset();
}

void PlayerOptionsWidget::videoOptionVideoChromaticityReset()
{
    ui.horizontalSliderVideoChromaticity->setValue(0);
    emit videoChromaticityReset();
}



