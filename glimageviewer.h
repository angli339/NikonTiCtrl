#ifndef GLIMAGEVIEWER_H
#define GLIMAGEVIEWER_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include "image.h"

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

class GLImageViewer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    using QOpenGLWidget::QOpenGLWidget;
    ~GLImageViewer();

    uint16_t getImageWidth() { return imageWidth; }
    uint16_t getImageHeight() { return imageHeight; }

public slots:
    void displayImage(const Image *im);
    void setVmin(uint16_t vmin);
    void setVmax(uint16_t vmax);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    QOpenGLShaderProgram *m_program = nullptr;

    QOpenGLTexture *texture = nullptr;
    QOpenGLTexture *texture_cmap = nullptr;

    bool imageDataSet = false;
    uint16_t imageWidth = 0;
    uint16_t imageHeight = 0;
    uint16_t vmin = 0;
    uint16_t vmax = 65535;
    uint8_t icmap = 1;

    void setImageFormat(uint16_t width, uint16_t height);
};

#endif // GLIMAGEVIEWER_H
