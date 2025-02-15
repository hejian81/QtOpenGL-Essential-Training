#include "widget.h"
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QTime>
#include "GLTool.h"
Widget::Widget(QWidget *parent)
    : QOpenGLWidget(parent)
    , texture(QOpenGLTexture::Target2D)
{
    timer.setInterval(20);        //设置定时器刷新间隔
                                  //定时调用重绘函数repaint(也可以调用update，只不过update不是直接响应)
    connect(&timer,&QTimer::timeout,this,static_cast<void (QWidget::*)()>(&QWidget::repaint));
    timer.start();

    QSurfaceFormat format;
    format.setSamples(10);
    this->setFormat(format);
}

Widget::~Widget()
{
}

void Widget::setBlurRadius(int radius)
{
    guassWeight.resize(radius);
    float sum=1;
    guassWeight[0]=1;
    for(int i=1;i<radius;i++){
        guassWeight[i] = exp(-i*i/(float)radius);
        sum+=guassWeight[i]*2;
    }
    for(int i=0;i<radius;i++)
        guassWeight[i]/=sum;
}

void Widget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    xBlur=new QOpenGLFramebufferObject(size());       //创建与窗口同等尺寸的帧缓存对象
    yBlur=new QOpenGLFramebufferObject(size());       //创建与窗口同等尺寸的帧缓存对象

    blurFilter=GLTool::createFilter(
                "#version 450 core\n"
                "uniform sampler2D tex;\n"
                "uniform float[128] guassWeight;\n"
                "uniform int blurRadius;"
                "uniform bool horizontal;"
                "in vec2 texCoord;\n"
                "out vec4 FragColor;\n"
                "void main(){\n"
                "   vec2 offset=1.0/textureSize(tex,0);\n"
                "   if(horizontal)"
                "       offset.y=0;"
                "   else"
                "       offset.x=0;"
                "   vec4 color=texture(tex, texCoord)*guassWeight[0];\n"
                "   for(int i=1;i<blurRadius;i++){\n"
                "       color+=texture(tex,texCoord+offset*vec2(i))*guassWeight[i];\n"
                "       color+=texture(tex,texCoord-offset*vec2(i))*guassWeight[i];\n"
                "   }\n"
                "   FragColor=color;\n"
                "}");


    glowFilter=GLTool::createFilter(
                "#version 450 core\n"
                "uniform sampler2D rawTex;\n"
                "uniform sampler2D blurTex;\n"
                "uniform float exposure;\n"     //曝光系数
                "uniform float gamma;\n"        //伽马
                "in vec2 texCoord;\n"
                "out vec4 FragColor;\n"
                "void main(){\n"
                "   vec4 rawColor  = texture(rawTex,texCoord);\n"
                "   vec4 blurColor = texture(blurTex,texCoord);\n"
                "   vec3 mapped = vec3(1.0)-exp(-(blurColor.rgb*exposure));\n"
                "   mapped = pow(mapped,1.0/vec3(gamma));\n"
                "   FragColor=vec4(mix(mapped,rawColor.rgb*exposure,rawColor.a), blurColor.a);\n"
                "}");

    setBlurRadius(50);
}

void Widget::paintGL()
{
    QString text=QTime::currentTime().toString();
    QFont font("微软雅黑",40);
    QFontMetrics fontMetrics(font);                                               //Qt中用来测量字体尺寸的
    QRect bounding=fontMetrics.boundingRect(text);
    QRect viewport(0,0,bounding.width(),bounding.height());                       //将OpenGL的视口调整为文字区域，且在屏幕中心
    viewport.moveCenter(rect().center());
    glViewport(viewport.x(),viewport.y(),viewport.width(),viewport.height());
    glClear(GL_COLOR_BUFFER_BIT);
    GLTool::drawText(text,font,QColor(50,80,250));

    glViewport(0,0,width(),height());                                             //重置视口为窗口大小
    QOpenGLFramebufferObject::blitFramebuffer(xBlur,rect(),nullptr,rect());

    blurFilter->runFilter(xBlur,rect(),yBlur,rect(),[this](QOpenGLShaderProgram& program){
        program.setUniformValue("horizontal",true);
        program.setUniformValueArray("guassWeight",guassWeight.data(),guassWeight.size(),1);
        program.setUniformValue("blurRadius",guassWeight.size());
    });

    blurFilter->runFilter(yBlur,rect(),xBlur,rect(),[this](QOpenGLShaderProgram& program){
        program.setUniformValue("horizontal",false);
        program.setUniformValueArray("guassWeight",guassWeight.data(),guassWeight.size(),1);
        program.setUniformValue("blurRadius",guassWeight.size());
    });

    glowFilter->runFilter(rect(),[this](QOpenGLShaderProgram& program){
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,xBlur->texture());
        program.setUniformValue("blurTex",1);
        program.setUniformValue("exposure",10.0f);
        program.setUniformValue("gamma",1.f);
    });

}

void Widget::resizeGL(int w, int h)
{
    glViewport(0,0,w,h);

    delete xBlur;
    xBlur=new QOpenGLFramebufferObject(w,h);       //重新帧缓存对象

    delete yBlur;
    yBlur=new QOpenGLFramebufferObject(w,h);
}


