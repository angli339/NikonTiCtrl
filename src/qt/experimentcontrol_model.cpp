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
    return exp->UserDataRoot().string().c_str();
}

void ExperimentControlModel::SetExperimentDir(QString exp_dir)
{
    exp->OpenExperiment(exp_dir.toStdString());
}

QString ExperimentControlModel::ExperimentDir()
{
    std::string path = exp->ExperimentDir().string();
    return QString::fromStdString(path);
}

void ExperimentControlModel::SetSelectedSite(Site *site)
{
    if (site != nullptr) {
        LOG_DEBUG("site {} selected", site->FullID());
    } else {
        LOG_DEBUG("site selection cleared");
    }

    selected_array = nullptr;
    selected_sample = nullptr;
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
        case EventType::ExperimentPathChanged:
            emit experimentPathChanged(e.value.c_str());
            break;
        case EventType::NDImageCreated:
            imageManagerModel->handleNDImageCreated(e.value);
            break;
        case EventType::NDImageChanged:
            imageManagerModel->handleNDImageChanged(e.value);
            break;
        }
    }
}

void ExperimentControlModel::StartMultiChannelTask()
{
    std::vector<Channel> channels = channelControlModel->GetEnabledChannels();
    exp->AcquireMultiChannel("test", channels, 0, 0);
}
