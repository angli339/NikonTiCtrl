#ifndef QT_IMAGEHISTVIEW_H
#define QT_IMAGEHISTVIEW_H

#include <utility>
#include <vector>

#include <QMouseEvent>
#include <QWidget>

class ImageHistView : public QWidget {
    Q_OBJECT
public:
    explicit ImageHistView(int n_bins, std::pair<int, int> range,
                            QWidget *parent = nullptr);

    QSize sizeHint() const override
    {
        return QSize(300, 50);
    }

public slots:
    void setData(std::vector<double> data);
    void setVmin(int vmin);
    void setVmax(int vmax);

signals:
    void vminChanged(int vmin);
    void vmaxChanged(int vmax);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    int n_bins;
    std::pair<int, int> range;
    std::vector<double> data;

    int vmin;
    int vmax;

    bool vminSliderPressed;
    double vminSliderPressedOffset;
    bool vmaxSliderPressed;
    double vmaxSliderPressedOffset;

    double linewidth = 1.5;
    int cbarHeight = 18;
    int cbarMarginTop = 3;
    int sliderWidth = 12;

    double valueToPos(double value);
    double posToValue(double pos);

    QPainterPath histPath();
    QPen histPen();
    QBrush cbarBrush();
    QRectF cbarRect();

    QPolygonF vminSliderPolygon();
    QPolygonF vmaxSliderPolygon();
    QLineF vminLine();
    QLineF vmaxLine();
};

#endif // QT_IMAGEHISTVIEW_H
