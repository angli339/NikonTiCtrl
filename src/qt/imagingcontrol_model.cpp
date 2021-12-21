#include "qt/imagingcontrol_model.h"

#include "logging.h"

ImagingControlModel::ImagingControlModel(ImagingControl *imagingControl,
                                         QObject *parent)
    : QObject(parent)
{
    this->imagingControl = imagingControl;

    channelControlModel =
        new ::ChannelControlModel(imagingControl->ChannelControl());
    dataManagerModel = new ::DataManagerModel(imagingControl->DataManager());
    sampleManagerModel =
        new ::SampleManagerModel(imagingControl->SampleManager());

    handleEventFuture = std::async(std::launch::async,
                                   &ImagingControlModel::handleEvents, this);
    imagingControl->SubscribeEvents(&eventStream);
}

ImagingControlModel::~ImagingControlModel()
{
    eventStream.Close();
    handleEventFuture.get();

    delete channelControlModel;
    delete dataManagerModel;
    delete sampleManagerModel;
}

ChannelControlModel *ImagingControlModel::ChannelControlModel()
{
    return channelControlModel;
}

DataManagerModel *ImagingControlModel::DataManagerModel()
{
    return dataManagerModel;
}

SampleManagerModel *ImagingControlModel::SampleManagerModel()
{
    return sampleManagerModel;
}

void ImagingControlModel::StartLiveView()
{
    try {
        imagingControl->StartLiveView();
    } catch (std::exception &e) {
        LOG_ERROR(e.what());
    }
}

void ImagingControlModel::StopLiveView()
{
    try {
        imagingControl->StopLiveView();
    } catch (std::exception &e) {
        LOG_ERROR(e.what());
    }
}

void ImagingControlModel::handleEvents()
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
            dataManagerModel->handleExperimentPathChanged(e.value);
            break;
        case EventType::NDImageCreated:
            dataManagerModel->handleNDImageCreated(e.value);
            break;
        case EventType::NDImageChanged:
            dataManagerModel->handleNDImageChanged(e.value);
            break;
        }
    }
}

void ImagingControlModel::StartMultiChannelTask()
{
    std::vector<Channel> channels = channelControlModel->GetEnabledChannels();
    imagingControl->AcquireMultiChannel("test", channels, 0, 0);
}
