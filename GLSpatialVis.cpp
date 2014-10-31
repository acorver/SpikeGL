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
	hasSelection = false;
	selx1 = selx2 = sely1 = sely2 = 0.;
    gridLineStipplePattern = 0xf0f0; // 4pix on 4 off 4 on 4 off
    auto_update = true;
    need_update = false;

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

void GLSpatialVis::drawPoints() const 
{
    GLfloat savedWidth;
    // save some values
    glGetFloatv(GL_POINT_SIZE, &savedWidth);
    glPointSize(pointSize());
    
	glEnableClientState(GL_COLOR_ARRAY);
	
	glVertexPointer(2, GL_DOUBLE, 0, pointsDisplayBuf.constData());
	glColorPointer(4, GL_FLOAT, 0, colorsDisplayBuf.constData());
	glDrawArrays(GL_POINTS, 0, pointsDisplayBuf.size());

	glDisableClientState(GL_COLOR_ARRAY);

    // restore saved values
    glPointSize(savedWidth);
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

/*
void GLSpatialVis::setPoints(const Vec2WrapBuffer *pointsWB, const Vec3fWrapBuffer *colorsWB)
{
	if (!pointsWB || !colorsWB) {
		Error() << "INTERNAL ERROR: GLSpatialVis::setPoints() was passed a NULL points pointer!";
		return;
	}
    const Vec2 *pv1(0), *pv2(0);
	const Vec3f *pc1(0), *pc2(0);
    unsigned l1(0), l2(0), l3(0), l4(0);
    
    pointsWB->dataPtr1((Vec2 *&)pv1, l1);
    pointsWB->dataPtr2((Vec2 *&)pv2, l2);
    colorsWB->dataPtr1((Vec3f *&)pc1, l3);
    colorsWB->dataPtr2((Vec3f *&)pc2, l4);
	
    const size_t len = l1+l2, len2 = l3+l4;
	
	if (len == len2) {
		QVector<Vec2> & points ( pointsDisplayBuf );
		QVector<Vec3f> & colors ( colorsDisplayBuf );
		// copy point to display vertex buffer
		if (size_t(points.size()) != len) points.resize(len);
		if (size_t(colors.size()) != len2) colors.resize(len2);
		if (l1)  memcpy(points.data(), pv1, l1*sizeof(Vec2));
		if (l2)  memcpy(points.data()+l1, pv2, l2*sizeof(Vec2));
		if (l3)  memcpy(colors.data(), pc1, l3*sizeof(Vec3f));
		if (l4)  memcpy(colors.data()+l3, pc2, l4*sizeof(Vec3f));		
	} else {
		Error() << "INTERNAL ERROR: GLSpatialVis '" << objectName() << "' -- color buffer length != vector buffer length! Argh!";
	}
    if (auto_update) updateGL();
    else need_update = true;
}
*/

void GLSpatialVis::setPoints(const QVector<Vec2> & points, const QVector<Vec4f> & colors)
{
	if (points.size() == colors.size()) {
		// constant time copy, because of copy-on-write semantics of QVector...
		pointsDisplayBuf = points;
		colorsDisplayBuf = colors;
	} else {
		Error() << "INTERNAL ERROR: GLSpatialVis '" << objectName() << "' -- color buffer length != vector buffer length! Argh!";
	}
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
	s.pointSize = pointSize();
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
	setPointSize(s.pointSize);
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
