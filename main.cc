
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

	// // Initialize timer for message timeout
	// QTimer *timer = new QTimer(this);
	// connect(timer, SIGNAL(timeout()), this, SLOT(timeoutHandler()));

	// QTimer *antiEntropyTimer = new QTimer(this);
	// connect(antiEntropyTimer, SIGNAL(timeout()), this, SLOT(antiEntropyHandler()));
	
	// antiEntropyTimer->start(10000);	
	// Initialize user-defined variables
	currentSeqNum = 1;
	// TODO: randomize origin with different seeds
	local_origin = QString::number(sock->localPort());

	pingList = sock->PeerList();

	qDebug() << "LOCAL ORIGIN: " << local_origin;

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));

	// Callback fired when message is received
	connect(sock, SIGNAL(readyRead()), this, SLOT(readPendingMessages()));

	Ping();

	if (neighborList.size() < 1) {
		QList<int> peerList = sock->PeerList();

		int index = rand() % peerList.size();
		neighborList.append(peerList[index]);

		peerList.removeOne(peerList[index]);
		
		index = rand() % peerList.size();
		neighborList.append(peerList[index]);
	}
	else if (neighborList.size() < 2) {
		QList<int> peerList = sock->PeerList();

		int index = rand() % peerList.size();
		neighborList.append(peerList[index]);
	}
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
	QString msg_origin = messageReceived.value("Origin").toString();
	quint32 msg_seqnum = messageReceived.value("SeqNo").toUInt();

	qDebug() << "message contains origin: " << msg_origin;
	qDebug() << "message contains seqnum: " << msg_seqnum;

	// If localStatusMap[msg_origin] = 0, this is a new peer not on list
	// Set expected msg_num to 1 for comparison
	if (localStatusMap.value(msg_origin) == 0) {
		localStatusMap[msg_origin] = 1;
	}

	// Modify localStatusMap before executing sendStatusMessage
	// If origin already in localStatusMap, seqno+1
	if (localStatusMap.value(msg_origin) == msg_seqnum) {
		
		messageList[msg_origin][msg_seqnum] = messageReceived;

		localStatusMap[msg_origin] = msg_seqnum+1;
		
		// Show in chatdialog textview
		textview->append(msg_origin + ": " + messageReceived.value("ChatText").toString());

		// TODO: Forward message to random peer (not including one received from)
		sendMessage(serializeMessage(messageReceived));

	}
	else if (localStatusMap.value(msg_origin) > msg_seqnum) {
		// If expected message number greater than one being received
		// Message has already been seen, ignore
		qDebug() << "message already seen: " << msg_seqnum;
	}
	else {
		qDebug() << "waiting for msg with msgnum: " << localStatusMap.value(msg_origin);
	}

	// timer->stop();

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

	// Check for keys in peerStatusMap that are not in localStatusMap
	for (auto originKey : peerStatusMap.keys()) {
		if (!localStatusMap.contains(originKey)){
			status = BEHIND;
			break;
		}
		qDebug() << "local: key: " << originKey << " value: " << localStatusMap.value(originKey);
		qDebug() << "peer: key: " << originKey << " value: " << peerStatusMap.value(originKey);

	}
	// timer->stop();

	switch (status) {
		case INSYNC:
			// Coin flip and randomly send, or stop
			srand(time(0));

			if((rand() % 2) == 1) {
				// start rumor

				// qDebug() << "COIN FLIP ABOUT TO SEND";
				// sendMessage(last_message_sent);
			}
			else {
				// qDebug() << "COIN FLIP ABOUT TO STOP";
			}
			break;
		case AHEAD:
			sendMessage(serializeMessage(messageToSend));
			break;
		case BEHIND:
			sendStatusMessage(sender, senderPort);
			break;
	}
}

void ChatDialog::processPingMessage(QHostAddress sender, int senderPort) {
	// Send ping reply
	sendPingReply(sender, senderPort);
}

void ChatDialog::processPingReply(QHostAddress sender, int senderPort) {
	// Check timer, add to pingTimes, and remove port from pingList
	int pingTime = pingTimer.elapsed();

	pingTimes[senderPort] = pingTime;

	pingList.removeOne(senderPort);

	qDebug() << "pingTimes: " << pingTimes;

	if (pingTimes.size() > 1) {
		// take minimum ping as neighbor
		// neighborList.append();
	}
	
// 	if (!pingList.empty()) {
// 		pingTimer.restart();
// 		sendPingMessage(sender, pingList[0]);
// 	}
// 	else {
// 		qDebug() << "pingTimes: " << pingTimes;
// 	}
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

	// TODO: cleanup ping debugging
	QMap<QString, QString> pingMap;
	QDataStream stream_ping(&datagramReceived,  QIODevice::ReadOnly);
	stream_ping >> pingMap;
	qDebug() << "pingMap: " << pingMap;

	qDebug() << "messageReceived: " << messageReceived;
	qDebug() << "wantMap: " << peerWantMap;

	if (messageReceived.contains("ChatText")) {
		processReceivedMessage(messageReceived, sender, senderPort);

	}
	else if (peerWantMap.contains("Want")) {
		processStatusMessage(peerWantMap, sender, senderPort);
	}
	else if (pingMap.contains("Ping")) {
		processPingMessage(sender, senderPort);
	}
	else if (pingMap.contains("PingReply")) {
		processPingReply(sender, senderPort);
	}
	
	// timer->stop();
}

