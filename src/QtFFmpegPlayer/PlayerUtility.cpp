#include "PlayerUtility.h"
#include <QDebug>
extern "C"
{
#include <libavutil/error.h>
}

PlayerUtility::PlayerUtility()
{
}


PlayerUtility::~PlayerUtility()
{
}

void PlayerUtility::av_strerror2(int errnum, const char* log)
{
	char buf[100] = {0};
	if (av_strerror(errnum, buf, sizeof(buf) - 1) == 0)
	{
		if (log != NULL && log[0] != '\0')
		{
			char buf2[512] = { 0 };
			sprintf(buf2, "[%s] %s",log, buf);
			qDebug() << buf2;
		}
		else
		{
			qDebug() << buf;
		}
	}
	else
	{
		qDebug("out error log failed, please check");
	}
}