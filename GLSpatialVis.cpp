#include "GLSpatialVis.h"
#ifdef Q_WS_MACX
#include <gl.h>
#include <agl.h>
#else
#include <GL/gl.h>
#endif
#ifdef Q_WS_WIN32
#  include <GL/GLU.h>
#endif
#include <QPainter>
#include <math.h>
#include <QPoint>
#include <QMouseEvent>
#include "Util.h"
#include <QVarLengthArray.h>

void GLSpatialVis::reset()
{
	bool wasupden = updatesEnabled();
	setUpdatesEnabled(false);
    bg_Color = QColor(0x2f, 0x4f, 0x4f);
    grid_Color = QColor(0x87, 0xce, 0xfa, 0x7f);
	point_size = 1.0;
	glyph_size = Vec2f(-1.,-1.);
	hasSelection = false;
	selx1 = selx2 = sely1 = sely2 = 0.;
    gridLineStipplePattern = 0xf0f0; // 4pix on 4 off 4 on 4 off
    auto_update = true;
    need_update = false;
	glyph = Square;

    pointsDisplayBuf.clear();
	colorsDisplayBuf.clear();
    auto_update = false;
    setNumHGridLines(4);
    setNumVGridLines(4);
    auto_update = true;

    setAutoBufferSwap(true);	
	setUpdatesEnabled(wasupden);
}

GLSpatialVis::GLSpatialVis(QWidget *parent)
    : QGLWidget(parent)    
{
    reset();
}

GLSpatialVis::~GLSpatialVis() {}

void GLSpatialVis::initializeGL()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
}


void GLSpatialVis::resizeGL(int w, int h)
{    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, w, h);
    gluOrtho2D( 0., 1., 0., 1.);
	if (glyphType() == Square) updateVertexBuf();
}

void GLSpatialVis::paintGL()
{
	if (!isVisible()) return;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(bg_Color.redF(), bg_Color.greenF(), bg_Color.blueF(), bg_Color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);


    drawGrid();

    if (pointsDisplayBuf.size() && colorsDisplayBuf.size()) {
        glPushMatrix();
//        if (fabs(max_x-min_x) > 0.) {
          glScaled(1./*/(max_x-min_x)*/, /*yscale*/1., 1.);
//        }
        drawPoints();
        glPopMatrix();
    }

	drawSelection();
	
    glDisableClientState(GL_VERTEX_ARRAY);

    need_update = false;
}

void GLSpatialVis::drawGrid() const
{
    bool wasEnabled = glIsEnabled(GL_LINE_STIPPLE);
    if (!wasEnabled)
        glEnable(GL_LINE_STIPPLE);
    
    GLint savedPat(0), savedRepeat(0);
    GLfloat savedWidth;
    GLfloat savedColor[4];
    // save some values
    glGetFloatv(GL_CURRENT_COLOR, savedColor);
    glGetIntegerv(GL_LINE_STIPPLE_PATTERN, &savedPat);
    glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &savedRepeat);
    glGetFloatv(GL_LINE_WIDTH, &savedWidth);

    glLineStipple(1, gridLineStipplePattern); 
    glLineWidth(1.f);
    glColor4f(grid_Color.redF(), grid_Color.greenF(), grid_Color.blueF(), grid_Color.alphaF());
    glVertexPointer(2, GL_DOUBLE, 0, &gridVs[0]);
    glDrawArrays(GL_LINES, 0, nVGridLines*2);
    glVertexPointer(2, GL_DOUBLE, 0, &gridHs[0]);
    glDrawArrays(GL_LINES, 0, nHGridLines*2);

    glLineStipple(1, 0xffff);
    static const GLfloat h[] = { 0.f, 0.f, 1.f, 0.f };
    glVertexPointer(2, GL_FLOAT, 0, h);
    glDrawArrays(GL_LINES, 0, 2);

    // restore saved values
    glColor4f(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    glLineStipple(savedRepeat, savedPat);
    glLineWidth(savedWidth);
    if (!wasEnabled) glDisable(GL_LINE_STIPPLE);
}

void GLSpatialVis::drawCircles() const
{
    GLfloat savedWidth;
    // save some values
    glGetFloatv(GL_POINT_SIZE, &savedWidth);
    glPointSize(point_size);
    
	glEnableClientState(GL_COLOR_ARRAY);
	
	glVertexPointer(2, GL_DOUBLE, 0, pointsDisplayBuf.constData());
	glColorPointer(4, GL_FLOAT, 0, colorsDisplayBuf.constData());
	glDrawArrays(GL_POINTS, 0, pointsDisplayBuf.size());
	
	glDisableClientState(GL_COLOR_ARRAY);
	
    // restore saved values
    glPointSize(savedWidth);	
}

void GLSpatialVis::drawPoints() const 
{
	switch (glyph) {
		case Circle:
			drawCircles();
			break;
		case Square:
		default:
			drawSquares();
			break;
	}
}

