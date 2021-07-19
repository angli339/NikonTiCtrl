#include "datamanager.h"

#include <ctime>
#include <fmt/format.h>

#include "config.h"
#include "logger.h"

DataManager::DataManager(fs::path data_root, QObject *parent) : QAbstractTableModel(parent)
{
    this->data_root = data_root;
    createExperiment("");
}

DataManager::~DataManager()
{
    if (currentExperiment) {
        delete currentExperiment;
    }
    if (liveImage) {
        delete liveImage;
    }
}

std::string DataManager::createExperiment(std::string name)
{
    Experiment *exp = new Experiment;

    exp->id = fmt::format("exp-{}", tempID);
    tempID++;

    if (name == "") {
        std::time_t t = std::time(nullptr);
        exp->name = fmt::format("{:%Y-%m-%d}", fmt::localtime(t));
    }

    // Path to save images
    fs::path image_dir = data_root / exp->name / "images";
    if (!fs::exists(image_dir)) {
        if (!fs::create_directories(image_dir)) {
            LOG_ERROR("failed to create {}", image_dir.string());
        }
    }
    exp->image_dir = image_dir;
    LOG_INFO("Images will be saved to '{}'", image_dir.string());

    if (currentExperiment) {
        delete currentExperiment;
    }
    currentExperiment = exp;

    return exp->id;
}

void DataManager::setLiveImage(Image *im)
{
    std::unique_lock<std::mutex> lk(mu_liveImage);
    if (liveImage != nullptr) {
        delete liveImage;
    }
    liveImage = im;

    lk.unlock();
    emit requestDisplayImage(liveImage);
    cv_liveImage.notify_all();
}

Image *DataManager::getNextLiveImage() {
    std::unique_lock<std::mutex> lk(mu_liveImage);
    cv_liveImage.wait(lk);
    if (isLiveViewRunning) {
        return liveImage;
    }
    return nullptr;
}

std::string DataManager::addImage(Image *im)
{
    im->id = std::to_string(tempID);
    tempID++;
    im->user_full_name = configUser.name;
    im->user_email = configUser.email;
   
    im->saveFile(currentExperiment->image_dir);

    // temporary: only keep 10 most recent images in memory.
    if (currentExperiment->images.size() > 10) {
        for (int i = 0; i < currentExperiment->images.size() - 10; i++) {
            Image *im = currentExperiment->images[i];
            if (im->buf != NULL) {
                free(im->buf);
                im->buf = NULL;
            }
        }
    }

    int n_row = currentExperiment->images.size();
    beginInsertRows(QModelIndex(), n_row, n_row+1);

    currentExperiment->images.push_back(im);

    endInsertRows();

    emit requestDisplayImage(im);
    return im->id;
}

Image *DataManager::getImage(std::string name)
{
    for (const auto& im : currentExperiment->images) {
        if (im->name == name) {
            return im;
        }
    }
    throw std::runtime_error(fmt::format("getImage: {} not found", name));
}

int DataManager::rowCount(const QModelIndex & /*parent*/) const
{
    return currentExperiment->images.size();
}

int DataManager::columnCount(const QModelIndex & /*parent*/) const
{
    return 2;
}

QVariant DataManager::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        auto im = currentExperiment->images[index.row()];
        if (index.column() == 0) {
            return QString("%1").arg(im->id.c_str());
        }
        if (index.column() == 1) {
            return QString("%1").arg(im->name.c_str());
        }
    }
    return QVariant();
}
