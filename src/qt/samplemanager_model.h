#ifndef SAMPLEMANAGER_MODEL_H
#define SAMPLEMANAGER_MODEL_H

#include <set>
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

    void handleExperimentOpen();
    void handleExperimentClose();
    void handlePlateCreated(QString plate_id);
    void handlePlateModified(QString plate_id);
    void handleCurrentPlateChanged(QString plate_id);
    void handleStagePositionUpdate(double x, double y);

signals:
    void currentPlateChanged(QString plate_id);
    void currentPlateTypeChanged(QString plate_type);
    void FOVPositionChanged(double x, double y);

private:
    SampleManager *sampleManager;

    SampleManagerTreeItem *rootItem = nullptr;
    std::set<SampleManagerTreeItem *> validTreeItems;
    void buildTree();
    void deleteTree(SampleManagerTreeItem *item);
    void addPlateItem(Plate *array);
    void addWellItem(SampleManagerTreeItem *plateItem, Well *sample);
    void addSiteItem(SampleManagerTreeItem *wellItem, Site *site);
};

enum class SampleManagerTreeItemType {
    Plate,
    Well,
    Site,
    NDImage,
};

class SampleManagerTreeItem {
public:
    SampleManagerTreeItemType type;
    std::string id;
    std::string summary;

    SampleManagerTreeItem *parent = nullptr;
    int row;
    std::vector<SampleManagerTreeItem *> child;

    // backend type
    Plate *plate = nullptr;
    Well *well = nullptr;
    Site *site = nullptr;

    int columnCount();
    QVariant columnName(int col);
    QVariant columnData(int col);
};

#endif
