#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

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
	int nextSeqNum;
	QString origin;
	QMap<QString, quint32> statusList;
	void sendMessage(QByteArray);
	void sendStatusMessage(QHostAddress sendto, int port);
	void processMessage(QByteArray datagramReceived, QHostAddress sender, int senderPort);


public slots:
	void gotReturnPressed();
	void readPendingMessages();

private:
	QTextEdit *textview;
	QLineEdit *textline;
};

#endif // P2PAPP_MAIN_HH
