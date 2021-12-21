#include "qt/samplemanager_view.h"

#include <QMouseEvent>
#include <QVBoxLayout>

#include "logging.h"

SampleManagerView::SampleManagerView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *sampleManagerTitle = new QLabel("Sample Manager");
    sampleManagerTitle->setFixedHeight(30);

    sampleView = new QTreeView;
    sampleView->viewport()->installEventFilter(this);

    layout->addWidget(sampleManagerTitle);
    layout->addWidget(sampleView);
}

void SampleManagerView::setModel(SampleManagerModel *model)
{
    this->model = model;
    this->sampleView->setModel(model);

    connect(sampleView->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &SampleManagerView::handleSelectionChanged);
}

bool SampleManagerView::eventFilter(QObject *source, QEvent *event)
{
    if ((source == sampleView->viewport()) &&
        (event->type() == QEvent::MouseButtonPress))
    {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
        if (!sampleView->indexAt(mouse_event->pos()).isValid()) {
            sampleView->clearSelection();
            return true;
        }
    }
    return false;
}

void SampleManagerView::handleSelectionChanged(const QItemSelection &selected,
                                               const QItemSelection &deselected)
{
    // QModelIndexList selectedRows =
    // sampleView->selectionModel()->selectedRows();
}
