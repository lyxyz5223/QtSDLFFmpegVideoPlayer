#pragma once
#include <QWidget>
#include <QDialog>
#include "ui_PlayerOptionsWidget.h"

class PlayerOptionsWidget : public QDialog
{
    Q_OBJECT
public:
    PlayerOptionsWidget(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~PlayerOptionsWidget();

    void setPlaySpeed(double speed) {
        ui.doubleSpinBoxPlaySpeed->setValue(speed);
    }

    void setPlayerABLoopIntervalSideA(uint64_t milliseconds) {
        ui.timeEditPlayerABLoopIntervalSideA->setTime(QTime::fromMSecsSinceStartOfDay(milliseconds));
    }
    void setPlayerABLoopIntervalSideB(uint64_t milliseconds) {
        ui.timeEditPlayerABLoopIntervalSideB->setTime(QTime::fromMSecsSinceStartOfDay(milliseconds));
    }

signals:
    // Play options signals
    void stepBackLong();
    void stepBackShort();
    void stepForwardShort();
    void stepForwardLong();
    void playSpeedChange(double speed);
    void playSpeedReset();
    void playerABLoopIntervalSideASet();
    void playerABLoopIntervalSideBSet();
    void playerABLoopIntervalRemove();

    // Video options signals

    // Audio options signals
    void equalizerEnableStateChange(bool enable);
    void equalizerGainChange(size_t bandIndex, double gain);
    void equalizerGainsChange(const std::vector<double>& gains);

    // Subtitle options signals


protected slots:
    void playOptionStepBackLong();
    void playOptionStepBackShort();
    void playOptionStepForwardShort();
    void playOptionStepForwardLong();
    void playOptionPlaySpeedChange(double speed);
    void playOptionPlaySpeedReset();
    void playOptionPlayerABLoopIntervalSideASet();
    void playOptionPlayerABLoopIntervalSideBSet();
    void playOptionPlayerABLoopIntervalRemove();

    void audioOptionEqualizerStateChange(Qt::CheckState state) { emit equalizerEnableStateChange(state == Qt::CheckState::Checked); }
    void audioOptionEqualizerValueChange(size_t bandIndex, int value) { emit equalizerGainChange(bandIndex, value / 100.0); }
    void audioOptionEqualizerValue1Change(int value) { audioOptionEqualizerValueChange(0, value); }
    void audioOptionEqualizerValue2Change(int value) { audioOptionEqualizerValueChange(1, value); }
    void audioOptionEqualizerValue3Change(int value) { audioOptionEqualizerValueChange(2, value); }
    void audioOptionEqualizerValue4Change(int value) { audioOptionEqualizerValueChange(3, value); }
    void audioOptionEqualizerValue5Change(int value) { audioOptionEqualizerValueChange(4, value); }
    void audioOptionEqualizerValue6Change(int value) { audioOptionEqualizerValueChange(5, value); }
    void audioOptionEqualizerValue7Change(int value) { audioOptionEqualizerValueChange(6, value); }
    void audioOptionEqualizerValue8Change(int value) { audioOptionEqualizerValueChange(7, value); }
    void audioOptionEqualizerValue9Change(int value) { audioOptionEqualizerValueChange(8, value); }
    void audioOptionEqualizerValue10Change(int value) { audioOptionEqualizerValueChange(9, value); }

private:
    Ui::PlayerOptionsClass ui;

};