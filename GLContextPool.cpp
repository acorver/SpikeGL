#include <QMessageBox>
#include "GLContextPool.h"

GLContextPool::GLContextPool(QObject *parent)
    : QObject(parent)
{
    resetCreationCount();
}

GLContextPool::~GLContextPool()
{
    while (!ch.empty()) {
        delete ch.front();
        ch.pop_front();
    }
}

QGLContext * GLContextPool::create() 
{
    QGLFormat f;
    f.setDepth(false);
    ++cCount;
    return new QGLContext(f);
}

QGLContext * GLContextPool::take()
{
    QGLContext *g = 0;
    if (ch.empty()) {
        g = create();
    } else {
        g = ch.back();
        ch.pop_back();
    }
    return g;
}

void GLContextPool::put(QGLContext *g)
{
    if (ch.contains(g)) return;
    ch.push_back(g);
}

