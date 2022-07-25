#include "imagemanager_view.h"

#include <QMouseEvent>
#include <QVBoxLayout>

ImageManagerView::ImageManagerView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *imageManagerTitle = new QLabel("Image Manager");
    imageManagerTitle->setFixedHeight(30);

    imageList = new QTreeView;
    imageList->installEventFilter(this);
    imageList->viewport()->installEventFilter(this);

    layout->addWidget(imageManagerTitle);
    layout->addWidget(imageList);
}


void ImageManagerView::setModel(ImageManagerModel *model)
{
    this->model = model;

    this->imageList->setModel(model);
    
    imageList->setColumnWidth(0, 100);
    imageList->setColumnWidth(1, 40);
    imageList->setColumnWidth(2, 60);

    connect(imageList->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImageManagerView::handleSelectionChanged);
}

bool ImageManagerView::eventFilter(QObject *source, QEvent *event)
{
    if ((source == imageList->viewport()) &&
        (event->type() == QEvent::MouseButtonPress))
    {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
        if (!imageList->indexAt(mouse_event->pos()).isValid()) {
            imageList->clearSelection();
            return true;
        }
    }
    if ((source == imageList) && (event->type() == QEvent::KeyPress)) {
        QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
        if (key_event->key() == Qt::Key_Escape) {
            imageList->clearSelection();
            return true;
        }
    }
    return false;
}

void ImageManagerView::handleSelectionChanged(const QItemSelection &selected,
                                             const QItemSelection &deselected)
{
    QModelIndexList selectedRows = imageList->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        emit ndImageSelected("");
        return;
    } else {
        QString name = model->NDImageName(selectedRows.first());
        emit ndImageSelected(name);
        return;
    }
}