void GLSpatialVis::drawSquares() const
{
	// in this mode, each point is the center of a square of glyphSize() width
	if (vbuf.size() != cbuf.size()) {
		Error() << "GLSparialVis::drawSquares INTERNAL error -- color buffer != points buffer size!";
		return;
	}
	GLint saved_polygonmode[2];
	glGetIntegerv(GL_POLYGON_MODE, saved_polygonmode);		   
	glPolygonMode(GL_FRONT, GL_FILL); // make suroe to fill the polygon;	

	glEnableClientState(GL_COLOR_ARRAY);
	
	glVertexPointer(2, GL_FLOAT, 0, vbuf.constData());
	glColorPointer(4, GL_FLOAT, 0, cbuf.constData());
	glDrawArrays(GL_QUADS, 0, vbuf.size());
	
	glDisableClientState(GL_COLOR_ARRAY);
	glPolygonMode(GL_FRONT, saved_polygonmode[0]);
}

void GLSpatialVis::drawSelection() const
{
	if (!isSelectionVisible()) return;
	int saved_polygonmode[2];
	// invert selection..
	glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

	glGetIntegerv(GL_POLYGON_MODE, saved_polygonmode);		   
	glColor4f(.5, .5, .5, .5);
	glPolygonMode(GL_FRONT, GL_FILL); // make suroe to fill the polygon;	
	const double vertices[] = {
		selx1, sely1,
		selx2, sely1,
		selx2, sely2,
		selx1, sely2
	};
	
	glVertexPointer(2, GL_DOUBLE, 0, vertices);
	glDrawArrays(GL_QUADS, 0, 4);	
	
	// restore saved OpenGL state
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT, saved_polygonmode[0]);
}

void GLSpatialVis::setNumHGridLines(unsigned n)
{
    nHGridLines = n;
    // setup grid points..
    gridHs.clear();
    gridHs.reserve(nHGridLines*2);
    for (unsigned i = 0; i < nHGridLines; ++i) {
        Vec2 v;
        v.x = 0.f;
        v.y = double(i)/double(nHGridLines);
        gridHs.push_back(v);         
        v.x = 1.f;
        gridHs.push_back(v);         
    }
    if (auto_update) updateGL();
    else need_update = true;    
}

void GLSpatialVis::setGlyphType(GLSpatialVis::GlyphType g)
{
	glyph = g;
	if (g == Square) {
		updateVertexBuf();
		updateColorBuf();
	}
	if (auto_update) updateGL();
	else need_update = true;
}


void GLSpatialVis::setNumVGridLines(unsigned n)
{
    nVGridLines = n;
    // setup grid points..
    gridVs.clear();
    gridVs.reserve(nVGridLines*2);
    for (unsigned i = 0; i < nVGridLines; ++i) {
        Vec2 v;
        v.x = double(i)/double(nVGridLines);
        v.y = 0;
        gridVs.push_back(v);         
        v.y = 1.f;
        gridVs.push_back(v);                 
    }
    if (auto_update) updateGL();
    else need_update = true;
}

void GLSpatialVis::setGlyphSize(Vec2f gs) 
{
	if (gs.x < 1.f || gs.y < 1.f) return;
	float ps = gs.x;
	if (gs.y < ps) ps = gs.y;
	const Vec2f oldgs = glyph_size;
	point_size = ps; 
	glyph_size = gs;
	if (glyphType() == Square && (int(oldgs.x) != int(gs.x) || int(oldgs.y) != int(gs.y))) {
		updateVertexBuf();
	}
	if (auto_update) updateGL();
    else need_update = true;
}


void GLSpatialVis::setPoints(const QVector<Vec2> & points)
{
	// constant time copy, because of copy-on-write semantics of QVector...
	pointsDisplayBuf = points;
	if (glyphType() == Square) updateVertexBuf();
    if (auto_update) updateGL();
    else need_update = true;	
}

void GLSpatialVis::updateColorBuf()
{
	int cdbsz = colorsDisplayBuf.size();
	cbuf.resize(cdbsz * 4);
	
	for (int i = 0, vi = 0; i < cdbsz; ++i, vi+=4) {
		// set vertex colors
		cbuf[vi] = cbuf[vi+1] = cbuf[vi+2] = cbuf[vi+3] = colorsDisplayBuf[i];
	}
	if (auto_update) updateGL();
	else need_update = true;
}

