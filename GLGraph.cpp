#include "GLGraph.h"
#include <GL/gl.h>
#include <QPainter>
#include <math.h>
#include <QMutex>

GLGraph::GLGraph(QWidget *parent, QMutex *mut)
    : QGLWidget(parent), ptsMut(mut),
      bg_Color(0x2f, 0x4f, 0x4f), graph_Color(0xee, 0xdd, 0x82), grid_Color(0x87, 0xce, 0xfa, 0x7f),
      nHGridLines(4), nVGridLines(4), min_x(0.), max_x(1.), 
      gridLineStipplePattern(0xf0f0), // 4pix on 4 off 4 on 4 off
      auto_update(true), need_update(false)
    
{
    yscale = 1.;
    pointsArr = 0;
    nPts = 0;
    // setup grid points..
    gridHs.reserve(nHGridLines*2);
    for (unsigned i = 0; i < nHGridLines; ++i) {
        Vec2 v;
        v.x = 0.f;
        v.y = double(i)/double(nHGridLines) * 2.0f - 1.0f;
        gridHs.push_back(v);         
        v.x = 1.f;
        gridHs.push_back(v);         
    }

    gridVs.reserve(nVGridLines*2);
    for (unsigned i = 0; i < nVGridLines; ++i) {
        Vec2 v;
        v.x = double(i)/double(nVGridLines);
        v.y = -1.f;
        gridVs.push_back(v);         
        v.y = 1.f;
        gridVs.push_back(v);                 
    }

    setAutoBufferSwap(true);
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

    if (pointsArr && nPts) {
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
    const Vec2 *pv = pointsArr;

    GLfloat savedColor[4];
    GLfloat savedWidth;
    // save some values
    glGetFloatv(GL_CURRENT_COLOR, savedColor);
    //glGetFloatv(GL_POINT_SIZE, &savedWidth);
    //glPointSize(1.0f);
    glGetFloatv(GL_LINE_WIDTH, &savedWidth);
    

    glLineWidth(1.0f);

    glColor4f(graph_Color.redF(), graph_Color.greenF(), graph_Color.blueF(), graph_Color.alphaF());

    glVertexPointer(2, GL_DOUBLE, 0, pv);    
    glDrawArrays(GL_LINE_STRIP, 0, nPts);
    //glDrawArrays(GL_POINTS, 0, nPts);

    // restore saved values
    glColor4f(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    //glPointSize(savedWidth);
    glLineWidth(savedWidth);
}

void GLGraph::setPoints(const Vec2 *vertexArray, unsigned arraySize)
{
    pointsArr = vertexArray;
    nPts = arraySize;
    if (auto_update) updateGL();
    else need_update = true;
}

