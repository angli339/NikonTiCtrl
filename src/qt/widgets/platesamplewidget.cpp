#include "platesamplewidget.h"

#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QPainterPath>

PlateSampleWidget::PlateSampleWidget(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    graphView = new QGraphicsView;
    graphView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(graphView);

    scene = new QGraphicsScene;
    graphView->setScene(scene);
}

QSize PlateSampleWidget::sizeHint() const
{
    return QSize(300/(85.48*2)*(127.76*2), 300);
}

void PlateSampleWidget::clear()
{
    scene->clear();
    rowlabels.clear();
    collabels.clear();
    wellitem_map.clear();
    rownames.clear();
    colnames.clear();
    fov_rect = nullptr;
    fn_circle = nullptr;
}

void PlateSampleWidget::setPlateType(QString plate_type)
{
    clear();
    
    this->plate_type = plate_type;
    if (plate_type == "wellplate96") {
        n_col = 12;
        n_row = 8;
        plate_size_x = 127.76;
        plate_size_y = 85.48;
        plate_radius = 3; // to find out
        spacing = 9.0;
        well_size = 7.52;
        well_radius = 0.2; // to find out

        colrowlabel_height = 5;
    } else if (plate_type == "wellplate384") {
        n_col = 24;
        n_row = 16;
        plate_size_x = 127.76;
        plate_size_y = 85.48;
        plate_radius = 3; // to find out
        spacing = 4.5;
        well_size = 2.9;
        well_radius = 0.2; // to find out

        colrowlabel_height = 3.5;
    } else {
        return;
    }

    for (int i_row = 0; i_row < n_row; i_row++) {
        char rowname = 'A' + i_row;
        rownames.push_back(QString(rowname));
    }
    for (int i_col = 0; i_col < n_col; i_col++) {
        QString colname = QString("%1").arg(i_col+1, 2, 10, QLatin1Char('0'));
        colnames.push_back(colname);
    }

    A1_x0 = (plate_size_x - spacing * (n_col - 1)) / 2;
    A1_y0 = (plate_size_y - spacing * (n_row - 1)) / 2;

    initScene();
    zoomFit();
}

void PlateSampleWidget::zoomCenter(QString well_id)
{
    if (!wellitem_map.contains(well_id)) {
        return;
    }
    
    GraphicsWellItem *wellitem = wellitem_map[well_id];
    zoomCenter(wellitem->x0 - A1_x0, wellitem->y0 - A1_y0);
}

void PlateSampleWidget::zoomCenter(double rel_x0, double rel_y0)
{
    double width = spacing * 2;
    double height = spacing * 2;
    double x1 = A1_x0 + rel_x0 - width / 2;
    double y1 = A1_y0 + rel_y0 - height / 2;
    scene->setSceneRect(x1, y1, width, height);
    graphView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

    for (auto & wellitem: wellitem_map) {
        wellitem->label->show();
    }
    zoom = true;
}

void PlateSampleWidget::zoomFit()
{
    scene->setSceneRect(-1, -1, plate_size_x+2, plate_size_y+2);
    graphView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

    for (auto & wellitem : wellitem_map) {
        wellitem->label->hide();
    }
    zoom = false;
}

void PlateSampleWidget::setFOVSize(double fov_width, double fov_height)
{
    if (fov_rect) {
        fov_rect->setRect(-fov_width/2, -fov_height/2, fov_width, fov_height);
    }
    
}

void PlateSampleWidget::setFOVPos(double rel_x0, double rel_y0)
{
    if (fov_rect) {
        fov_x0 = rel_x0;
        fov_y0 = rel_y0;
        fov_rect->setPos(A1_x0 + rel_x0, A1_y0 + rel_y0);
        fov_rect->show();
    }
    
    if (fn_circle) {
        fn_circle->setPos(A1_x0 + rel_x0, A1_y0 + rel_y0);
        fn_circle->show();
    }

    if (zoom) {
        zoomCenter(rel_x0, rel_y0);
    }
}

void PlateSampleWidget::setWellHighlighted(QString well_id, bool highlighted)
{
    if (!wellitem_map.contains(well_id)) {
        return;
    }
    GraphicsWellItem *wellitem = wellitem_map[well_id];

    // border
    QPen pen = wellitem->outline->pen();
    pen.setColor(highlight_border_color);
    pen.setWidthF(hightlight_border_width);
    wellitem->outline->setPen(pen);

    // well label
    if (highlighted) {
        wellitem->label->setBrush(highlight_label_color);
    } else {
        wellitem->label->setBrush(wellitem->label_color);
    }

    // row and col labels
    if (well_id.size() != 3) {
        return;
    }
    QString rowname = well_id[0];
    QString colname = well_id.last(2);

    int i_row = rownames.indexOf(rowname);
    int i_col = colnames.indexOf(colname);
    QGraphicsSimpleTextItem *rowlabel_item = rowlabels[i_row];
    QGraphicsSimpleTextItem *collabel_item = collabels[i_col];

    if (highlighted) {
        rowlabel_item->setBrush(highlight_label_color);
        collabel_item->setBrush(highlight_label_color);
    } else {
        rowlabel_item->setBrush(colrowlabel_color);
        collabel_item->setBrush(colrowlabel_color);
    }

}

