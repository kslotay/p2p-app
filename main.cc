
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	// Create a UDP network socket
	sock = new NetSocket();
	if (!sock->bind())
		exit(1);

	// Initialize user-defined variables
	currentSeqNum = 1;
	// TODO: randomize origin with different seeds
	local_origin = QString::number(sock->localPort());

	qDebug() << "LOCAL ORIGIN: " << local_origin;

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));

	// Callback fired when message is received
	connect(sock, SIGNAL(readyRead()), this, SLOT(readPendingMessages()));

}

void ChatDialog::readPendingMessages()
{
	while (sock->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(sock->pendingDatagramSize());
		QHostAddress sender;
		quint16 senderPort;

		sock->readDatagram(datagram.data(), datagram.size(),
								&sender, &senderPort);

		qDebug() << "\nDEBUG: RECEIVING MESSAGE";

		processIncomingData(datagram, sender, senderPort);
	}
}

void ChatDialog::processReceivedMessage(QMap<QString, QVariant> messageReceived, QHostAddress sender, int senderPort)
{
	QString msg_chattext = messageReceived.value("ChatText").toString();
	QString msg_origin = messageReceived.value("Origin").toString();
	quint32 msg_seqnum = messageReceived.value("SeqNo").toUInt();

	qDebug() << "message contains chattext: " << msg_chattext;
	qDebug() << "message contains origin: " << msg_origin;
	qDebug() << "message contains seqnum: " << msg_seqnum;

	// If localStatusMap[msg_origin] = 0, this is a new peer not on list
	// Set expected msg_num to 1
	if (localStatusMap.value(msg_origin) == 0) {
		localStatusMap[msg_origin] = 1;
	}

	// Modify localStatusMap before executing sendStatusMessage
	// If origin already in localStatusMap, seqno+1
	if (localStatusMap.value(msg_origin) == msg_seqnum) {
		
		localStatusMap[msg_origin] = msg_seqnum+1;

		// Store into messageList, adding to existing nested QMap
		messageList[msg_origin][msg_seqnum]["ChatText"] = msg_chattext;
		messageList[msg_origin][msg_seqnum]["Origin"] = msg_origin;
		messageList[msg_origin][msg_seqnum]["SeqNo"] = msg_seqnum;

		qDebug() << "current messageList: " << messageList;

		// Show in chatdialog textview
		textview->append(msg_origin + ": " + messageReceived.value("ChatText").toString());
	}
	else if (localStatusMap[msg_origin] > msg_seqnum) {
		// If expected message number greater than one being received
		// Message has already been seen, ignore
		qDebug() << "message already seen: " << msg_seqnum;
	}
	else {
		qDebug() << "waiting for msg with msgnum: " << localStatusMap[msg_origin];
	}

	qDebug() << "DEBUG: localStatusMap: " << localStatusMap;

	sendStatusMessage(sender, senderPort);
}

void ChatDialog::processStatusMessage(QMap<QString, QMap<QString, quint32>> peerWantMap, QHostAddress sender, int senderPort) {
	
	QMap<QString, QVariant> messageToSend;
	// Unwrap peerWant
	QMap<QString, quint32> peerStatusMap = peerWantMap.value("Want");

	// Create enum for differences in status
	enum Status {INSYNC, AHEAD, BEHIND};

	qDebug() << "\nDEBUG: message contains want:" << peerStatusMap;
	qDebug() << "DEBUG: localStatusMap:" << localStatusMap;

	// Set initial status
	Status status = INSYNC;
	// Compare statusMaps using localStatus keys
	for (auto originKey : localStatusMap.keys()) {
		if (!peerStatusMap.contains(originKey)){
			status = AHEAD;
			// Add message to message buffer from messageList
			messageToSend = messageList[originKey][1];
			break;
		}
		else if (peerStatusMap.value(originKey) < localStatusMap.value(originKey)) {
			status = AHEAD;
			// Add message to message buffer from messageList
			messageToSend = messageList[originKey][peerStatusMap.value(originKey)];
			break;
		}
		else if (peerStatusMap.value(originKey) > localStatusMap.value(originKey)) {
			// Send status message
			status = BEHIND;
			break;
		}

		qDebug() << "local: key: " << originKey << " value: " << localStatusMap.value(originKey);
		qDebug() << "peer: key: " << originKey << " value: " << peerStatusMap.value(originKey);

	}

	// Compare statusMaps using peerStatus keys
	for (auto originKey : peerStatusMap.keys()) {
		if (!localStatusMap.contains(originKey)){
			status = BEHIND;
			// Add message to message buffer from messageList
			messageToSend = messageList[originKey][1];
			break;
		}
		else if (localStatusMap.value(originKey) < peerStatusMap.value(originKey)) {
			status = BEHIND;
			// Add message to message buffer from messageList
			messageToSend = messageList[originKey][localStatusMap.value(originKey)];
			break;
		}
		else if (localStatusMap.value(originKey) > peerStatusMap.value(originKey)) {
			// Send status message
			status = AHEAD;
			break;
		}

		qDebug() << "local: key: " << originKey << " value: " << localStatusMap.value(originKey);
		qDebug() << "peer: key: " << originKey << " value: " << peerStatusMap.value(originKey);

	}

	switch (status) {
		case INSYNC:
			// Coin flip and randomly send, or stop
			break;
		case AHEAD:
			//sendMessage(messageToSend);
			break;
		case BEHIND:
			sendStatusMessage(sender, senderPort);
			break;
	}
}

