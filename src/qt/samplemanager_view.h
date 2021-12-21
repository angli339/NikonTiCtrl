#ifndef SAMPLEMANAGER_VIEW_H
#define SAMPLEMANAGER_VIEW_H

#include <QLabel>
#include <QTreeView>
#include <QWidget>

#include "qt/samplemanager_model.h"

class SampleManagerView : public QWidget {
    Q_OBJECT
public:
    explicit SampleManagerView(QWidget *parent = nullptr);
    void setModel(SampleManagerModel *model);

    bool eventFilter(QObject *source, QEvent *event);
    void handleSelectionChanged(const QItemSelection &selected,
                                const QItemSelection &deselected);

public:
    SampleManagerModel *model;

    QTreeView *sampleView;
};

#endif
