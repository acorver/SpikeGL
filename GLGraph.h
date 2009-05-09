#ifndef GLGraph_H
#define GLGraph_H

#include <QGLWidget>
#include <QColor>
#include <vector>
class QMutex;

#include "Vec2WrapBuffer.h"

class GLGraph : public QGLWidget
{
    Q_OBJECT
public:
    GLGraph(QWidget *parent=0, QMutex *ptsMutex=0);
    virtual ~GLGraph();

    void setPoints(const Vec2WrapBuffer *pointsBuf);

    QColor & bgColor() { return bg_Color; }
    QColor & graphColor() { return graph_Color; }
    QColor & gridColor() { return grid_Color; }
    
    double & minx() { return min_x; }
    double & maxx() { return max_x; }

    double & yScale() { return yscale; }

    unsigned short & gridLineStipple() { return gridLineStipplePattern; }

    bool autoUpdate() const { return auto_update; }
    void setAutoUpdate(bool b) { auto_update = b; }

    bool needsUpdateGL() const { return need_update; }

signals:    
    // for all the below: x is a time value, y is a graph Y-pos in range [-1,1]
    void cursorOver(double x, double y); 
    void clicked(double x, double y);
    void doubleClicked(double x, double y);

protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    Vec2 pos2Vec(const QPoint & pos);
    void mouseMoveEvent(QMouseEvent *evt);
    void mousePressEvent(QMouseEvent *evt);
    void mouseDoubleClickEvent(QMouseEvent *evt);

private:
    void drawGrid() const;
    void drawPoints() const;

    QMutex *ptsMut;
    QColor bg_Color, graph_Color, grid_Color;
    unsigned nHGridLines, nVGridLines;
    double min_x, max_x, yscale;
    unsigned short gridLineStipplePattern;
    const Vec2WrapBuffer *pointsWB;
    std::vector<Vec2> gridVs, gridHs;
    bool auto_update, need_update;
};

#endif