// Process the message read from pending messages from sock
void ChatDialog::processIncomingData(QByteArray datagramReceived, QHostAddress sender, int senderPort)
{
	// Stream for both msg and want, as stream is emptied on read
	QMap<QString, QVariant> messageReceived;
	QDataStream stream_msg(&datagramReceived,  QIODevice::ReadOnly);

	stream_msg >> messageReceived;

	QMap<QString, QMap<QString, quint32>> peerWantMap;
	QDataStream stream_want(&datagramReceived,  QIODevice::ReadOnly);

	stream_want >> peerWantMap;

	qDebug() << "messageReceived: " << messageReceived;
	qDebug() << "wantMap: " << peerWantMap;

	if (messageReceived.contains("ChatText")) {
		processReceivedMessage(messageReceived, sender, senderPort);
	}
	else if (peerWantMap.contains("Want")) {
		processStatusMessage(peerWantMap, sender, senderPort);
	}

}

QByteArray ChatDialog::serializeMessage(QString messageText)
{
	QVariantMap messageMap;

	messageMap.insert("ChatText", messageText);
	messageMap.insert("Origin", local_origin);
	messageMap.insert("SeqNo",currentSeqNum);


	// if we have already seen this origin
	if(messageList.contains(local_origin)) {
		messageList[local_origin][currentSeqNum] = messageMap;
	} 
	else {
		messageList[local_origin][currentSeqNum] = messageMap;
	}

	QByteArray buffer;
	QDataStream stream(&buffer,  QIODevice::ReadWrite);

	stream << messageMap;

	return buffer;
}

void ChatDialog::gotReturnPressed()
{

	QString text = textline->text();
	
	QByteArray messageBuffer = serializeMessage(text);

	textview->append(local_origin + ": " + textline->text());

	// Append to localStatusMap
	localStatusMap[local_origin] = currentSeqNum+1;

	sendMessage(messageBuffer);

	// If local message being forwarded, increment, else don't
	currentSeqNum++;

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::sendMessage(QByteArray buffer)
{
	QList<int> peerList = sock->PeerList();

	qDebug() << "Peer List: " << peerList;

	int index = rand() % peerList.size();

	qDebug() << "Sending to peer:" << peerList[index];

	sock->writeDatagram(buffer, buffer.size(), QHostAddress::LocalHost, peerList[index]);
}

void ChatDialog::sendStatusMessage(QHostAddress sendto, int port)
{
	QByteArray buffer;
	QDataStream stream(&buffer,  QIODevice::ReadWrite);
	QMap<QString, QMap<QString, quint32>> statusMessage;

	// Define message QMap
	statusMessage["Want"] = localStatusMap;

	qDebug() << "\nSending statusMessage: " << statusMessage["Want"];
//	qDebug() << "message in buff: " << buffer;
//	qDebug() << "message in sock: " << sock;
	qDebug() << "sending to peer: " << port;

	stream << statusMessage;

	sock->writeDatagram(buffer, buffer.size(), sendto, port);
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}

QList<int> NetSocket::PeerList()
{
    QList<int> peerList;
	for (int p = myPortMin; p <= myPortMax; p++) {
	    if (this->localPort() != p) {
            peerList.append(p);
	    }
	}
    return peerList;
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

