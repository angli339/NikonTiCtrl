#include "datamanager.h"

#include <QDir>
#include <QStandardPaths>
#include <ctime>
#include <fmt/format.h>
#include "logging.h"

DataManager::DataManager(QObject *parent) : QAbstractTableModel(parent)
{
//    qRegisterMetaType<QVector<int> >("QVector<int>");
    this->createExperiment("");
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

    std::time_t t = std::time(nullptr);

    if (name == "") {
        exp->name = fmt::format("{:%Y-%m-%d}", *std::localtime(&t));
    }

    // Find out current user
    std::string user_name = qgetenv("USERNAME").toStdString();
    if (user_name == "") {
        user_name = qgetenv("USER").toStdString();
    }
    if (user_name == "Ang") {
        user_full_name = "Ang Li";
        user_email = "angli01@g.harvard.edu";
    }
    if (user_full_name != "") {
        SPDLOG_INFO("Current User: {} ({} <{}>)", user_name, user_full_name, user_email);
    } else {
        SPDLOG_INFO("Current User: {}", user_name);
    }

    // Path to save images
    std::string home_folder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toStdString();
    if (home_folder == "") {
        SPDLOG_ERROR("Failed to determine home folder path");
        home_folder = "C:/tmp";
    }

    exp->folder = home_folder + "/Data/" + exp->name + "/images";
    SPDLOG_INFO("Images will be saved to '{}'", exp->folder);

    if (!QDir().exists(exp->folder.c_str())) {
        bool status = QDir().mkpath(exp->folder.c_str());
        if (!status) {
            SPDLOG_ERROR("failed to create {}", exp->folder);
        }
    }

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
    im->user_full_name = user_full_name;
    im->user_email = user_email;
   
    im->saveFile(currentExperiment->folder);

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
