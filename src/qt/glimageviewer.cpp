#include "glimageviewer.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

#include "logger.h"

extern const uint8_t cmap_data_viridis[];

GLImageViewer::~GLImageViewer()
{
    makeCurrent();

    delete texture;
    delete texture_cmap;
    delete m_program;

    doneCurrent();
}

void GLImageViewer::initializeGL()
{
    initializeOpenGLFunctions();

    m_program = new QOpenGLShaderProgram;

    m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/gl_image_viewer.vert");
    m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/gl_image_viewer_mono.frag");
    m_program->link();

    m_program->bind();

    static GLfloat const triangleVertices[] = {
        -1, 1, 1, 1, -1, -1, -1, -1, 1, 1, 1, -1
    };
    static GLfloat const texVertices[] = {
        0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1,
    };

    m_program->enableAttributeArray("a_position");
    m_program->setAttributeArray("a_position", triangleVertices, 2);

    m_program->enableAttributeArray("a_texCoord");
    m_program->setAttributeArray("a_texCoord", texVertices, 2);

    // Use Texture0 for u_image
    m_program->setUniformValue("u_image", 0);

    if (imageWidth != 0 && imageHeight != 0) {
        texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        texture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
        texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture->setFormat(QOpenGLTexture::R8_UNorm);
        texture->setSize(imageWidth, imageHeight);
        texture->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    }

    // Use Texture1 for u_colormaps
    m_program->setUniformValue("u_colormaps", 1);

    texture_cmap = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture_cmap->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    texture_cmap->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture_cmap->setFormat(QOpenGLTexture::RGB8_UNorm);
    texture_cmap->setSize(256, 1);
    texture_cmap->allocateStorage();
    texture_cmap->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, cmap_data_viridis);

    // Set default cmap & range
    uint8_t n_cmap = 1;
    float u_cmap_select = 1.0/n_cmap*(0.5+icmap);

    m_program->setUniformValue("u_blacklevel", vmin);
    m_program->setUniformValue("u_whitelevel", vmax);
    m_program->setUniformValue("u_highlight_clipped_under", 0, 0, 1);
    m_program->setUniformValue("u_highlight_clipped_over", 1, 0, 0);
    m_program->setUniformValue("u_cmap_select", u_cmap_select);

    m_program->release();

    auto err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR("GLImageViewer: initializeGL failed, err={:#06x}", err);
    }
}

void GLImageViewer::paintGL()
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if ((imageDataSet == false) || (texture == nullptr) || (texture_cmap == nullptr)) {
        return;
    }

    m_program->bind();
    texture->bind(0);
    texture_cmap->bind(1);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    texture->release();
    texture_cmap->release();
    m_program->release();
}

void GLImageViewer::resizeGL(int width, int height)
{
    const qreal retinaScale = devicePixelRatio();
    glViewport(0, 0, width * retinaScale, height * retinaScale);
}

void GLImageViewer::setVmin(uint16_t vmin) {
    this->vmin = vmin;

    m_program->bind();
    m_program->setUniformValue("u_blacklevel", vmin);
    m_program->release();

    update();
}

void GLImageViewer::setVmax(uint16_t vmax) {
    this->vmax = vmax;

    m_program->bind();
    m_program->setUniformValue("u_whitelevel", vmax);
    m_program->release();

    update();
}

void GLImageViewer::setImageFormat(uint16_t width, uint16_t height) {
    if (m_program == nullptr) {
        imageWidth = width;
        imageHeight = height;
        return;
    }

    makeCurrent();

    if (texture != nullptr) {
        texture->destroy();
        delete texture;
    }

    texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture->setFormat(QOpenGLTexture::R16_UNorm);
    texture->setSize(width, height);
    texture->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt16);

    auto err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR("GLImageViewer: setImageFormat({}, {}) failed, err={:#06x}", width, height, err);
    }

    doneCurrent();

    update();
}

void GLImageViewer::displayImage(const Image *im) {
    if ((imageWidth != im->width) || (imageHeight != im->height)) {
        setImageFormat(im->width, im->height);
    }

    makeCurrent();
    texture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16, (const void *)im->buf);
    doneCurrent();

    imageDataSet = true;
    update();
}
