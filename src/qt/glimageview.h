#ifndef GLIMAGEVIEW_H
#define GLIMAGEVIEW_H

#include <atomic>
#include <mutex>

#include <QLabel>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QSlider>

#include "image/ndimage.h"
#include "qt/imagehistview.h"

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

class GLImageView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    using QOpenGLWidget::QOpenGLWidget;
    ~GLImageView();

    uint16_t getImageWidth() { return imageWidth; }
    uint16_t getImageHeight() { return imageHeight; }
    void setImageFrame(ImageData frame);

public slots:
    void setVmin(uint16_t vmin);
    void setVmax(uint16_t vmax);
    void setCmap(uint8_t icmap);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    QOpenGLShaderProgram *m_program = nullptr;

    QOpenGLTexture *texture = nullptr;
    QOpenGLTexture *texture_cmap = nullptr;

    std::mutex frame_mutex;
    ImageData frame;
    std::atomic<bool> new_frame_set = false;

    uint16_t imageWidth = 0;
    uint16_t imageHeight = 0;
    uint16_t vmin = 0;
    uint16_t vmax = 65535;
    uint8_t icmap = 0;
};

class GLImageControlBar : public QWidget {
    Q_OBJECT
public:
    explicit GLImageControlBar(QWidget *parent = nullptr);

public slots:
    void setChannelName(QString channelName)
    {
        channelNameLabel->setText(channelName);
    }

public:
    ImageHistView *histView;
    QLabel *channelNameLabel;
};

#endif
