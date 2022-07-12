#ifndef DATAMANAGER_VIEW_H
#define DATAMANAGER_VIEW_H

#include <QLabel>
#include <QTreeView>
#include <QWidget>

#include "qt/datamanager_model.h"

class DataManagerView : public QWidget {
    Q_OBJECT
public:
    explicit DataManagerView(QWidget *parent = nullptr);
    void setModel(ImageManagerModel *model);

    bool eventFilter(QObject *source, QEvent *event);
    void handleSelectionChanged(const QItemSelection &selected,
                                const QItemSelection &deselected);

signals:
    void ndImageSelected(QString name);

private:
    ImageManagerModel *model;

    QTreeView *dataView;
};

#endif
