#include "qt/imagingcontrol_model.h"

#include "logging.h"

ExperimentControlModel::ExperimentControlModel(ExperimentControl *imagingControl,
                                         QObject *parent)
    : QObject(parent)
{
    this->experimentControl = imagingControl;

    channelControlModel =
        new ::ChannelControlModel(imagingControl->Channels());
    imageManagerModel = new ::ImageManagerModel(imagingControl->Images());
    sampleManagerModel =
        new ::SampleManagerModel(imagingControl->Samples());

    handleEventFuture = std::async(std::launch::async,
                                   &ExperimentControlModel::handleEvents, this);
    imagingControl->SubscribeEvents(&eventStream);
}

ExperimentControlModel::~ExperimentControlModel()
{
    eventStream.Close();
    handleEventFuture.get();

    delete channelControlModel;
    delete imageManagerModel;
    delete sampleManagerModel;
}

void ExperimentControlModel::SetExperimentDir(QString exp_dir)
{
    experimentControl->OpenExperiment(exp_dir.toStdString());
}

QString ExperimentControlModel::ExperimentDir()
{
    std::string path = experimentControl->ExperimentDir().string();
    return QString::fromStdString(path);
}

ChannelControlModel *ExperimentControlModel::ChannelControlModel()
{
    return channelControlModel;
}

ImageManagerModel *ExperimentControlModel::DataManagerModel()
{
    return imageManagerModel;
}

SampleManagerModel *ExperimentControlModel::SampleManagerModel()
{
    return sampleManagerModel;
}

void ExperimentControlModel::StartLiveView()
{
    try {
        experimentControl->StartLiveView();
    } catch (std::exception &e) {
        LOG_ERROR(e.what());
    }
}

void ExperimentControlModel::StopLiveView()
{
    try {
        experimentControl->StopLiveView();
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
    experimentControl->AcquireMultiChannel("test", channels, 0, 0);
}
