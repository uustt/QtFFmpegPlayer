#include "VideoCanvas.h"
#include <QDebug>
#include <QThread>
extern "C"
{
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

#define GET_STR(x) #x
#define A_VER 3
#define T_VER 4

//顶点shader
const char*vString = GET_STR(
	attribute vec4 vertexIn;
	attribute vec2 textureIn;
	varying vec2 textureOut;

	void main(void)
	{
		gl_Position = vertexIn;
		textureOut = textureIn;
	}
);
//片元shader
const char* tString = GET_STR(
	varying vec2 textureOut;
	uniform sampler2D tex_y;
	uniform sampler2D tex_u;
	uniform sampler2D tex_v;
	void main(void)
	{
		vec3 yuv;
		vec3 rgb;
		yuv.x = texture2D(tex_y, textureOut).r;
		yuv.y = texture2D(tex_u, textureOut).r - 0.5;
		yuv.z = texture2D(tex_v, textureOut).r - 0.5;
		rgb = mat3(1.0, 1.0, 1.0,
		0, -0.39465, 2.03211,
		1.13983, -0.58060, 0.0
		)* yuv;
		//计算灰度
		//float gray = rgb.r * 0.299 + rgb.g * 0.587 + rgb.b * 0.114;
		//gl_FragColor = vec4(gray, gray, gray, 1.0);
		gl_FragColor = vec4(rgb, 1.0);
	}
);

static const char *fragNV12 = GET_STR(

	varying vec2 textureOut;
	uniform sampler2D yTexture;
	uniform sampler2D uvTexture;
	void main() {
		vec3 yuv;
		vec3 rgb;
		yuv.r = texture2D(yTexture, vTexCoord).r;
		yuv.g = texture2D(uvTexture, vTexCoord).r - 0.5;
		yuv.b = texture2D(uvTexture, vTexCoord).a - 0.5;
		rgb = mat3(1.0, 1.0, 1.0,
			0.0, -0.39465, 2.03211,
			1.13983, -0.58060, 0.0)*yuv;
	//输出像素颜色
		gl_FragColor = vec4(rgb, 1.0);
	}
);

static const char *fragNV21 = GET_STR(
	varying vec2 vTexCoord;     //顶点着色器传递的坐标
	uniform sampler2D yTexture; //输入的材质（不透明灰度，单像素）
	uniform sampler2D uvTexture;
	void main() {
		vec3 yuv;
		vec3 rgb;
		yuv.r = texture2D(yTexture, vTexCoord).r;
		yuv.g = texture2D(uvTexture, vTexCoord).a - 0.5;
		yuv.b = texture2D(uvTexture, vTexCoord).r - 0.5;
		rgb = mat3(1.0, 1.0, 1.0,
			0.0, -0.39465, 2.03211,
			1.13983, -0.58060, 0.0)*yuv;
		//输出像素颜色
		gl_FragColor = vec4(rgb, 1.0);
	}
);

VideoCanvas::VideoCanvas(QWidget* parent)
	:QOpenGLWidget(parent){}


VideoCanvas::~VideoCanvas(){}


void VideoCanvas::Init(int width, int height)
{
	QMutexLocker locker(&mutex);
	this->width = width;
	this->height = height;

	delete datas[0];
	delete datas[1];
	delete datas[2];
	//分配内存空间
	datas[0] = new unsigned char[width * height * 10];
	datas[1] = new unsigned char[width * height / 4 * 10];
	datas[2] = new unsigned char[width * height / 4 * 10];

	//清理
	if (texs[0])
	{
		glDeleteTextures(3, texs);
	}
	//创建材质
	glGenTextures(3, texs);
	//Y
	glBindTexture(GL_TEXTURE_2D, texs[0]);
	//放大缩小过滤器，线性插值  GL_NEAREST(效率高，但马赛克严重)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//创建材质空间(显卡空间）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	//U
	glBindTexture(GL_TEXTURE_2D, texs[1]);
	//放大缩小过滤器，线性插值
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//创建材质空间(显卡空间）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	//V
	glBindTexture(GL_TEXTURE_2D, texs[2]);
	//放大缩小过滤器，线性插值
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//创建材质空间(显卡空间）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
}

void VideoCanvas::Repaint(AVFrame *frame)
{
	while (isRepainting) {
		if (isExit) return;
		QThread::msleep(1);
	}
	isRepainting = true;
	if (!frame) return;
	QMutexLocker locker(&mutex);
	//保证尺寸正确，保证是视频帧
	if (!datas[0] || width * height == 0 || frame->width != this->width || frame->height != this->height)
	{
		av_frame_free(&frame);
		return;
	}
	//不需要对齐
	if (width == frame->linesize[0])
	{
		memcpy(datas[0], frame->data[0], width * height);
		memcpy(datas[1], frame->data[1], width * height / 4);
		memcpy(datas[2], frame->data[2], width * height / 4);
	}
	else
	{
		//行对齐
		for (int i = 0; i < height; i++)
		{
			memcpy(datas[0] + width * i, frame->data[0] + frame->linesize[0] * i, width);
		}
		for (int i = 0; i < height / 2; i++)
		{
			memcpy(datas[1] + width / 2 * i, frame->data[1] + frame->linesize[1] * i, width);
		}
		for (int i = 0; i < height / 2; i++)
		{
			memcpy(datas[2] + width / 2 * i, frame->data[2] + frame->linesize[2] * i, width);
		}
	}

	locker.unlock();
	av_frame_free(&frame);
	//刷新显示
	update();

	callUpdate++;
	if (callUpdate % 100 == 0)
	{
		//qDebug() << "callupdate:" << callUpdate << " update time:" << updateTime;
	}
}
//绘制YUV数据
void VideoCanvas::Repaint(unsigned char* yuv[])
{
	while (isRepainting) {
		if (isExit) return;
		QThread::msleep(1);
	}
	isRepainting = true;
	memcpy(datas[0], yuv[0], width * height);
	memcpy(datas[1], yuv[1], width * height / 4);
	memcpy(datas[2], yuv[2], width * height / 4);
	update();
	callUpdate++;
	if (callUpdate % 100 == 0)
	{
		//qDebug() << "callupdate:" << callUpdate << " update time:" << updateTime;
	}
}

void VideoCanvas::paintGL()
{
	QMutexLocker locker(&mutex);

	glActiveTexture(GL_TEXTURE0);
	//绑定到材质
	glBindTexture(GL_TEXTURE_2D, texs[0]);
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[0]);
	//修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, datas[0]);
	//与shader变量关联
	glUniform1i(unis[0], 0);



	glActiveTexture(GL_TEXTURE1);
	//绑定到材质
	glBindTexture(GL_TEXTURE_2D, texs[1]);
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[1]);
	//修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, datas[1]);
	//与shader变量关联
	glUniform1i(unis[1], 1);

	


	glActiveTexture(GL_TEXTURE2);
	//绑定到材质
	glBindTexture(GL_TEXTURE_2D, texs[2]);
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[2]);
	//修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, datas[2]);
	//与shader变量关联
	glUniform1i(unis[2], 2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	updateTime++;
	isRepainting = false;
}