void GLSpatialVis::updateVertexBuf()
{
	// in this mode, each point is the center of a square of glyphSize() width
	const int pdbsz = pointsDisplayBuf.size();
	vbuf.resize(pdbsz * 4);
	const float xoff = (glyphSize().x/2.0f) / width(), yoff = (glyphSize().y/2.0f) / height();
	
	for (int i = 0, vi = 0; i < pdbsz; ++i, vi+=4) {
		const Vec2 & p (pointsDisplayBuf[i]);
		vbuf[vi+0].x = p.x-xoff, vbuf[vi+0].y = p.y-yoff; // bottom left
		vbuf[vi+1].x = p.x+xoff, vbuf[vi+1].y = p.y-yoff; // bottom right
		vbuf[vi+2].x = p.x+xoff, vbuf[vi+2].y = p.y+yoff; // top right
		vbuf[vi+3].x = p.x-xoff, vbuf[vi+3].y = p.y+yoff; // top left		
	}	
}

void GLSpatialVis::setColors(const QVector<Vec4f> & colors)
{
	if (pointsDisplayBuf.size() != colors.size()) {
		Error() << "INTERNAL ERROR: GLSpatialVis '" << objectName() << "' -- color buffer length != vector buffer length! Argh!";		
	}
	colorsDisplayBuf = colors;
	if (glyphType() == Square) updateColorBuf();	
	if (auto_update) updateGL();
	else need_update = true;
}

void GLSpatialVis::mouseMoveEvent(QMouseEvent *evt)
{
	emit(cursorOverWindowCoords(evt->x(), evt->y()));
    Vec2 v(pos2Vec(evt->pos()));
    emit(cursorOver(v.x,v.y));
}

void GLSpatialVis::mousePressEvent(QMouseEvent *evt)
{
	if (!(evt->buttons() & Qt::LeftButton)) return;
	emit(clickedWindowCoords(evt->x(), evt->y()));
    Vec2 v(pos2Vec(evt->pos()));
    emit(clicked(v.x,v.y));
}

void GLSpatialVis::mouseReleaseEvent(QMouseEvent *evt)
{
	if (evt->buttons() & Qt::LeftButton) return;
	emit(clickReleasedWindowCoords(evt->x(), evt->y()));
    Vec2 v(pos2Vec(evt->pos()));
    emit(clickReleased(v.x,v.y));
	
}

void GLSpatialVis::mouseDoubleClickEvent(QMouseEvent *evt)
{
	if (!(evt->buttons() & Qt::LeftButton)) return;
    Vec2 v(pos2Vec(evt->pos()));
    emit(doubleClicked(v.x,v.y));
}

Vec2 GLSpatialVis::pos2Vec(const QPoint & pos)
{
    Vec2 ret;
    ret.x = double(pos.x())/double(width());
    // invert Y
    int y = height()-pos.y();
    ret.y = double(y)/double(height());
    return ret;
}

void GLSpatialVis::setSelectionRange(double begin_x, double end_x, double begin_y, double end_y)
{
	if (begin_x > end_x) 
		selx1 = end_x, selx2 = begin_x;
	else
		selx1 = begin_x, selx2 = end_x;
	if (begin_y > end_y) 
		sely1 = end_y, sely2 = begin_y;
	else
		sely1 = begin_y, sely2 = end_y;
	if (auto_update) updateGL();
	else need_update = true;
}

void GLSpatialVis::setSelectionEnabled(bool onoff)
{
	hasSelection = onoff;
}


bool GLSpatialVis::isSelectionVisible() const
{
	return hasSelection && selx2 >= /*min_x*/0.0 && selx1 <= /*max_x*/1.0 && sely2 >= 0. && sely1 <= 1.;	
}

GLSpatialVisState GLSpatialVis::getState() const
{
	GLSpatialVisState s;
	
	s.bg_Color = bg_Color;
	s.grid_Color = grid_Color;
	s.nHGridLines = nHGridLines;
	s.nVGridLines = nVGridLines;
	s.glyphSize = glyphSize();
	s.gridLineStipplePattern = gridLineStipplePattern;
	s.selx1 = selx1;
	s.selx2 = selx2;
	s.sely1 = sely1;
	s.sely2 = sely2;
	s.hasSelection = hasSelection;
	s.objectName = objectName();
	
	return s;
}

void GLSpatialVis::setState(const GLSpatialVisState & s)
{
	bg_Color = s.bg_Color;
	grid_Color = s.grid_Color;
	setNumHGridLines(s.nHGridLines);
	setNumVGridLines(s.nVGridLines);
	gridLineStipplePattern = s.gridLineStipplePattern;
	setGlyphSize(s.glyphSize);
	selx1 = s.selx1;
	selx2 = s.selx2;
	sely1 = s.sely1;
	sely2 = s.sely2;
	hasSelection = s.hasSelection;
	setObjectName(s.objectName);
	if (auto_update) updateGL();
	else need_update = true;
}

void GLSpatialVis::setBGColor(QColor c)
{
	bg_Color = c;
	if (auto_update) updateGL();
	else need_update = true;
}

void GLSpatialVis::setGridColor(QColor c)
{
	grid_Color = c;
	if (auto_update) updateGL();
	else need_update = true;
}
