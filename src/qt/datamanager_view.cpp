#include "datamanager_view.h"

#include <QMouseEvent>
#include <QVBoxLayout>

DataManagerView::DataManagerView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *dataManagerTitle = new QLabel("Data Manager");
    dataManagerTitle->setFixedHeight(30);

    experimentPath = new QLabel;
    experimentPath->setFixedHeight(20);

    dataView = new QTreeView;
    dataView->installEventFilter(this);
    dataView->viewport()->installEventFilter(this);

    layout->addWidget(dataManagerTitle);
    layout->addWidget(experimentPath);
    layout->addWidget(dataView);
}


void DataManagerView::setModel(ImageManagerModel *model)
{
    this->model = model;

    this->experimentPath->setText(model->ExperimentPath());
    connect(model, &ImageManagerModel::experimentPathChanged,
            this->experimentPath, &QLabel::setText);

    this->dataView->setModel(model);
    connect(dataView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &DataManagerView::handleSelectionChanged);
}

bool DataManagerView::eventFilter(QObject *source, QEvent *event)
{
    if ((source == dataView->viewport()) &&
        (event->type() == QEvent::MouseButtonPress))
    {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
        if (!dataView->indexAt(mouse_event->pos()).isValid()) {
            dataView->clearSelection();
            return true;
        }
    }
    if ((source == dataView) && (event->type() == QEvent::KeyPress)) {
        QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
        if (key_event->key() == Qt::Key_Escape) {
            dataView->clearSelection();
            return true;
        }
    }
    return false;
}

void DataManagerView::handleSelectionChanged(const QItemSelection &selected,
                                             const QItemSelection &deselected)
{
    QModelIndexList selectedRows = dataView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        emit ndImageSelected("");
        return;
    } else {
        QString name = model->NDImageName(selectedRows.first());
        emit ndImageSelected(name);
        return;
    }
}
