#ifndef GLSpatialVis_H
#define GLSpatialVis_H

#include <QGLWidget>
#include <QColor>
#include <vector>
#include <QVariant>

#include "Vec.h"
//#include "VecWrapBuffer.h"


struct GLSpatialVisState
{
    QColor bg_Color, grid_Color;
    unsigned nHGridLines, nVGridLines;
    unsigned short gridLineStipplePattern;
	Vec2f glyphSize;
	double selx1, selx2, sely1, sely2;
	bool hasSelection;	
	QString objectName;
};


class GLSpatialVis : public QGLWidget
{
    Q_OBJECT
public:
    GLSpatialVis(QWidget *parent = 0);
    virtual ~GLSpatialVis();

    /// Reset the graph to default params, as if it were freshly constructed
    void reset();

	void setPoints(const QVector<Vec2> & points);
	void setColors(const QVector<Vec4f> & colors);
	
	Vec2f glyphSize() const { return glyph_size; }
	void setGlyphSize(Vec2f gs);
	
	enum GlyphType { Square=0, Circle, N_GlyphType };

	GlyphType glyphType() const { return glyph; }
	void setGlyphType(GlyphType g);
	
    const QColor & bgColor() const { return bg_Color; }
    const QColor & gridColor() const { return grid_Color; }
	void setBGColor(QColor c);
	void setGridColor(QColor c);
    
    unsigned numVGridLines() const { return nVGridLines; }
    void setNumVGridLines(unsigned);

    unsigned numHGridLines() const { return nHGridLines; }
    void setNumHGridLines(unsigned);

    bool autoUpdate() const { return auto_update; }
    void setAutoUpdate(bool b) { auto_update = b; }

    bool needsUpdateGL() const { return need_update; }
	
	void setSelectionRange(double begin_x, double end_x, double begin_y, double end_y);
	void setSelectionEnabled(bool onoff);
	bool isSelectionEnabled() const { return hasSelection; }
	bool isSelectionVisible() const;

	GLSpatialVisState getState() const;
	void setState(const GLSpatialVisState & state);
	
signals:    
	/// like cursorOver(), except emitted x,y units are in window coordinates, not graph coordinates
	void cursorOverWindowCoords(int x, int y);
	/// like clicked(), except emitted x,y units are in window coordinates
	void clickedWindowCoords(int x, int y);
	void clickReleasedWindowCoords(int x, int y);
    /// for all the below: x is a value in range [0,1], y is a graph Y-pos in range [0,1]
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

    unsigned short & gridLineStipple() { return gridLineStipplePattern; }

private:
    void drawGrid() const;
    void drawPoints() const;
	void drawCircles() const;
	void drawSquares() const;
	void drawSelection() const;
	
	void updateColorBuf();
	void updateVertexBuf();

    QColor bg_Color, grid_Color;
	double point_size;
	Vec2f glyph_size;
	GlyphType glyph;
    unsigned nHGridLines, nVGridLines;
    unsigned short gridLineStipplePattern;
    QVector<Vec2> pointsDisplayBuf;
	QVector<Vec4f> colorsDisplayBuf;
	QVector<Vec2f> vbuf;///< scratch buff for vertices in squares mode, mostly
	QVector<Vec4f> cbuf; 
    std::vector<Vec2> gridVs, gridHs;
    bool auto_update, need_update;
	double selx1,selx2,sely1,sely2;
	bool hasSelection;
};

#endif
