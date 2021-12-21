#ifndef NDIMAGE_VIEW_H
#define NDIMAGE_VIEW_H

#include <vector>

#include <QGridLayout>
#include <QWidget>

#include "qt/glimageview.h"

class NDImageChannelView;

class NDImageView : public QWidget {
    Q_OBJECT
public:
    explicit NDImageView(QWidget *parent = nullptr);

    void setNumChannels(int n_channel);
    void setFrameData(int i_channel, ImageData frame);
    void setChannelName(int i_channel, QString channelName);

public:
    QGridLayout *layout = nullptr;

    std::vector<NDImageChannelView *> channelViewList;
};

class NDImageChannelView : public QWidget {
    Q_OBJECT
public:
    explicit NDImageChannelView(QWidget *parent = nullptr);

public:
    GLImageView *glImageView = nullptr;
    GLImageControlBar *controlBar = nullptr;
};

#endif
