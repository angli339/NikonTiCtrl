#ifndef IMAGEMANAGER_MODEL_H
#define IMAGEMANAGER_MODEL_H

#include <string>
#include <vector>
#include <set>

#include <QAbstractItemModel>

#include "image/imagemanager.h"

class ExperimentControlModel;
class ImageManagerTreeItem;

class ImageManagerModel : public QAbstractItemModel {
    Q_OBJECT
    friend class ExperimentControlModel;

public:
    explicit ImageManagerModel(ImageManager *imageManager,
                              QObject *parent = nullptr);
    ~ImageManagerModel();

    void handleExperimentOpen();
    void handleExperimentClose();

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
    void ndImageCreated(QString name);
    void ndImageChanged(QString name);

private:
    ImageManager *imageManager;

    ImageManagerTreeItem *rootItem = nullptr;
    std::set<ImageManagerTreeItem *> validTreeItems;
    void buildTree();
    void deleteTree(ImageManagerTreeItem *item);
    void addNDImageItem(NDImage *ndimage);
    void updateNDImageItem(ImageManagerTreeItem *item, NDImage *ndimage);

    void handleExperimentPathChanged(std::string path);
    void handleNDImageCreated(std::string name);
    void handleNDImageChanged(std::string name);
};

class ImageManagerTreeItem {
public:
    std::string ndimage_name;
    std::string type;
    std::string channels;

    ImageManagerTreeItem *parent = nullptr;
    int row = 0;
    std::vector<ImageManagerTreeItem *> child;

    // backend type
    NDImage *ndimage = nullptr;

    int columnCount()
    {
        return 3;
    }
    QVariant columnName(int col)
    {
        switch (col) {
        case 0:
            return QVariant("Name");
        case 1:
            return QVariant("Type");
        case 2:
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
            return QVariant(type.c_str());
        case 2:
            return QVariant(channels.c_str());
        default:
            return QVariant();
        }
    }
};

#endif
