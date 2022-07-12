#ifndef IMAGEMANAGER_VIEW_H
#define IMAGEMANAGER_VIEW_H

#include <QLabel>
#include <QTreeView>
#include <QWidget>

#include "qt/imagemanager_model.h"

class ImageManagerView : public QWidget {
    Q_OBJECT
public:
    explicit ImageManagerView(QWidget *parent = nullptr);
    void setModel(ImageManagerModel *model);

    bool eventFilter(QObject *source, QEvent *event);
    void handleSelectionChanged(const QItemSelection &selected,
                                const QItemSelection &deselected);

signals:
    void ndImageSelected(QString name);

private:
    ImageManagerModel *model;

    QTreeView *imageList;
};

#endif
