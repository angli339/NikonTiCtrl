#include "imagemanager_model.h"

ImageManagerModel::ImageManagerModel(ImageManager *imageManager, QObject *parent)
    : QAbstractItemModel(parent)
{
    this->imageManager = imageManager;

    buildTree();
}

ImageManagerModel::~ImageManagerModel()
{
    deleteTree(rootItem);
}

ImageData ImageManagerModel::GetNextLiveViewFrame()
{
    return imageManager->GetNextLiveViewFrame();
}

NDImage *ImageManagerModel::GetNDImage(QString name)
{
    return imageManager->GetNDImage(name.toStdString());
}

void ImageManagerModel::buildTree()
{
    rootItem = new ImageManagerTreeItem;
    for (const auto &ndimage : imageManager->ListNDImage()) {
        addNDImage(ndimage);
    }
}

void ImageManagerModel::addNDImage(NDImage *ndimage)
{
    ImageManagerTreeItem *item = new ImageManagerTreeItem;
    item->parent = rootItem;
    item->ndimage_name = ndimage->Name();

    beginInsertRows(QModelIndex(), rootItem->child.size(),
                    rootItem->child.size() + 1);
    rootItem->child.push_back(item);
    endInsertRows();
}

void ImageManagerModel::deleteTree(ImageManagerTreeItem *item)
{
    for (auto &childItem : item->child) {
        deleteTree(childItem);
    }
    delete item;
}

void ImageManagerModel::handleNDImageCreated(std::string name)
{
    NDImage *im = imageManager->GetNDImage(name);
    addNDImage(im);
    emit ndImageCreated(name.c_str());
}

void ImageManagerModel::handleNDImageChanged(std::string name)
{
    emit ndImageChanged(name.c_str());
};


QModelIndex ImageManagerModel::index(int row, int column,
                                    const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    ImageManagerTreeItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem =
            static_cast<ImageManagerTreeItem *>(parent.internalPointer());
    }

    ImageManagerTreeItem *childItem = parentItem->child[row];
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    return QModelIndex();
}

QModelIndex ImageManagerModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    ImageManagerTreeItem *item =
        static_cast<ImageManagerTreeItem *>(index.internalPointer());
    ImageManagerTreeItem *parentItem = item->parent;

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

int ImageManagerModel::rowCount(const QModelIndex &parent) const
{
    ImageManagerTreeItem *parentItem;
    if (parent.column() > 0) {
        return 0;
    }

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem =
            static_cast<ImageManagerTreeItem *>(parent.internalPointer());
    }
    return parentItem->child.size();
}

int ImageManagerModel::columnCount(const QModelIndex &parent) const
{
    return rootItem->columnCount();
}

QVariant ImageManagerModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return rootItem->columnName(section);
    }
    return QVariant();
}

QVariant ImageManagerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    ImageManagerTreeItem *item =
        static_cast<ImageManagerTreeItem *>(index.internalPointer());
    return item->columnData(index.column());
}

Qt::ItemFlags ImageManagerModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return QAbstractItemModel::flags(index);
}

QString ImageManagerModel::NDImageName(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return "";
    }

    ImageManagerTreeItem *item =
        static_cast<ImageManagerTreeItem *>(index.internalPointer());
    return item->columnData(0).toString();
}