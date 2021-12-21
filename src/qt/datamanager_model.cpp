#include "datamanager_model.h"

DataManagerModel::DataManagerModel(DataManager *dataManager, QObject *parent)
    : QAbstractItemModel(parent)
{
    this->dataManager = dataManager;

    buildTree();
}

DataManagerModel::~DataManagerModel()
{
    deleteTree(rootItem);
}

QString DataManagerModel::ExperimentPath()
{
    std::string path = dataManager->ExperimentPath().string();
    return QString::fromStdString(path);
}

ImageData DataManagerModel::GetNextLiveViewFrame()
{
    return dataManager->GetNextLiveViewFrame();
}

NDImage *DataManagerModel::GetNDImage(QString name)
{
    return dataManager->GetNDImage(name.toStdString());
}

void DataManagerModel::buildTree()
{
    rootItem = new DataManagerTreeItem;
    for (const auto &ndimage : dataManager->ListNDImage()) {
        addNDImage(ndimage);
    }
}

void DataManagerModel::addNDImage(NDImage *ndimage)
{
    DataManagerTreeItem *item = new DataManagerTreeItem;
    item->parent = rootItem;
    item->ndimage_name = ndimage->Name();

    beginInsertRows(QModelIndex(), rootItem->child.size(),
                    rootItem->child.size() + 1);
    rootItem->child.push_back(item);
    endInsertRows();
}

void DataManagerModel::deleteTree(DataManagerTreeItem *item)
{
    for (auto &childItem : item->child) {
        deleteTree(childItem);
    }
    delete item;
}

void DataManagerModel::handleExperimentPathChanged(std::string path)
{
    emit experimentPathChanged(path.c_str());
}

void DataManagerModel::handleNDImageCreated(std::string name)
{
    NDImage *im = dataManager->GetNDImage(name);
    addNDImage(im);
    emit ndImageCreated(name.c_str());
}

void DataManagerModel::handleNDImageChanged(std::string name)
{
    emit ndImageChanged(name.c_str());
};


QModelIndex DataManagerModel::index(int row, int column,
                                    const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    DataManagerTreeItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem =
            static_cast<DataManagerTreeItem *>(parent.internalPointer());
    }

    DataManagerTreeItem *childItem = parentItem->child[row];
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    return QModelIndex();
}

QModelIndex DataManagerModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    DataManagerTreeItem *item =
        static_cast<DataManagerTreeItem *>(index.internalPointer());
    DataManagerTreeItem *parentItem = item->parent;

    if (parentItem == rootItem) {
        return QModelIndex();
    }

    for (int i = 0; i < parentItem->child.size(); i++) {
        if (parentItem->child[i] == item) {
            return createIndex(i, 0, parentItem);
        }
    }
    return QModelIndex();
}

int DataManagerModel::rowCount(const QModelIndex &parent) const
{
    DataManagerTreeItem *parentItem;
    if (parent.column() > 0) {
        return 0;
    }

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem =
            static_cast<DataManagerTreeItem *>(parent.internalPointer());
    }
    return parentItem->child.size();
}

int DataManagerModel::columnCount(const QModelIndex &parent) const
{
    return rootItem->columnCount();
}

QVariant DataManagerModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return rootItem->columnName(section);
    }
    return QVariant();
}

QVariant DataManagerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    DataManagerTreeItem *item =
        static_cast<DataManagerTreeItem *>(index.internalPointer());
    return item->columnData(index.column());
}

Qt::ItemFlags DataManagerModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return QAbstractItemModel::flags(index);
}

QString DataManagerModel::NDImageName(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return "";
    }

    DataManagerTreeItem *item =
        static_cast<DataManagerTreeItem *>(index.internalPointer());
    return item->columnData(0).toString();
}