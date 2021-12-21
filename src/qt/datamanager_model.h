#ifndef DATAMANAGER_MODEL_H
#define DATAMANAGER_MODEL_H

#include <string>
#include <vector>

#include <QAbstractItemModel>

#include "data/datamanager.h"

class ImagingControlModel;
class DataManagerTreeItem;

class DataManagerModel : public QAbstractItemModel {
    Q_OBJECT
    friend class ImagingControlModel;

public:
    explicit DataManagerModel(DataManager *dataManager,
                              QObject *parent = nullptr);
    ~DataManagerModel();

    QString ExperimentPath();

    ImageData GetNextLiveViewFrame();
    NDImage *GetNDImage(QString name);

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QString NDImageName(const QModelIndex &index) const;

signals:
    void experimentPathChanged(QString path);
    void ndImageCreated(QString name);
    void ndImageChanged(QString name);

private:
    DataManager *dataManager;

    DataManagerTreeItem *rootItem;
    void buildTree();
    void deleteTree(DataManagerTreeItem *item);
    void addNDImage(NDImage *ndimage);

    void handleExperimentPathChanged(std::string path);
    void handleNDImageCreated(std::string name);
    void handleNDImageChanged(std::string name);
};

class DataManagerTreeItem {
public:
    std::string ndimage_name;

    DataManagerTreeItem *parent;
    std::vector<DataManagerTreeItem *> child;

    // backend type
    NDImage *ndimage;

    int columnCount()
    {
        return 2;
    }
    QVariant columnName(int col)
    {
        switch (col) {
        case 0:
            return QVariant("Name");
        case 1:
            return QVariant("Channels");
        default:
            return QVariant();
        }
    }
    QVariant columnData(int col)
    {
        switch (col) {
        case 0:
            return QVariant(ndimage_name.c_str());
        case 1:
            return QVariant();
        default:
            return QVariant();
        }
    }
};

#endif
