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
    void setBackgroundBrush(QBrush brush) {
        this->bkgBrush = brush;
    }
    QBrush getBackgroundBrush() const {
        return bkgBrush;
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
    virtual void paintEvent(QPaintEvent*) override;
    virtual void resizeEvent(QResizeEvent* e) override;
    virtual bool hitButton(const QPoint& pos) const override;
    virtual void mouseMoveEvent(QMouseEvent* e) override;
    virtual void mousePressEvent(QMouseEvent* e) override;
    virtual void mouseReleaseEvent(QMouseEvent* e) override;
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    
signals:

public slots:

private:
    QPen pen{ Qt::NoPen };
    QBrush bkgBrush{ QColor(238, 180, 180, 0) };
    QColor normalIconColor{ Qt::black };
    QColor hoveredIconColor{ QColor(205, 183, 181) }; // MistyRose3
    QColor selectedIconColor{ QColor(139, 125, 123) }; // MistyRose4
    //QColor hoveredIconColor{ QColor(255, 240, 245) }; // LavenderBlush
    //QColor selectedIconColor{ QColor(255, 228, 225) }; // MistyRose
    bool mousePressed{ false };
    bool mouseHovered{ false };

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
    QPixmap iconToColoredPixmap(const QIcon& icon, QSize size, QColor color);
};

