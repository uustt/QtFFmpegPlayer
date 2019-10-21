#pragma once
#include <QMutex>
struct AVPacket;
struct AVFrame;
struct AVCodecParameters;
struct AVCodecContext;
class Decode
{
public:
	Decode();
	~Decode();

	bool Open(AVCodecParameters *para);

	//���͵������̣߳����ܳɹ��������pkt�ռ䣨�����ý������)
	bool Send(AVPacket *pkt);
	//��ȡ��������ݣ�һ��send���ܶ��receive������ȡ�����е�����send null��Recv���
	//ÿ�θ���һ�ݣ��ɵ������ͷ� av_frame_free
	AVFrame* Recv();

	void Clear();
	void Close();
	
private:

	QMutex mutex;
	const int CODEC_THREAD_COUNT = 8;

	//�����������ģ���Ҫ�ͷ� avcodec_free_context
	AVCodecContext* codec = NULL;
};
