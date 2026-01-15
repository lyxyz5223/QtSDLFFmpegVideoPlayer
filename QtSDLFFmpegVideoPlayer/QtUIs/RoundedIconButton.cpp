#include "RoundedIconButton.h"
#include <QPainter>
#include <QPicture>
#include <QPixmap>
#include <QRegion>
#include <QResizeEvent>

RoundedIconButton::RoundedIconButton(QWidget* parent)
    : QPushButton(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFlat(true);
    setMouseTracking(true);
}

RoundedIconButton::~RoundedIconButton()
{

}

void RoundedIconButton::paintEvent(QPaintEvent* e)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(pen);
    //auto& btnBkg = palette().brush(QPalette::Button);
    auto btnBkg = bkgBrush;
    painter.setBrush(btnBkg);
    //painter.setBrush(QColor(238, 180, 180));
    //painter.drawRoundedRect(rect(), xRadius, yRadius, roundMode);
    painter.drawPath(roundedBtnPath);
    // 绘制图标，需更新iconPixmap，否则图标改变后不会刷新
    QColor iconColor = normalIconColor;
    if (mousePressed || isChecked())
        iconColor = selectedIconColor;
    else if (mouseHovered)
        iconColor = hoveredIconColor;
    iconPixmap = iconToColoredPixmap(icon(), iconSize, iconColor);
    //iconPixmap = icon().pixmap(iconSize, QIcon::Mode::Normal, QIcon::State::Off);
    painter.drawPixmap(calcIconRect(), iconPixmap);
}

void RoundedIconButton::resizeEvent(QResizeEvent* e)
{
    auto&& newSize = e->size();
    xRadius = yRadius = qMin(newSize.width(), newSize.height()) / 2.0;
    roundedBtnPath.clear();

    QRectF newRect(0, 0, newSize.width(), newSize.height());
    if (newSize.width() > newSize.height()) // height小
        newRect = QRectF((newSize.width() - newSize.height()) / 2.0, 0, newSize.height(), newSize.height());
    else // width小
        newRect = QRectF(0, (newSize.height() - newSize.width()) / 2.0, newSize.width(), newSize.width());
    roundedBtnPath.addRoundedRect(newRect, xRadius, yRadius, roundMode);
    iconPixmap = icon().pixmap(iconSize, QIcon::Mode::Normal, QIcon::State::Off);
}

bool RoundedIconButton::hitButton(const QPoint& pos) const
{
    return hitTest(pos);
}

void RoundedIconButton::mouseMoveEvent(QMouseEvent* e)
{
    if (hitTest(e->pos()))
        mouseHovered = true;
    else
        mouseHovered = false;
    QPushButton::mouseMoveEvent(e);
}

void RoundedIconButton::mousePressEvent(QMouseEvent* e)
{
    mousePressed = true;
    QPushButton::mousePressEvent(e);
}

void RoundedIconButton::mouseReleaseEvent(QMouseEvent* e)
{
    mousePressed = false;
    QPushButton::mouseReleaseEvent(e);
}

void RoundedIconButton::enterEvent(QEnterEvent* event)
{
    if (hitTest(event->position().toPoint()))
        mouseHovered = true;
    QPushButton::enterEvent(event);
}

void RoundedIconButton::leaveEvent(QEvent* event)
{
    mouseHovered = false;
    QPushButton::leaveEvent(event);
}

QPixmap RoundedIconButton::iconToColoredPixmap(const QIcon& icon, QSize size, QColor color)
{
    QPixmap pixmap = icon.pixmap(size);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    QIcon colorIcon = QIcon(pixmap);
    return pixmap;
}

bool RoundedIconButton::hitTest(const QPoint& pos) const
{
    // 简单圆角矩形命中测试
    return roundedBtnPath.contains(pos);
}