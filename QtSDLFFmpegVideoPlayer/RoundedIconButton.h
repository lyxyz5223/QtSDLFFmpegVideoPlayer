#pragma once
#include <QPushButton>
#include <QPen>
#include <QBrush>
#include <QPainterPath>

class RoundedIconButton : public QPushButton
{
    Q_OBJECT

public:
    RoundedIconButton(QWidget* parent = nullptr);
    ~RoundedIconButton();

    void setPen(QPen pen) {
        this->pen = pen;
    }
    QPen getPen() const {
        return pen;
    }
    void setBrush(QBrush brush) {
        this->brush = brush;
    }
    QBrush getBrush() const {
        return brush;
    }
    void setIconSize(QSize size) {
        QSize s(size);
        iconSize = s;
        constexpr double scaleValue = 1.5;
        QPushButton::setIconSize(QSize(s.width() * scaleValue, s.height() * scaleValue));
    }
    void setIconSize(int size) {
        setIconSize(QSize(size, size));
    }

    //setIconSize设置图标大小，但图标实际绘制大小由下面的iconSize成员变量控制
    void setIconRealSize(int size) {
        iconSize = QSize(size, size);
    }
    void setIconRealSize(QSize size) {
        iconSize = size;
    }
    void setText(const QString& text) {

    }
    QString text() const {

    }
    bool hitTest(const QPoint& pos) const;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent* e) override;
    bool hitButton(const QPoint& pos) const override;

signals:

public slots:

private:
    QPen pen{ Qt::NoPen };
    QBrush brush{ QColor(238, 180, 180) };
    //QBrush brush{ Qt::NoBrush };
    QSize iconSize{ 16, 16 };
    QPixmap iconPixmap{ icon().pixmap(iconSize, QIcon::Mode::Normal, QIcon::State::Off) };
    QRegion btnMask;
    qreal xRadius = qMin(width(), height()) / 2.0;
    qreal yRadius = qMin(width(), height()) / 2.0;
    QRect calcIconRect() const {
        int x = (width() - iconSize.width()) / 2;
        int y = (height() - iconSize.height()) / 2;
        return QRect(x, y, iconSize.width(), iconSize.height());
    }
    Qt::SizeMode roundMode = Qt::AbsoluteSize;
    QPainterPath roundedBtnPath;
};

