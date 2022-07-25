#include "imagemanager_model.h"

#include "logging.h"

ImageManagerModel::ImageManagerModel(ImageManager *imageManager, QObject *parent)
    : QAbstractItemModel(parent)
{
    this->imageManager = imageManager;
    rootItem = new ImageManagerTreeItem;
    validTreeItems.insert(rootItem);
    buildTree();
}

ImageManagerModel::~ImageManagerModel()
{
    deleteTree(rootItem);
    delete rootItem;
}

ImageData ImageManagerModel::GetNextLiveViewFrame()
{
    return imageManager->GetNextLiveViewFrame();
}

NDImage *ImageManagerModel::GetNDImage(QString name)
{
    return imageManager->GetNDImage(name.toStdString());
}

void ImageManagerModel::handleExperimentOpen()
{
    beginResetModel();
    buildTree();
    endResetModel();
}

void ImageManagerModel::handleExperimentClose()
{
    beginResetModel();
    validTreeItems.clear();
    validTreeItems.insert(rootItem);
    deleteTree(rootItem);
    endResetModel();
}

void ImageManagerModel::buildTree()
{
    for (const auto &ndimage : imageManager->ListNDImage()) {
        addNDImageItem(ndimage);
    }
}

void ImageManagerModel::deleteTree(ImageManagerTreeItem *item)
{
    for (auto &childItem : item->child) {
        deleteTree(childItem);
    }
    if (item != rootItem) {
        delete item;
    } else {
        item->child.clear();
    }
}

void ImageManagerModel::addNDImageItem(NDImage *ndimage)
{
    ImageManagerTreeItem *item = new ImageManagerTreeItem;
    item->parent = rootItem;
    item->row = rootItem->child.size();
    updateNDImageItem(item, ndimage);
    rootItem->child.push_back(item);
    validTreeItems.insert(item);
}

void ImageManagerModel::updateNDImageItem(ImageManagerTreeItem *item, NDImage *ndimage)
{
    item->ndimage = ndimage;
    item->ndimage_name = ndimage->Name();
    if ((ndimage->NDimZ() > 1) && (ndimage->NDimT() > 1)) {
        item->type = "XYZT";
    } else if (ndimage->NDimZ() > 1) {
        item->type = "XYZ";
    } else if (ndimage->NDimT() > 1) {
        item->type = "XYT";
    } else {
        item->type = "XY";
    }
    item->channels = fmt::format("{}", fmt::join(ndimage->ChannelNames(), " ,"));
}

void ImageManagerModel::handleNDImageCreated(std::string name)
{
    NDImage *ndimage = imageManager->GetNDImage(name);

    beginInsertRows(QModelIndex(), rootItem->child.size(),
                    rootItem->child.size() + 1);
    addNDImageItem(ndimage);
    endInsertRows();

    // Emit signal for other downstream users
    emit ndImageCreated(name.c_str());
}

void ImageManagerModel::handleNDImageChanged(std::string name)
{
    NDImage *ndimage = imageManager->GetNDImage(name);
    ImageManagerTreeItem *item = nullptr;
    for (auto & it : rootItem->child) {
        if (it->ndimage == ndimage) {
            item = it;
        }
    }
    if (item == nullptr) {
        return;
    }

    updateNDImageItem(item, ndimage);
    emit dataChanged(index(item->row, 0), index(item->row + 1, columnCount()));

    // Emit signal for other downstream users
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
    if (!validTreeItems.contains(item)) {
        // workaround to deal with index with invalid tree item pointer
        LOG_TRACE("received invalid index");
        return QModelIndex();
    }

    ImageManagerTreeItem *parentItem = item->parent;
    if (parentItem == rootItem) {
        return QModelIndex();
    }
    return createIndex(parentItem->row, 0, parentItem);
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
    return item->ndimage->Name().c_str();
}