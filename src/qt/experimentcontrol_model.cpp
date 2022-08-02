#include "qt/experimentcontrol_model.h"

#include "logging.h"

ExperimentControlModel::ExperimentControlModel(ExperimentControl *exp,
                                         QObject *parent)
    : QObject(parent)
{
    this->exp = exp;

    channelControlModel =
        new ::ChannelControlModel(exp->Channels());
    imageManagerModel = new ::ImageManagerModel(exp->Images());
    sampleManagerModel =
        new ::SampleManagerModel(exp->Samples());

    handleEventFuture = std::async(std::launch::async,
                                   &ExperimentControlModel::handleEvents, this);
    exp->SubscribeEvents(&eventStream);
    exp->Devices()->SubscribeEvents(&eventStream);
}

ExperimentControlModel::~ExperimentControlModel()
{
    eventStream.Close();
    handleEventFuture.get();

    delete channelControlModel;
    delete imageManagerModel;
    delete sampleManagerModel;
}

ChannelControlModel *ExperimentControlModel::ChannelControlModel()
{
    return channelControlModel;
}

ImageManagerModel *ExperimentControlModel::ImageManagerModel()
{
    return imageManagerModel;
}

SampleManagerModel *ExperimentControlModel::SampleManagerModel()
{
    return sampleManagerModel;
}

QString ExperimentControlModel::GetUserDataRoot()
{
    return exp->BaseDir().string().c_str();
}

void ExperimentControlModel::SetExperimentDir(QString exp_dir)
{
    exp->OpenExperimentDir(exp_dir.toStdString());
}

QString ExperimentControlModel::ExperimentDir()
{
    std::string path = exp->ExperimentDir().string();
    return QString::fromStdString(path);
}

void ExperimentControlModel::SetSelectedSite(Site *site)
{
    if (site != nullptr) {
        LOG_DEBUG("site {}/{}/{} selected", site->Well()->Plate()->ID(), site->Well()->ID(), site->ID());
    } else {
        LOG_DEBUG("site selection cleared");
    }

    selected_plate = nullptr;
    selected_well = nullptr;
    selected_site = site;
}

void ExperimentControlModel::StartLiveView()
{
    try {
        exp->StartLiveView();
    } catch (std::exception &e) {
        LOG_ERROR(e.what());
    }
}

void ExperimentControlModel::StopLiveView()
{
    try {
        exp->StopLiveView();
    } catch (std::exception &e) {
        LOG_ERROR(e.what());
    }
}

void ExperimentControlModel::handleEvents()
{
    Event e;
    while (eventStream.Receive(&e)) {
        switch (e.type) {
        case EventType::TaskStateChanged:
            emit stateChanged(e.value.c_str());
            break;
        case EventType::TaskChannelChanged:
            emit channelChanged(e.value.c_str());
            break;
        case EventType::TaskMessage:
            emit messageReceived(e.value.c_str());
            break;
        case EventType::ExperimentOpened:
            emit experimentOpened(e.value.c_str());
            sampleManagerModel->handleExperimentOpen();
            imageManagerModel->handleExperimentOpen();
            break;
        case EventType::ExperimentClosed:
            emit experimentClosed();
            sampleManagerModel->handleExperimentClose();
            imageManagerModel->handleExperimentClose();
            break;
        case EventType::PlateCreated:
            sampleManagerModel->handlePlateCreated(e.value.c_str());
            break;
        case EventType::PlateModified:
            sampleManagerModel->handlePlateModified(e.value.c_str());
            break;
        case EventType::CurrentPlateChanged:
            sampleManagerModel->handleCurrentPlateChanged(e.value.c_str());
            break;
        case EventType::NDImageCreated:
            imageManagerModel->handleNDImageCreated(e.value);
            break;
        case EventType::NDImageChanged:
            imageManagerModel->handleNDImageChanged(e.value);
            break;
        case EventType::DevicePropertyValueUpdate:
            if (e.path.ToString() == "/PriorProScan/XYPosition") {
                QStringList list = QString(e.value.c_str()).split(",");
                if (list.length() != 2) {
                    return;
                }
                sampleManagerModel->handleStagePositionUpdate(list[0].toDouble(), list[1].toDouble());
            }
            break;
        }
    }
}

void ExperimentControlModel::StartMultiChannelTask()
{
    std::vector<Channel> channels = channelControlModel->GetEnabledChannels();
    exp->AcquireMultiChannel("test", channels, 0, 0);
}
