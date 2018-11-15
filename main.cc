
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
	nextSeqNum = 1;
	origin = QString(sock->localPort());

	qDebug() << "origin: " << origin;

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
		qDebug() << "message sender: " << sender;
		qDebug() << "message senderPort: " << senderPort;
		qDebug() << "message in datagram: " << datagram.data();

		processMessage(datagram, sender, senderPort);
	}
}
// Process the message read from pending messages from sock
void ChatDialog::processMessage(QByteArray datagramReceived, QHostAddress sender, int senderPort)
{
	QMap<QString, QVariant> messageMap;
	QDataStream stream(&datagramReceived,  QIODevice::ReadOnly);
	QMap<quint32, QString> message;

	// Put stream into messageMap
	stream >> messageMap;

	if (messageMap.contains("ChatText")) {
		QString msg_chattext = messageMap.value("ChatText").toString();
		QString msg_origin = messageMap.value("Origin").toString();
		quint32 msg_seqnum = messageMap.value("SeqNo").toUInt();

		qDebug() << "message contains chattext: " << msg_chattext;
		qDebug() << "message contains origin: " << msg_origin;
		qDebug() << "message contains seqnum: " << msg_seqnum;

		// If statusMap[msg_origin] = 0, this is a new peer not on list
		// Set expected msg_num to 1
		if (statusMap[msg_origin] == 0) {
			statusMap[msg_origin] = 1;
		}

		// Modify statusMap before executing sendStatusMessage
		// If origin already in statusMap, seqno+1
		if (statusMap[msg_origin] == msg_seqnum) {
			// New expected message received
			qDebug() << "\nnew message received with seqnum: " << msg_seqnum;
			statusMap[msg_origin] = msg_seqnum+1;

			// Store into messageList, adding to existing nested QMap
			message = messageList[msg_origin];
			message[msg_seqnum] = msg_chattext;
			messageList[msg_origin] = message;

			qDebug() << "current message: " << message;
			qDebug() << "current messageList: " << messageList;

			// Show in chatdialog textview
			textview->append(messageMap.value("ChatText").toString());
		}
		else if (statusMap[msg_origin] > msg_seqnum) {
			// If expected message number greater than one being received
			// Message has already been seen, ignore
			qDebug() << "message already seen: " << msg_seqnum;
		}
		else {
			qDebug() << "waiting for msg with msgnum: " << statusMap[msg_origin];
		}

		qDebug() << "DEBUG: current statusMap: " << statusMap;

		sendStatusMessage(sender, senderPort);

		// Check if origin already in messageList
		// If there, add to existing map, else add message map as value
		// to origin key

//		qDebug() << "CHECKVAR: messageList[msg_origin]: " << messageList[msg_origin];

//		message = messageList[msg_origin];
//		if (!message.contains(msg_seqnum)) {
//			message[msg_seqnum] = msg_chattext;
//			messageList[msg_origin] = message;
//		}
//		else {
//			message[msg_seqnum] = msg_chattext;
//			messageList[msg_origin] = message;
//		}

	}
	else if (messageMap.contains("Want")) {
		// If message is a statusMessage, handle as QMap<QString, quint32>
		QMap<QString, quint32> wantMap;

		// Put stream data into wantMap
		stream >> wantMap;

		qDebug() << "\nDEBUG: message contains want:" << wantMap;

		// Compare statusMaps
		// and forward/request required messages
	}

}

void ChatDialog::gotReturnPressed()
{
	QByteArray buffer;
	QDataStream stream(&buffer,  QIODevice::ReadWrite);
	QMap<QString, QVariant> messageMap;

	// Initially, just echo the string locally.
	// Insert some networking code here...
	qDebug() << "FIX: send message to other peers: " << textline->text();

	// Define message QMap
	messageMap["ChatText"] = textline->text();
	messageMap["Origin"] = origin;
	messageMap["SeqNo"] = nextSeqNum;
	stream << messageMap;

	qDebug() << "message in chattext: " << messageMap["ChatText"];
	qDebug() << "message in origin: " << messageMap["Origin"];
	qDebug() << "message in seqno: " << messageMap["SeqNo"];

	textview->append(textline->text());

	sendMessage(buffer);

	// If local message being forwarded, increment, else don't
	nextSeqNum++;

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::sendMessage(QByteArray buffer)
{
	qDebug() << "message in buff: " << buffer;
	qDebug() << "message in sock: " << sock;

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
	statusMessage["Want"] = statusMap;

	qDebug() << "\nSending statusMessage: " << statusMessage["Want"];
	qDebug() << "message in buff: " << buffer;
	qDebug() << "message in sock: " << sock;
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

