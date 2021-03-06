#include "UDPReceiver.h"
#include "PlayerUtility.h"
UDPReceiver::UDPReceiver(QObject *parent)
	: QObject(parent)
{
	uSocket = new QUdpSocket;
	int port = 5555;
	while (!uSocket->bind(QHostAddress("127.0.0.1"), port))
	{
		port++;
	}
	qDebug() << "start udp server success! ip=" << "127.0.0.1 port=" << port;
	
	connect(uSocket, &QUdpSocket::readyRead, this, &UDPReceiver::OnReceive);
}

UDPReceiver::~UDPReceiver()
{
	qDebug() << "close udp server";
}

void UDPReceiver::SendTo(QByteArray msg, QString host, int port)
{
	uSocket->writeDatagram(msg, QHostAddress(host), port);
}

void UDPReceiver::OnReceive()
{
	QByteArray ba;
	while (uSocket->hasPendingDatagrams())
	{
		ba.resize(uSocket->pendingDatagramSize());
		uSocket->readDatagram(ba.data(), ba.size());
		if (ba == "play")
		{
			PlayerUtility::Get()->isPause = false;
		}
		if (ba == "pause")
		{
			PlayerUtility::Get()->isPause = true;
		}
	}
}
