#include "AnimatedMenuAction.h"
#include <qevent.h>
#include <QPainter>
#include <qwidget.h>

AnimatedMenuAction::AnimatedMenuAction(QObject* parent) : QAction(parent)
{

}

bool AnimatedMenuAction::event(QEvent* e)
{
	
	return false;
}
