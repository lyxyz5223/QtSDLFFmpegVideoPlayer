#pragma once
#include <qaction.h>

class AnimatedMenuAction :
    public QAction
{
    Q_OBJECT
public:
    ~AnimatedMenuAction() {}
    AnimatedMenuAction(QObject* parent = nullptr);
    AnimatedMenuAction(const QString& text, QObject* parent = nullptr)
        : QAction(text, parent) {}
    AnimatedMenuAction(const QIcon& icon, const QString& text, QObject* parent = nullptr)
        : QAction(icon, text, parent) {}
    
    //没啥用
    void setTipText(QString text) {
        tipText = text;
    }
    QString getTipText() const {
        return tipText;
    }
protected:
    AnimatedMenuAction(QActionPrivate& dd, QObject* parent) : QAction(dd, parent) {}
    bool event(QEvent* e) override;
private:
    QString tipText;
};

