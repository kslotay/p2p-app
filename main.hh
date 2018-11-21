#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>

// state struct for waiting ACK (status) or not
struct state {
    int waitingForStatus;
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

	public:
		NetSocket();
		QList<quint16> PeerList();

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
	quint32 currentSeqNum;
	int senderPort;
	QString local_origin;
	QMap<QString, quint32> localStatusMap;
	QMap<quint16, QMap<QString, QVariant>> last_message_sent;
	QMap<quint16, int> pingTimes;
	QList<quint16> pingList;
	QList<quint16> neighborList;
	// QMap<QString, quint16> neighborList;
	QMap<QString, QMap<quint32, QMap<QString, QVariant>>> messageList;
	void sendMessage(QByteArray buffer, quint16 port);
	void sendRandomMessage(QByteArray buffer);
	void sendStatusMessage(QHostAddress sendto, quint16 port);
	void sendPingMessage(QHostAddress sendto, quint16 port);
	void sendPingReply(QHostAddress sendto, quint16 port);
	void Ping();
	void processPingMessage(QHostAddress sender, quint16 senderPort);
	void processPingReply(quint16 senderPort);
	void processIncomingData(QByteArray datagramReceived, QHostAddress sender, quint16 senderPort);
	QByteArray serializeLocalMessage(QString messageText);
	QByteArray serializeMessage(QMap<QString, QVariant> messageToSend);
	void processReceivedMessage(QMap<QString, QVariant> messageReceived, QHostAddress sender, quint16 senderPort);
	void processStatusMessage(QMap<QString, QMap<QString, quint32>> peerWantMap, QHostAddress sender, quint16 senderPort);
	void cacheLastSentMessage(quint16 peerPost, QByteArray buffer);
	void pickNeighboring();	
	QTimer *timer;
	QTimer *antiEntropyTimer;
	QElapsedTimer pingTimer;
	QElapsedTimer pingFnTimer;

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
