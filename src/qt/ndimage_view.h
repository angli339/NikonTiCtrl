#ifndef NDIMAGE_VIEW_H
#define NDIMAGE_VIEW_H

#include <vector>

#include <QGridLayout>
#include <QWidget>
#include <QScrollBar>
#include <QLabel>

#include "qt/glimageview.h"

class NDImageChannelView;

class NDImageView : public QWidget {
    Q_OBJECT
public:
    explicit NDImageView(QWidget *parent = nullptr);

    void setNChannels(int n_channel);
    void setNDimZ(int n_z);
    void setNDimT(int n_t);

    void setIndexZ(int i_z);
    void setIndexT(int i_t);
    void setChannelName(int i_channel, QString channelName);
    void setFrameData(int i_channel, ImageData frame);

public:
    QGridLayout *imGridLayout = nullptr;
    std::vector<NDImageChannelView *> channelViewList;

    QScrollBar *zSlider;
    QLabel *zLabelCurrent;
    QLabel *zLabelTotal;

    QScrollBar *tSlider;
    QLabel *tLabelCurrent;
    QLabel *tLabelTotal;
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
