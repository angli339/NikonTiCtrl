#ifndef CHANNELCONTROL_MODEL_H
#define CHANNELCONTROL_MODEL_H

#include <vector>

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#include "task/channelcontrol.h"

class ChannelControlModel : public QObject {
    Q_OBJECT
public:
    explicit ChannelControlModel(ChannelControl *channelControl,
                                 QObject *parent = nullptr);

    QStringList ListChannelNames();
    Channel GetChannel(QString name);
    void SetChannelExposure(QString name, int exposure_ms);
    void SetChannelIllumination(QString name, int illumination_intensity);

    void SwitchChannel(QString name);

    void SetChannelEnabled(QString name, bool enabled);
    bool IsChannelEnabled(QString name);
    std::vector<Channel> GetEnabledChannels();

private:
    ChannelControl *channelControl;

    QStringList channel_names;
    QMap<QString, bool> channel_enabled;
    QMap<QString, Channel> channels;
};

#endif
