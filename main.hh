#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>

class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		NetSocket();
		QList<int> PeerList();

		// Bind this socket to a P2Papp-specific default port.
		bool bind();

	private:
		int myPortMin, myPortMax;
};


class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
	NetSocket *sock;
	int currentSeqNum;
	int senderPort;
	QString local_origin;
	QMap<QString, quint32> localStatusMap;
	QByteArray last_message_sent;
	QMap<int, int> pingTimes;
	QList<int> pingList;
	QMap<QString, QMap<quint32, QMap<QString, QVariant>>> messageList;
	void sendMessage(QByteArray);
	void sendStatusMessage(QHostAddress sendto, int port);
	void sendPingMessage(QHostAddress sendto, int port);
	void sendPingReply(QHostAddress sendto, int port);
	void processPingMessage(QHostAddress sender, int senderPort);
	void processPingReply(QHostAddress sender, int senderPort);
	void processIncomingData(QByteArray datagramReceived, QHostAddress sender, int senderPort);
	QByteArray serializeLocalMessage(QString messageText);
	QByteArray serializeMessage(QMap<QString, QVariant> messageToSend);
	void processReceivedMessage(QMap<QString, QVariant> messageReceived, QHostAddress sender, int senderPort);
	void processStatusMessage(QMap<QString, QMap<QString, quint32>> peerWantMap, QHostAddress sender, int senderPort);
	QTimer *timer;
	QTimer *antiEntropyTimer;
	QElapsedTimer pingTimer;

public slots:
	void gotReturnPressed();
	void readPendingMessages();
	void timeoutHandler();
	void antiEntropyHandler();

private:
	QTextEdit *textview;
	QLineEdit *textline;
};

#endif // P2PAPP_MAIN_HH
