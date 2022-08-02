#ifndef PLATESAMPLEWIDGET_H
#define PLATESAMPLEWIDGET_H

#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWidget>
#include <map>

class GraphicsWellItem;

class PlateSampleWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlateSampleWidget(QWidget *parent = nullptr);
    void setPlateType(QString plate_type);

    void zoomCenter(double rel_x0, double rel_y0);
    void zoomCenter(QString well_id);
    void zoomFit();

    void setFOVSize(double fov_width, double fov_height);
    void setFOVPos(double rel_x0, double rel_y0);
    void setWellHighlighted(QString well_id, bool highlighted);

    QSize sizeHint() const;

protected:
    void resizeEvent(QResizeEvent *event);

private:
    void clear();
    void initScene();
    QGraphicsSimpleTextItem *addColRowLabel(QString text, double x0, double y0);

public:
    QGraphicsView *graphView = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsRectItem *fov_rect = nullptr;
    QGraphicsEllipseItem *fn_circle = nullptr;

    QList<QGraphicsSimpleTextItem *> rowlabels;
    QList<QGraphicsSimpleTextItem *> collabels;
    QMap<QString, GraphicsWellItem *> wellitem_map;

    bool zoom;

    double fov_width;
    double fov_height;
    double fov_x0;
    double fov_y0;

    // Plate dimensions
    QString plate_type;
    int n_col;
    int n_row;
    double plate_size_x;
    double plate_size_y;
    double plate_radius;
    double spacing;
    double well_size;
    double well_radius;
    QStringList rownames;
    QStringList colnames;
    double A1_x0;
    double A1_y0;

    // default styles
    QColor outline_border_color = Qt::lightGray;
    qreal outline_border_width = 0.5;
    QColor colrowlabel_color = Qt::darkGray;
    qreal colrowlabel_height = 3.5; // mm
    QColor fov_rect_color = Qt::darkRed;
    qreal fn_circle_size = 25.0 / 60; // mm
    QColor fn_circle_color = Qt::darkYellow;


    QColor highlight_border_color = QColor("#BBB");
    qreal hightlight_border_width = 1.5;
    QColor highlight_label_color = QColor("#EEE");
};

class GraphicsWellItem : public QGraphicsItem {
public:
    explicit GraphicsWellItem(QString id, double pos_x, double pos_y,
                              double size, double radius,
                              QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

public:
    QString id;
    double x0;
    double y0;
    double size;
    double radius;

    QGraphicsPathItem *outline;
    QGraphicsSimpleTextItem *label;

    // default styles
    QColor outline_border_color = Qt::lightGray;
    qreal outline_border_width = 0.5;
    QColor label_color = QColor("#555");
    qreal label_height = 0.75; // mm
};

#endif // PLATESAMPLEWIDGET_H
