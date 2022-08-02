#include "qt/samplemanager_model.h"

#include "logging.h"

SampleManagerModel::SampleManagerModel(SampleManager *sampleManager,
                                       QObject *parent)
    : QAbstractItemModel(parent)
{
    this->sampleManager = sampleManager;
    rootItem = new SampleManagerTreeItem;
    validTreeItems.insert(rootItem);
    buildTree();
}

SampleManagerModel::~SampleManagerModel()
{
    deleteTree(rootItem);
    delete rootItem;
}

void SampleManagerModel::handleExperimentClose()
{
    beginResetModel();
    validTreeItems.clear();
    validTreeItems.insert(rootItem);
    deleteTree(rootItem);
    endResetModel();
}

void SampleManagerModel::handleExperimentOpen()
{
    beginResetModel();
    buildTree();
    endResetModel();
}

void SampleManagerModel::handlePlateCreated(QString plate_id)
{
    Plate *plate = sampleManager->Plate(plate_id.toStdString());
    beginInsertRows(QModelIndex(), rootItem->child.size(),
                    rootItem->child.size() + 1);
    addPlateItem(plate);
    endInsertRows();
}

void SampleManagerModel::handlePlateModified(QString plate_id)
{
    // rebuild the tree fow now
    handleExperimentClose();
    handleExperimentOpen();
}

void SampleManagerModel::handleCurrentPlateChanged(QString plate_id)
{
    Plate *plate = sampleManager->Plate(plate_id.toStdString());
    if (plate == nullptr) {
        emit currentPlateChanged("");
        emit currentPlateTypeChanged("");
        return;
    }

    emit currentPlateChanged(plate_id);
    if (plate->Type() == PlateType::Wellplate96) {
        emit currentPlateTypeChanged("wellplate96");
    } else if (plate->Type() == PlateType::Wellplate384) {
        emit currentPlateTypeChanged("wellplate384");
    } else {
        emit currentPlateTypeChanged("");
    }
}

void SampleManagerModel::handleStagePositionUpdate(double x, double y)
{
    Plate *plate = sampleManager->CurrentPlate();
    if (plate == nullptr) {
        return;
    };

    if (!plate->PositionOrigin().has_value()) {
        return;
    }
    auto pos_origin = plate->PositionOrigin().value();

    // TODO: get direction from config file
    double rel_x = -(x - pos_origin.x)/1000;
    double rel_y = -(y - pos_origin.y)/1000;

    emit FOVPositionChanged(rel_x, rel_y);
}

void SampleManagerModel::buildTree()
{
    for (Plate *plate : sampleManager->Plates()) {
        addPlateItem(plate);
    }
}

void SampleManagerModel::deleteTree(SampleManagerTreeItem *item)
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

void SampleManagerModel::addPlateItem(Plate *plate)
{
    SampleManagerTreeItem *item = new SampleManagerTreeItem;
    item->parent = rootItem;
    item->row = rootItem->child.size();
    item->type = SampleManagerTreeItemType::Plate;
    item->plate = plate;
    rootItem->child.push_back(item);
    validTreeItems.insert(item);

    item->id = plate->ID();
    item->summary = fmt::format("{}, {} wells", PlateTypeToString(plate->Type()), plate->NumEnabledWells());
    if (plate->PositionOrigin().has_value()) {
        auto pos_origin = plate->PositionOrigin().value();
        item->summary = fmt::format("{}, xy=({},{})", item->summary, pos_origin.x, pos_origin.y);
    }
    auto metadata = plate->Metadata();
    if (metadata.contains("name")) {
        item->summary = fmt::format("{}, name='{}'", item->summary, metadata["name"]);
    }
    
    for (const auto &well : plate->Wells()) {
        if (well->Enabled()) {
            addWellItem(item, well);
        }
    }
}

void SampleManagerModel::addWellItem(SampleManagerTreeItem *plateItem,
                                   Well *well)
{
    SampleManagerTreeItem *item = new SampleManagerTreeItem;
    item->parent = plateItem;
    item->row = plateItem->child.size();
    item->type = SampleManagerTreeItemType::Well;
    item->well = well;
    plateItem->child.push_back(item);
    validTreeItems.insert(item);

    item->id = well->ID();
    item->summary = fmt::format("{} sites", well->NumEnabledSites());
    auto metadata = well->Metadata();
    if (metadata.contains("preset_name")) {
        item->summary = fmt::format("{}, {}", item->summary, metadata["preset_name"]);
    }

    for (const auto &site : well->Sites()) {
        addSiteItem(item, site);
    }
}

void SampleManagerModel::addSiteItem(SampleManagerTreeItem *wellItem, Site *site)
{
    SampleManagerTreeItem *item = new SampleManagerTreeItem;
    item->parent = wellItem;
    item->row = wellItem->child.size();
    item->type = SampleManagerTreeItemType::Site;
    item->site = site;
    wellItem->child.push_back(item);
    validTreeItems.insert(item);

    item->id = site->ID();
    auto metadata = site->Metadata();
    if (metadata.contains("name")) {
        item->summary = fmt::format("{}", metadata["name"]);
    }
}

QModelIndex SampleManagerModel::index(int row, int column,
                                      const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    SampleManagerTreeItem *parentItem;
    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem =
            static_cast<SampleManagerTreeItem *>(parent.internalPointer());
    }

    SampleManagerTreeItem *childItem = parentItem->child[row];
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    return QModelIndex();
}

QModelIndex SampleManagerModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    SampleManagerTreeItem *item =
        static_cast<SampleManagerTreeItem *>(index.internalPointer());
    if (!validTreeItems.contains(item)) {
        // workaround to deal with index with invalid tree item pointer
        LOG_TRACE("received invalid index");
        return QModelIndex();
    }

    SampleManagerTreeItem *parentItem = item->parent;
    if (parentItem == rootItem) {
        return QModelIndex();
    }
    return createIndex(parentItem->row, 0, parentItem);
}

int SampleManagerModel::rowCount(const QModelIndex &parent) const
{
    SampleManagerTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem =
            static_cast<SampleManagerTreeItem *>(parent.internalPointer());
    }

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
        return QVariant("");
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
        return QVariant(summary.c_str());
    default:
        return QVariant();
    }
}