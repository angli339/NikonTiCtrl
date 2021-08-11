#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ghc/filesystem.hpp>
#include <QAbstractTableModel>
#include <QObject>
#include <string>
#include <unordered_map>
#include <vector>

#include "image.h"

namespace fs = ghc::filesystem;

struct Experiment {
    fs::path image_dir;
    std::string id;
    std::string name;
    std::vector<Image *> images;
};

class DataManager : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DataManager(fs::path data_root, QObject *parent = nullptr);
    ~DataManager();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    std::string createExperiment(std::string name);
    void setLiveImage(Image *im);
    void setLiveViewStatus(bool isLiveViewRunning) {this->isLiveViewRunning = isLiveViewRunning; }
    Image *getNextLiveImage();

    std::string addImage(Image *im);
    Image *getImage(std::string id);

signals:
    void requestDisplayImage(Image *im);

private:
    fs::path data_root;
    Experiment *currentExperiment = nullptr;
    int tempID = 0;
    std::string user_full_name;
    std::string user_email;

    std::atomic<bool> isLiveViewRunning;
    std::mutex mu_liveImage;
    std::condition_variable cv_liveImage;
    Image *liveImage = nullptr;
};

#endif // DATAMANAGER_H