void PlateSampleWidget::initScene()
{
    // Outer bounder
    QPen pen(outline_border_color, outline_border_width);
    pen.setCosmetic(true);

    QPainterPath path_boundary;
    path_boundary.addRoundedRect(0, 0, plate_size_x, plate_size_y, plate_radius, plate_radius);
    scene->addPath(path_boundary, pen);

    // Row labels
    for (int i_row = 0; i_row < n_row; i_row++) {
        QGraphicsSimpleTextItem * labelitem = addColRowLabel(
            rownames[i_row],
            A1_x0 - well_size / 2 - colrowlabel_height,
            A1_y0 + i_row * spacing);
        rowlabels.push_back(labelitem);
    }

    // Column labels
    for (int i_col = 0; i_col < n_col; i_col++) {
        QGraphicsSimpleTextItem * labelitem = addColRowLabel(
            QString("%1").arg(i_col+1),
            A1_x0 + i_col * spacing,
            A1_y0 - well_size / 2 - colrowlabel_height / 2 - 1);
        collabels.push_back(labelitem);
    }

    // Wells
    for (int i_row = 0; i_row < n_row; i_row++) {
        for (int i_col = 0; i_col < n_col; i_col++) {
            double well_pos_x0 = A1_x0 + i_col * spacing;
            double well_pos_y0 = A1_y0 + i_row * spacing;
            QString well_id = QString("%1%2").arg(rownames[i_row]).arg(colnames[i_col]);
            GraphicsWellItem *wellItem = new GraphicsWellItem(well_id, well_pos_x0, well_pos_y0, well_size, well_radius);
            wellitem_map[well_id] = wellItem;
            scene->addItem(wellItem);
        }
    }

    // FOV
    QPen fov_pen(fov_rect_color, 1.5);
    fov_pen.setCosmetic(true);

    fov_rect = new QGraphicsRectItem;
    fov_rect->setPen(fov_pen);
    fov_rect->setBrush(Qt::NoBrush);
    fov_rect->hide();
    scene->addItem(fov_rect);

    // FN circle
    fn_circle = new QGraphicsEllipseItem(-fn_circle_size/2, -fn_circle_size/2, fn_circle_size, fn_circle_size);
    QPen fn_pen(fn_circle_color, 1.5);
    fn_pen.setCosmetic(true);
    fn_circle->setPen(fn_pen);
    fn_circle->setBrush(Qt::NoBrush);
    fn_circle->hide();
    scene->addItem(fn_circle);
}

QGraphicsSimpleTextItem  *PlateSampleWidget::addColRowLabel(QString text, double x0, double y0)
{
    QGraphicsSimpleTextItem *textitem = new QGraphicsSimpleTextItem(text);
    textitem->setBrush(colrowlabel_color);

    QFont font;
    font.setBold(true);
    textitem->setFont(font);
    
    QRectF bb = textitem->sceneBoundingRect();
    textitem->setScale(colrowlabel_height / bb.height());

    bb = textitem->sceneBoundingRect();
    textitem->setPos(x0 - bb.width() / 2, y0 - bb.height() / 2);

    scene->addItem(textitem);
    return textitem;
}

void PlateSampleWidget::resizeEvent(QResizeEvent *event)
{
    graphView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

GraphicsWellItem::GraphicsWellItem(QString id, double pos_x, double pos_y, double size, double radius, QGraphicsItem *parent) : QGraphicsItem(parent)
{
    this->id = id;
    this->x0 = pos_x;
    this->y0 = pos_y;
    this->size = size;
    this->radius = radius;

    // Top left corner
    double x1 = pos_x - size / 2;
    double y1 = pos_y - size / 2;

    // Outline
    outline = new QGraphicsPathItem(this);
    outline->setBrush(Qt::NoBrush);

    QPainterPath path = outline->path();
    path.addRoundedRect(x1, y1, size, size, radius, radius);
    outline->setPath(path);

    QPen pen(outline_border_color, outline_border_width);
    pen.setCosmetic(true);
    outline->setPen(pen);

    // Well ID Label
    label = new QGraphicsSimpleTextItem(id, this);
    label->setBrush(label_color);

    QFont font;
    font.setBold(true);
    label->setFont(font);

    QRectF bb = label->sceneBoundingRect();
    label->setScale(label_height / bb.height());
    bb = label->sceneBoundingRect();
    label->setPos(x1, y1 - bb.height());
}

QRectF GraphicsWellItem::boundingRect() const
{
    return outline->boundingRect();
}

void GraphicsWellItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

}
