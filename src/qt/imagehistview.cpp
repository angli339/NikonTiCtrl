#include "qt/imagehistview.h"

#include <cmath>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

extern const uint8_t cmap_data[];

ImageHistView::ImageHistView(int n_bins, std::pair<int, int> range,
                             QWidget *parent)
    : QWidget(parent)
{
    this->n_bins = n_bins;
    this->range = range;
    this->vmin = range.first;
    this->vmax = range.second;
}

void ImageHistView::setData(std::vector<double> data)
{
    this->data = data;
    update();
}

void ImageHistView::setVmin(int vmin)
{
    this->vmin = vmin;
    emit vminChanged(vmin);
    update();
}

void ImageHistView::setVmax(int vmax)
{
    this->vmax = vmax;
    emit vmaxChanged(vmax);
    update();
}

void ImageHistView::setCmap(int icmap)
{
    this->icmap = icmap;
    update();
}

void ImageHistView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(histPen());
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(histPath());

    painter.setPen(Qt::NoPen);
    painter.setBrush(cbarBrush());
    painter.drawRect(cbarRect());

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::lightGray);
    painter.drawPolygon(vminSliderPolygon());
    painter.drawPolygon(vmaxSliderPolygon());

    painter.setPen(QPen(Qt::lightGray, 0.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(vminLine());
    painter.drawLine(vmaxLine());
}

QPainterPath ImageHistView::histPath()
{
    double xmax = range.second;
    double xmin = range.first;
    double ymax = 1;
    double ymin = 0;

    double xrange = xmax - xmin;
    double yrange = ymax - ymin;
    double xscale = (geometry().width() - sliderWidth * 2) / xrange;
    double xoffset = -xmin * xscale + sliderWidth;
    double yscale =
        -(geometry().height() - cbarHeight - cbarMarginTop) / yrange;
    double yoffset =
        geometry().height() - cbarHeight - cbarMarginTop - ymin * yscale;

    double bin_size = (xmax + 1 - xmin) / n_bins;

    QPainterPath path;

    for (int i = 0; i < data.size(); i++) {
        double x = xmin + bin_size * ((double)i + 0.5);
        double y = data[i];
        double x_transformed = x * xscale + xoffset;
        double y_transformed = y * yscale + yoffset;

        if (i == 0) {
            path.moveTo(x_transformed, y_transformed);
        } else {
            path.lineTo(x_transformed, y_transformed);
        }
    }

    return path;
}

QPen ImageHistView::histPen()
{
    QPen pen;
    pen.setWidthF(linewidth);
    pen.setBrush(QBrush(Qt::SolidPattern));
    pen.setColor(Qt::lightGray);
    return pen;
}

QBrush ImageHistView::cbarBrush()
{
    QLinearGradient cmap;
    cmap.setCoordinateMode(QGradient::ObjectMode);
    cmap.setStart(0, 0.5);
    cmap.setFinalStop(1, 0.5);

    for (int i = 0; i < 256; i++) {
        cmap.setColorAt((double)(i + 0.5) / 256,
                        QColor(cmap_data[icmap*256*3 + 3 * i],
                               cmap_data[icmap*256*3 + 3 * i + 1],
                               cmap_data[icmap*256*3 + 3 * i + 2]));
    }
    return QBrush(cmap);
}

QRectF ImageHistView::cbarRect()
{
    double pos_vmin = valueToPos(vmin);
    double pos_vmax = valueToPos(vmax);

    return QRectF(pos_vmin, geometry().height() - cbarHeight,
                  pos_vmax - pos_vmin, cbarHeight);
}

double ImageHistView::valueToPos(double value)
{
    double xmin = range.first;
    double xmax = range.second;
    double xrange = xmax - xmin;
    double xscale = (geometry().width() - sliderWidth * 2) / xrange;
    double xoffset = -xmin * xscale + sliderWidth;
    return value * xscale + xoffset;
}

double ImageHistView::posToValue(double pos)
{
    double xmin = range.first;
    double xmax = range.second;
    double xrange = xmax - xmin;
    double xscale = (geometry().width() - sliderWidth * 2) / xrange;
    double xoffset = -xmin * xscale + sliderWidth;
    return (pos - xoffset) / xscale;
}

QPolygonF ImageHistView::vminSliderPolygon()
{
    double x2 = valueToPos(vmin);
    double x1 = x2 - sliderWidth;
    double y1 = geometry().height() - cbarHeight;
    double y2 = y1 + cbarHeight;
    double y3 = y1 + cbarHeight / 3;

    return QPolygonF({{x1, y3}, {x1, y2}, {x2, y2}, {x2, y1}, {x1, y3}});
}

QPolygonF ImageHistView::vmaxSliderPolygon()
{
    double x1 = valueToPos(vmax);
    double x2 = x1 + sliderWidth;
    double y1 = geometry().height() - cbarHeight;
    double y2 = y1 + cbarHeight;
    double y3 = y1 + cbarHeight / 3;

    return QPolygonF({{x1, y1}, {x1, y2}, {x2, y2}, {x2, y3}, {x1, y1}});
}

QLineF ImageHistView::vminLine()
{
    double pos = valueToPos(vmin);
    return QLineF(pos, 0, pos, geometry().height() - cbarHeight);
}

QLineF ImageHistView::vmaxLine()
{
    double pos = valueToPos(vmax);
    return QLine(pos, 0, pos, geometry().height() - cbarHeight);
}

void ImageHistView::mousePressEvent(QMouseEvent *ev)
{
    QPolygonF vminSlider = vminSliderPolygon();
    QPolygonF vmaxSlider = vmaxSliderPolygon();

    if (ev->buttons() & Qt::LeftButton) {
        vminSliderPressed =
            vminSlider.containsPoint(ev->pos(), Qt::OddEvenFill);
        vmaxSliderPressed =
            vmaxSlider.containsPoint(ev->pos(), Qt::OddEvenFill);
        if (vminSliderPressed) {
            vminSliderPressedOffset = valueToPos(vmin) - ev->pos().x();
        }
        if (vmaxSliderPressed) {
            vmaxSliderPressedOffset = ev->pos().x() - valueToPos(vmax);
        }
    }
}

void ImageHistView::mouseMoveEvent(QMouseEvent *ev)
{
    if (ev->buttons() & Qt::LeftButton) {
        if (vminSliderPressed) {
            double pos = ev->pos().x() + vminSliderPressedOffset;
            int value = std::round(posToValue(pos));
            if (value < range.first) {
                value = range.first;
            }
            if (value > vmax) {
                value = vmax;
            }
            if (vmin != value) {
                setVmin(value);
            }
        }
        if (vmaxSliderPressed) {
            int pos = ev->pos().x() - vmaxSliderPressedOffset;
            int value = std::round(posToValue(pos));
            if (value > range.second) {
                value = range.second;
            }

            if (value < vmin) {
                value = vmin;
            }
            if (vmax != value) {
                setVmax(value);
            }
        }
    }
}

void ImageHistView::mouseReleaseEvent(QMouseEvent *ev)
{
    vminSliderPressed = false;
    vmaxSliderPressed = false;
}
