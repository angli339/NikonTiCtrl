#include "qt/samplemanager_model.h"

#include "logging.h"

SampleManagerModel::SampleManagerModel(SampleManager *sampleManager,
                                       QObject *parent)
    : QAbstractItemModel(parent)
{
    this->sampleManager = sampleManager;

    buildTree();
}

SampleManagerModel::~SampleManagerModel()
{
    if (rootItem) {
        deleteTree(rootItem);
    }
}

void SampleManagerModel::handleExperimentOpen()
{
    beginResetModel();
    if (rootItem) {
        deleteTree(rootItem);
    }
    buildTree();
    endResetModel();
}

void SampleManagerModel::buildTree()
{
    rootItem = new SampleManagerTreeItem;
    for (Plate *plate : sampleManager->Plates()) {
        addPlate(rootItem, plate);
    }
}

void SampleManagerModel::deleteTree(SampleManagerTreeItem *item)
{
    for (auto &childItem : item->child) {
        deleteTree(childItem);
    }
    delete item;
}

void SampleManagerModel::addPlate(SampleManagerTreeItem *parent,
                                        Plate *plate)
{

    SampleManagerTreeItem *item = new SampleManagerTreeItem;
    parent->child.push_back(item);

    item->type = SampleManagerTreeItemType::Plate;
    item->id = plate->ID();
    item->name = plate->Name();
    item->parent = parent;
    item->plate = plate;

    for (const auto &well : plate->Wells()) {
        addWell(item, well);
    }
}

void SampleManagerModel::addWell(SampleManagerTreeItem *parent,
                                   Well *well)
{
    SampleManagerTreeItem *item = new SampleManagerTreeItem;
    parent->child.push_back(item);

    item->type = SampleManagerTreeItemType::Well;
    item->id = well->ID();
    item->parent = parent;
    item->well = well;

    for (const auto &site : well->Sites()) {
        addSite(item, site);
    }
}

void SampleManagerModel::addSite(SampleManagerTreeItem *parent, Site *site)
{
    SampleManagerTreeItem *item = new SampleManagerTreeItem;
    parent->child.push_back(item);

    item->type = SampleManagerTreeItemType::Site;
    item->id = site->ID();
    item->name = site->Name();
    item->parent = parent;
    item->site = site;
}

QModelIndex SampleManagerModel::index(int row, int column,
                                      const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    SampleManagerTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem =
            static_cast<SampleManagerTreeItem *>(parent.internalPointer());

    SampleManagerTreeItem *childItem = parentItem->child[row];
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex SampleManagerModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SampleManagerTreeItem *item =
        static_cast<SampleManagerTreeItem *>(index.internalPointer());
    SampleManagerTreeItem *parentItem = item->parent;

    if (parentItem == rootItem)
        return QModelIndex();

    for (int i = 0; i < parentItem->child.size(); i++) {
        if (parentItem->child[i] == item) {
            return createIndex(i, 0, parentItem);
        }
    }
    return QModelIndex();
}

int SampleManagerModel::rowCount(const QModelIndex &parent) const
{
    SampleManagerTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem =
            static_cast<SampleManagerTreeItem *>(parent.internalPointer());

    return parentItem->child.size();
}

int SampleManagerModel::columnCount(const QModelIndex &parent) const
{
    return rootItem->columnCount();
}

QVariant SampleManagerModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return rootItem->columnName(section);
    }
    return QVariant();
}

QVariant SampleManagerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    SampleManagerTreeItem *item =
        static_cast<SampleManagerTreeItem *>(index.internalPointer());
    return item->columnData(index.column());
}

Qt::ItemFlags SampleManagerModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

int SampleManagerTreeItem::columnCount()
{
    return 2;
}
QVariant SampleManagerTreeItem::columnName(int col)
{
    switch (col) {
    case 0:
        return QVariant("ID");
    case 1:
        return QVariant("Name");
    default:
        return QVariant();
    }
}
QVariant SampleManagerTreeItem::columnData(int col)
{
    switch (col) {
    case 0:
        return QVariant(id.c_str());
    case 1:
        return QVariant(name.c_str());
    default:
        return QVariant();
    }
}