#include "GLGraph.h"
#ifdef Q_WS_MACX
#include <gl.h>
#include <agl.h>
#else
#include <GL/gl.h>
#endif
#include <QPainter>
#include <math.h>
#include <QMutex>
#include <QPoint>
#include <QMouseEvent>

void GLGraph::reset(QWidget *prnt, QMutex *mut)
{
    if (prnt != parent())  setParent(prnt);
    ptsMut = mut;
    bg_Color = QColor(0x2f, 0x4f, 0x4f);
    graph_Color = QColor(0xee, 0xdd, 0x82);
    grid_Color = QColor(0x87, 0xce, 0xfa, 0x7f);
    min_x = 0., max_x = 1.; 
    gridLineStipplePattern = 0xf0f0; // 4pix on 4 off 4 on 4 off
    auto_update = true;
    need_update = false;

    yscale = 1.;
    pointsWB = 0;
    auto_update = false;
    setNumHGridLines(4);
    setNumVGridLines(4);
    auto_update = true;

    setAutoBufferSwap(true);
}

GLGraph::GLGraph(QGLContext *ctx, QWidget *p, QMutex *mut)
    : QGLWidget(ctx, p), ptsMut(mut)
{
    reset(p, mut);
}

GLGraph::GLGraph(QWidget *parent, QMutex *mut)
    : QGLWidget(parent), ptsMut(mut)    
{
    reset(parent, mut);
}

GLGraph::~GLGraph() 
{}

void GLGraph::initializeGL()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    //glEnable(GL_LINE_SMOOTH);
    //glEnable(GL_POINT_SMOOTH);
}


void GLGraph::resizeGL(int w, int h)
{    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, w, h);
    gluOrtho2D( 0., 1., -1., 1.);
}

void GLGraph::paintGL()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(bg_Color.redF(), bg_Color.greenF(), bg_Color.blueF(), bg_Color.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);


    drawGrid();

    if (ptsMut) ptsMut->lock();

    if (pointsWB && pointsWB->size()) {
        glPushMatrix();
        if (fabs(max_x-min_x) > 0.) {
            glScaled(1./(max_x-min_x), yscale, 1.);
        }
        glTranslated(-min_x, 0., 0.);
        drawPoints();
        glPopMatrix();
    }

    if (ptsMut) ptsMut->unlock();

    glDisableClientState(GL_VERTEX_ARRAY);

    need_update = false;
}

void GLGraph::drawGrid() const
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

void GLGraph::drawPoints() const 
{
    const Vec2 *pv1(0), *pv2(0);
    unsigned l1(0), l2(0);
    
    pointsWB->dataPtr1((Vec2 *&)pv1, l1);
    pointsWB->dataPtr2((Vec2 *&)pv2, l2);

    GLfloat savedColor[4];
    GLfloat savedWidth;
    // save some values
    glGetFloatv(GL_CURRENT_COLOR, savedColor);
    //glGetFloatv(GL_POINT_SIZE, &savedWidth);
    //glPointSize(1.0f);
    glGetFloatv(GL_LINE_WIDTH, &savedWidth);
    

    glLineWidth(1.0f);

    glColor4f(graph_Color.redF(), graph_Color.greenF(), graph_Color.blueF(), graph_Color.alphaF());

    if (l1 > 1) {
        glVertexPointer(2, GL_DOUBLE, 0, pv1);
        glDrawArrays(GL_LINE_STRIP, 0, l1);
    }
    if (pv2 && l2 > 1) {
            glVertexPointer(2, GL_DOUBLE, 0, pv2);
            glDrawArrays(GL_LINE_STRIP, 0, l2);
            if (l1 > 1) {
                // now, draw the connecting line between the 2 groups..
                GLdouble cv[] = {
                    pv1[l1-1].x, pv1[l1-1].y,
                    pv2[0].x, pv2[0].y
                };
                glVertexPointer(2, GL_DOUBLE, 0, cv);
                glDrawArrays(GL_LINES, 0, 2);
            }
    }
    
    //glDrawArrays(GL_POINTS, 0, nPts);

    // restore saved values
    glColor4f(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    //glPointSize(savedWidth);
    glLineWidth(savedWidth);
}

void GLGraph::setNumHGridLines(unsigned n)
{
    nHGridLines = n;
    // setup grid points..
    gridHs.clear();
    gridHs.reserve(nHGridLines*2);
    for (unsigned i = 0; i < nHGridLines; ++i) {
        Vec2 v;
        v.x = 0.f;
        v.y = double(i)/double(nHGridLines) * 2.0f - 1.0f;
        gridHs.push_back(v);         
        v.x = 1.f;
        gridHs.push_back(v);         
    }
    if (auto_update) updateGL();
    else need_update = true;    
}

void GLGraph::setNumVGridLines(unsigned n)
{
    nVGridLines = n;
    // setup grid points..
    gridVs.clear();
    gridVs.reserve(nVGridLines*2);
    for (unsigned i = 0; i < nVGridLines; ++i) {
        Vec2 v;
        v.x = double(i)/double(nVGridLines);
        v.y = -1.f;
        gridVs.push_back(v);         
        v.y = 1.f;
        gridVs.push_back(v);                 
    }
    if (auto_update) updateGL();
    else need_update = true;
}

void GLGraph::setPoints(const Vec2WrapBuffer *va)
{
    pointsWB = va;
    if (auto_update) updateGL();
    else need_update = true;
}

void GLGraph::setYScale(double d)
{
    yscale = d;
    if (auto_update) updateGL();
    else need_update = true;    
}

void GLGraph::mouseMoveEvent(QMouseEvent *evt)
{
    Vec2 v(pos2Vec(evt->pos()));
    emit(cursorOver(v.x,v.y));
}

void GLGraph::mousePressEvent(QMouseEvent *evt)
{
    Vec2 v(pos2Vec(evt->pos()));
    emit(clicked(v.x,v.y));
}

void GLGraph::mouseDoubleClickEvent(QMouseEvent *evt)
{
    Vec2 v(pos2Vec(evt->pos()));
    emit(doubleClicked(v.x,v.y));
}

Vec2 GLGraph::pos2Vec(const QPoint & pos)
{
    Vec2 ret;
    ret.x = double(pos.x())/width();
    // invert Y
    int y = height()-pos.y();
    ret.y = (double(y)/height()*2.-1.)/yscale;
    ret.x = (ret.x * (maxx()-minx()))+minx();
    return ret;
}
