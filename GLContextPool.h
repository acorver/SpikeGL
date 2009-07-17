#ifndef GLContextPool_H
#define GLContextPool_H

#include <QObject>
#include <QWidget>
#include <QMutex>
#include <QGLContext>
#include <QGLFormat>
#include <QList>

/// A class used to manage a pre-created pool of OpenGL contexts for QGLWidget subclasses
class GLContextPool : public QObject
{
public:
    GLContextPool(QObject *parent);
    ~GLContextPool();


    QGLContext *take(); ///< take a context from the pool, or create a new one if pool is empty
    void put(QGLContext *); ///< return a context back to the pool -- nothing really calls this..
    QGLContext *create(); ///< create a context, but don't add it to pool

    unsigned numInPool() const { return ch.size(); }
    unsigned creationCount() const { return cCount; }
    void resetCreationCount() { cCount = 0; }

private:
    QList<QGLContext *> ch;
    unsigned cCount;
};

#endif