QByteArray ChatDialog::serializeLocalMessage(QString messageText)
{
	QVariantMap messageMap;

	messageMap.insert("ChatText", messageText);
	messageMap.insert("Origin", local_origin);
	messageMap.insert("SeqNo", currentSeqNum);

	messageList[local_origin][currentSeqNum] = messageMap;

	QByteArray buffer;
	QDataStream stream(&buffer,  QIODevice::ReadWrite);

	stream << messageMap;

	return buffer;
}

QByteArray ChatDialog::serializeMessage(QMap<QString, QVariant> messageToSend)
{
	QVariantMap messageMap;

	messageMap.insert("ChatText", messageToSend.value("ChatText"));
	messageMap.insert("Origin", messageToSend.value("Origin"));
	messageMap.insert("SeqNo", messageToSend.value("SeqNo"));

	QByteArray buffer;
	QDataStream stream(&buffer,  QIODevice::ReadWrite);

	stream << messageMap;

	return buffer;
}

void ChatDialog::timeoutHandler()
{
	qDebug() << "Time out occured";


	// what destination orgin timed out
	// send message from last_message_sent of that origin that timed out
	// remove that origin from last_message_sent
	// sendme	
	// sock->writeDatagram(, last_message_sent.size(), QHostAddress::LocalHost, senderPort);
	
	// timer->start(1000);
	

}

void ChatDialog::antiEntropyHandler() 
{
	QList<int> peerList = sock->PeerList();

	qDebug() << "Peer List: " << peerList;

	int index = rand() % peerList.size();

	sendStatusMessage(QHostAddress::LocalHost, peerList[index]);

	// antiEntropyTimer->start(10000);
}

void ChatDialog::gotReturnPressed()
{
	QString text = textline->text();

	textview->append(local_origin + ": " + textline->text());

	// Append to localStatusMap
	localStatusMap[local_origin] = currentSeqNum+1;

	sendMessage(serializeLocalMessage(text));

	// If local message being forwarded, increment, else don't
	currentSeqNum++;

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::sendMessage(QByteArray buffer)
{
	// QMap<QString, QVariant> lastMessageSentMap;
	// QDataStream stream(&datagramReceived,  QIODevice::ReadOnly);
	// stream >> lastMessageSentMap;

	QList<int> peerList = sock->PeerList();

	qDebug() << "Peer List: " << peerList;

	int index = rand() % peerList.size();

	qDebug() << "Sending to peer:" << peerList[index];
	
	sock->writeDatagram(buffer, buffer.size(), QHostAddress::LocalHost, peerList[index]);
	
	// todo find unique id for peer
	// last_message_sent[peerList[index]] = lastMessageSentMap;
	
}

void ChatDialog::sendPingMessage(QHostAddress sendto, int port)
{
	QByteArray ping;
	QDataStream stream(&ping,  QIODevice::ReadWrite);
	
	QMap<QString, QString> pingMsg;
	pingMsg["Ping"] = "Ping";

	stream << pingMsg;

	qDebug() << "Sending ping to peer: " << port;
	
	sock->writeDatagram(ping, ping.size(), sendto, port);
}

void ChatDialog::Ping() {
	int pingTimeout = 5000;
	int attempts = 3;
	pingFnTimer.start();

	int remainingTime = pingTimeout - pingFnTimer.elapsed();
	while ((remainingTime > 0) && (attempts > 0)) {
		remainingTime = pingTimeout - pingFnTimer.elapsed();
		
		int timeout2 = 1600;
	
		if(pingTimer.elapsed() > 0) {
			pingTimer.restart();	
		}
		else {
			pingTimer.start();
		}

		for (int i = 0; i < pingList.size(); i++) {
			sendPingMessage(QHostAddress::LocalHost, pingList[i]);
		}

		int remainingTime = timeout2 - pingTimer.elapsed();
		while (remainingTime > 0) {
			remainingTime = timeout2 - pingTimer.elapsed();
		}

		attempts--;
	}
}

void ChatDialog::sendPingReply(QHostAddress sendto, int port)
{
	QByteArray pingreply;
	QDataStream stream(&pingreply,  QIODevice::ReadWrite);
	
	QMap<QString, QString> pingReply;
	pingReply["PingReply"] = "PingReply";

	stream << pingReply;

	qDebug() << "Sending pingreply to peer: " << port;
	
	sock->writeDatagram(pingreply, pingreply.size(), sendto, port);
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

