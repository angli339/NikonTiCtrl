#ifndef SAMPLEMANAGER_MODEL_H
#define SAMPLEMANAGER_MODEL_H

#include <string>
#include <vector>

#include <QAbstractItemModel>

#include "sample/samplemanager.h"

class SampleManagerTreeItem;

class SampleManagerModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit SampleManagerModel(SampleManager *sampleManager,
                                QObject *parent = nullptr);
    ~SampleManagerModel();

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    SampleManager *sampleManager;

    SampleManagerTreeItem *rootItem;
    void buildTree();
    void deleteTree(SampleManagerTreeItem *item);
    void addSampleArray(SampleManagerTreeItem *parent, SampleArray *array);
    void addSample(SampleManagerTreeItem *parent, Sample *sample);
    void addSite(SampleManagerTreeItem *parent, Site *site);
};

enum class SampleManagerTreeItemType
{
    SampleArray,
    Sample,
    Site,
    NDImage,
};

class SampleManagerTreeItem {
public:
    SampleManagerTreeItemType type;
    std::string id;
    std::string name;

    SampleManagerTreeItem *parent;
    std::vector<SampleManagerTreeItem *> child;

    // backend type
    SampleArray *array;
    Sample *sample;
    Site *site;

    int columnCount();
    QVariant columnName(int col);
    QVariant columnData(int col);
};

#endif
