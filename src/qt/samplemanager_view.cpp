#include "qt/samplemanager_view.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include "config.h"
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

    plateLabel = new QLabel;
    plateZoomButton = new QPushButton("Zoom");
    plateZoomButton->setFixedSize(40, 20);
    plateZoomFitButton = new QPushButton("Fit");
    plateZoomFitButton->setFixedSize(40, 20);

    QHBoxLayout *plateWidgetCtrlBar = new QHBoxLayout;
    plateWidgetCtrlBar->setSpacing(10);
    plateWidgetCtrlBar->addWidget(plateLabel);
    plateWidgetCtrlBar->addStretch();
    plateWidgetCtrlBar->addWidget(plateZoomButton);
    plateWidgetCtrlBar->addWidget(plateZoomFitButton);

    plateWidget = new PlateSampleWidget;

    layout->addWidget(sampleManagerTitle);
    layout->addWidget(sampleView);
    layout->addLayout(plateWidgetCtrlBar);
    layout->addWidget(plateWidget);

    connect(plateZoomButton, &QPushButton::pressed, [this] {
        plateWidget->zoomCenter(plateWidget->fov_x0, plateWidget->fov_y0);
    });
    connect(plateZoomFitButton, &QPushButton::pressed, plateWidget,
            &PlateSampleWidget::zoomFit);
}

void SampleManagerView::setModel(SampleManagerModel *model)
{
    this->model = model;
    this->sampleView->setModel(model);

    connect(sampleView->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &SampleManagerView::handleSelectionChanged);

    connect(model, &SampleManagerModel::currentPlateChanged, plateLabel,
            &QLabel::setText);

    connect(model, &SampleManagerModel::currentPlateTypeChanged, plateWidget,
            &PlateSampleWidget::setPlateType);

    connect(model, &SampleManagerModel::currentPlateTypeChanged,
            [this](QString plate_type) {
                // TODO: remove the hard coded FOV size
                double pixel_size = config.system.pixel_size["60xO"];
                plateWidget->setFOVSize(1344 * pixel_size, 1024 * pixel_size);
            });

    connect(model, &SampleManagerModel::FOVPositionChanged, plateWidget,
            &PlateSampleWidget::setFOVPos);
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
