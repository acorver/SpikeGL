#ifndef GLGraph_H
#define GLGraph_H

#include <QGLWidget>
#include <QColor>
#include <vector>
#include <QVariant>
class QMutex;

#include "Vec2WrapBuffer.h"

class GLGraph : public QGLWidget
{
    Q_OBJECT
public:
    GLGraph(QGLContext *ctx, QWidget *parent = 0, QMutex *ptsMutex = 0);
    GLGraph(QWidget *parent = 0, QMutex *ptsMutex = 0);
	GLGraph(const QGLFormat & f, QWidget *p = 0, QMutex *mut = 0);
    virtual ~GLGraph();

    /// Reset the graph to default params, as if it were freshly constructed
    void reset(QWidget *parent = 0, QMutex *ptsMutex = 0);

    // associate whatever to this graph
    QVariant tag() const { return tagData; }
    void setTag(const QVariant & v) { tagData = v; }

    void setPoints(const Vec2WrapBuffer *pointsBuf);

    QColor & bgColor() { return bg_Color; }
    QColor & graphColor() { return graph_Color; }
    QColor & gridColor() { return grid_Color; }
    
    double & minx() { return min_x; }
    double & maxx() { return max_x; }

    double yScale() const { return yscale; }
    void setYScale(double);

    unsigned numVGridLines() const { return nVGridLines; }
    void setNumVGridLines(unsigned);

    unsigned numHGridLines() const { return nHGridLines; }
    void setNumHGridLines(unsigned);

    unsigned short & gridLineStipple() { return gridLineStipplePattern; }

    bool autoUpdate() const { return auto_update; }
    void setAutoUpdate(bool b) { auto_update = b; }

    bool needsUpdateGL() const { return need_update; }
	
	void setSelectionRange(double begin_x, double end_x);
	void setSelectionEnabled(bool onoff);
	bool isSelectionEnabled() const { return hasSelection; }
	bool isSelectionVisible() const;

signals:    
	/// like cursorOver(), except emitted x,y units are in window coordinates, not graph coordinates
	void cursorOverWindowCoords(int x, int y);
	/// like clicked(), except emitted x,y units are in window coordinates
	void clickedWindowCoords(int x, int y);
	void clickReleasedWindowCoords(int x, int y);
    /// for all the below: x is a time value, y is a graph Y-pos in range [-1,1]
    void cursorOver(double x, double y);
    void clicked(double x, double y); ///< this only emitted on Left mouse button clicks
    void clickReleased(double x, double y); ///< this only emitted on Right mouse button clicks
    void doubleClicked(double x, double y); ///< this only emitted on Left dbl-click

protected:
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

    Vec2 pos2Vec(const QPoint & pos);
    void mouseMoveEvent(QMouseEvent *evt);
    void mousePressEvent(QMouseEvent *evt);
    void mouseReleaseEvent(QMouseEvent *evt);
    void mouseDoubleClickEvent(QMouseEvent *evt);

private:
    void drawGrid() const;
    void drawPoints() const;
	void drawSelection() const;

    QMutex *ptsMut;
    QColor bg_Color, graph_Color, grid_Color;
    unsigned nHGridLines, nVGridLines;
    double min_x, max_x, yscale;
    unsigned short gridLineStipplePattern;
    const Vec2WrapBuffer *pointsWB;
    mutable QVector<Vec2> pointsDisplayBuf;
    std::vector<Vec2> gridVs, gridHs;
    bool auto_update, need_update;
    QVariant tagData;
	double selectionBegin, selectionEnd;
	bool hasSelection;
};

#endif
