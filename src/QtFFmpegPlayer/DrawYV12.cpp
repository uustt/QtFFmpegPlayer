#include "DrawYV12.h"


//片元shader YV12
char* YV12_fragment = GET_STR(
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
	gl_FragColor = vec4(rgb, 1.0);
}
);

DrawYV12::DrawYV12(const char* url, int width, int height)
	: DrawYUVBase(url, width, height) {}


DrawYV12::~DrawYV12()
{
}

void DrawYV12::InitializeCanvas()
{

	if (!fp)
	{
		qDebug() << "open failed";
		return;
	}
	resize(width, height);

	datas[0] = new unsigned char[width * height];
	datas[1] = new unsigned char[width * height / 4];
	datas[2] = new unsigned char[width * height / 4];

	//创建材质
	glGenTextures(3, texs);

	glBindTexture(GL_TEXTURE_2D, texs[0]);
	//放大缩小过滤器，线性插值  GL_NEAREST(效率高，但马赛克严重)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//创建材质空间(显卡空间）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glBindTexture(GL_TEXTURE_2D, texs[1]);
	//放大缩小过滤器，线性插值
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//创建材质空间(显卡空间）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);


	glBindTexture(GL_TEXTURE_2D, texs[2]);
	//放大缩小过滤器，线性插值
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//创建材质空间(显卡空间）
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

}

void DrawYV12::paintGL()
{

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
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, datas[2]);
	//与shader变量关联
	glUniform1i(unis[1], 1);

	glActiveTexture(GL_TEXTURE2);
	//绑定到材质
	glBindTexture(GL_TEXTURE_2D, texs[2]);
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[2]);
	//修改材质内容
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, datas[1]);
	//与shader变量关联
	glUniform1i(unis[2], 2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	isDrawing = false;
}

void DrawYV12::initializeGL()
{
	initializeOpenGLFunctions();
	qDebug() << "initializeGL";
	//加载shader脚本
	program.addShaderFromSourceCode(QGLShader::Vertex, vertexFrag);
	program.addShaderFromSourceCode(QGLShader::Fragment, YV12_fragment);

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

void DrawYV12::resizeGL(int w, int h)
{

}

void DrawYV12::run()
{
	while (!isExit)
	{
		if (isDrawing)
		{
			QThread::msleep(1);
			continue;
		}

		if (feof(fp) != 0)
		{
			fseek(fp, 0, SEEK_SET);
		}
		fread(datas[0], 1, width * height, fp);
		fread(datas[1], 1, width * height / 4, fp);
		fread(datas[2], 1, width * height / 4, fp);

		update();
		isDrawing = true;
		QThread::msleep(1);
	}
}