void VideoCanvas::initializeGL()
{

	QMutexLocker locker(&mutex);
	initializeOpenGLFunctions();

	//加载shader脚本
	program.addShaderFromSourceCode(QGLShader::Vertex, vString);
	program.addShaderFromSourceCode(QGLShader::Fragment, tString);
	//设置定点坐标的变量
	program.bindAttributeLocation("vertexIn", A_VER);
	//设置材质坐标
	program.bindAttributeLocation("textureIn", T_VER);

	program.link();
	program.bind();

	//传递定点和材质坐标
	//顶点
	static const GLfloat ver[] = {
		-1.0f,	-1.0f,
		1.0f,	-1.0f,
		-1.0f,	1.0f,
		1.0f,	1.0f
	};
	//材质坐标
	static const GLfloat tex[] = {
		0.0f,	1.0f,
		1.0f,	1.0f,
		0.0f,	0.0f,
		1.0f,	0.0f
	};

	//顶点
	glVertexAttribPointer(A_VER, 2, GL_FLOAT, false, 0, ver);
	glEnableVertexAttribArray(A_VER);

	//材质
	glVertexAttribPointer(T_VER, 2, GL_FLOAT, false, 0, tex);
	glEnableVertexAttribArray(T_VER);

	//从shader获取材质
	unis[0] = program.uniformLocation("tex_y");
	unis[1] = program.uniformLocation("tex_u");
	unis[2] = program.uniformLocation("tex_v");

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void VideoCanvas::resizeGL(int w, int h)
{

}

