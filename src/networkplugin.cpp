/**
 * libtransport -- C++ library for easy XMPP Transports development
 *
 * Copyright (C) 2011, Jan Kaluza <hanzz.k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "transport/networkplugin.h"
#include "transport/user.h"
#include "transport/transport.h"
#include "transport/storagebackend.h"
#include "transport/rostermanager.h"
#include "transport/usermanager.h"
#include "transport/conversationmanager.h"
#include "Swiften/Swiften.h"
#include "Swiften/Server/ServerStanzaChannel.h"
#include "Swiften/Elements/StreamError.h"
#include "pbnetwork.pb.h"

namespace Transport {

#define WRAP(MESSAGE, TYPE) 	pbnetwork::WrapperMessage wrap; \
	wrap.set_type(TYPE); \
	wrap.set_payload(MESSAGE); \
	wrap.SerializeToString(&MESSAGE);

NetworkPlugin::NetworkPlugin(Swift::EventLoop *loop, const std::string &host, int port) {
	m_factories = new Swift::BoostNetworkFactories(loop);
	m_host = host;
	m_port = port;
	m_pingReceived = false;
	m_conn = m_factories->getConnectionFactory()->createConnection();
	m_conn->onDataRead.connect(boost::bind(&NetworkPlugin::handleDataRead, this, _1));
	m_conn->onConnectFinished.connect(boost::bind(&NetworkPlugin::handleConnected, this, _1));
	m_conn->onDisconnected.connect(boost::bind(&NetworkPlugin::handleDisconnected, this));

	m_pingTimer = m_factories->getTimerFactory()->createTimer(30000);
	m_pingTimer->onTick.connect(boost::bind(&NetworkPlugin::pingTimeout, this)); 
	connect();
}

NetworkPlugin::~NetworkPlugin() {
	delete m_factories;
}

void NetworkPlugin::handleMessage(const std::string &user, const std::string &legacyName, const std::string &msg, const std::string &nickname) {
	pbnetwork::ConversationMessage m;
	m.set_username(user);
	m.set_buddyname(legacyName);
	m.set_message(msg);
	m.set_nickname(nickname);

	std::string message;
	m.SerializeToString(&message);

	WRAP(message, pbnetwork::WrapperMessage_Type_TYPE_CONV_MESSAGE);
// 	std::cout << "SENDING MESSAGE\n";

	send(message);
}

void NetworkPlugin::handleSubject(const std::string &user, const std::string &legacyName, const std::string &msg, const std::string &nickname) {
	pbnetwork::ConversationMessage m;
	m.set_username(user);
	m.set_buddyname(legacyName);
	m.set_message(msg);
	m.set_nickname(nickname);

	std::string message;
	m.SerializeToString(&message);

	WRAP(message, pbnetwork::WrapperMessage_Type_TYPE_ROOM_SUBJECT_CHANGED);
// 	std::cout << "SENDING MESSAGE\n";

	send(message);
}

void NetworkPlugin::handleBuddyChanged(const std::string &user, const std::string &buddyName, const std::string &alias,
			const std::string &groups, int status, const std::string &statusMessage, const std::string &iconHash) {
	pbnetwork::Buddy buddy;
	buddy.set_username(user);
	buddy.set_buddyname(buddyName);
	buddy.set_alias(alias);
	buddy.set_groups(groups);
	buddy.set_status(status);
	buddy.set_statusmessage(statusMessage);
	buddy.set_iconhash(iconHash);

	std::string message;
	buddy.SerializeToString(&message);

	WRAP(message, pbnetwork::WrapperMessage_Type_TYPE_BUDDY_CHANGED);

	send(message);
}

void NetworkPlugin::handleDisconnected(const std::string &user, const std::string &legacyName, int error, const std::string &msg) {
	pbnetwork::Disconnected d;
	d.set_user(user);
	d.set_name(legacyName);
	d.set_error(error);
	d.set_message(msg);

	std::string message;
	d.SerializeToString(&message);

	WRAP(message, pbnetwork::WrapperMessage_Type_TYPE_DISCONNECTED);

	send(message);
}

void NetworkPlugin::handleParticipantChanged(const std::string &user, const std::string &nickname, const std::string &room, int flags, int status, const std::string &statusMessage, const std::string &newname) {
	pbnetwork::Participant d;
	d.set_username(user);
	d.set_nickname(nickname);
	d.set_room(room);
	d.set_flag(flags);
	d.set_newname(newname);
	d.set_status(status);
	d.set_statusmessage(statusMessage);

	std::string message;
	d.SerializeToString(&message);

	WRAP(message, pbnetwork::WrapperMessage_Type_TYPE_PARTICIPANT_CHANGED);

	send(message);
}

void NetworkPlugin::handleRoomChanged(const std::string &user, const std::string &r, const std::string &nickname) {
	pbnetwork::Room room;
	room.set_username(user);
	room.set_nickname(nickname);
	room.set_room(r);
	room.set_password("");

	std::string message;
	room.SerializeToString(&message);

	WRAP(message, pbnetwork::WrapperMessage_Type_TYPE_ROOM_NICKNAME_CHANGED);
 
	send(message);
}

void NetworkPlugin::handleConnected(bool error) {
	if (error) {
		std::cerr << "Connecting error\n";
		m_pingTimer->stop();
		exit(1);
	}
	else {
		std::cout << "Connected\n";
		m_pingTimer->start();
	}
}

void NetworkPlugin::handleDisconnected() {
	std::cerr << "Disconnected\n";
	m_pingTimer->stop();
	exit(1);
}

void NetworkPlugin::connect() {
	std::cout << "Trying to connect the server\n";
	m_conn->connect(Swift::HostAddressPort(Swift::HostAddress(m_host), m_port));
}

void NetworkPlugin::handleLoginPayload(const std::string &data) {
	pbnetwork::Login payload;
	if (payload.ParseFromString(data) == false) {
		// TODO: ERROR
		return;
	}
	handleLoginRequest(payload.user(), payload.legacyname(), payload.password());
}

void NetworkPlugin::handleLogoutPayload(const std::string &data) {
	pbnetwork::Logout payload;
	if (payload.ParseFromString(data) == false) {
		// TODO: ERROR
		return;
	}
	handleLogoutRequest(payload.user(), payload.legacyname());
}

void NetworkPlugin::handleConvMessagePayload(const std::string &data) {
	pbnetwork::ConversationMessage payload;
	if (payload.ParseFromString(data) == false) {
		// TODO: ERROR
		return;
	}

	handleMessageSendRequest(payload.username(), payload.buddyname(), payload.message());
}

void NetworkPlugin::handleJoinRoomPayload(const std::string &data) {
	pbnetwork::Room payload;
	if (payload.ParseFromString(data) == false) {
		// TODO: ERROR
		return;
	}

	handleJoinRoomRequest(payload.username(), payload.room(), payload.nickname(), payload.password());
}

void NetworkPlugin::handleLeaveRoomPayload(const std::string &data) {
	pbnetwork::Room payload;
	if (payload.ParseFromString(data) == false) {
		// TODO: ERROR
		return;
	}

	handleLeaveRoomRequest(payload.username(), payload.room());
}

void NetworkPlugin::handleDataRead(const Swift::ByteArray &data) {
	long expected_size = 0;
	m_data += data.toString();
// 	std::cout << "received data; size = " << m_data.size() << "\n";
	while (m_data.size() != 0) {
		if (m_data.size() >= 4) {
			expected_size = (((((m_data[0] << 8) | m_data[1]) << 8) | m_data[2]) << 8) | m_data[3];
// 			std::cout << "expected_size=" << expected_size << "\n";
			if (m_data.size() - 4 < expected_size)
				return;
		}
		else {
			return;
		}

		std::string msg = m_data.substr(4, expected_size);
		m_data.erase(0, 4 + expected_size);

		pbnetwork::WrapperMessage wrapper;
		if (wrapper.ParseFromString(msg) == false) {
			// TODO: ERROR
			return;
		}

		switch(wrapper.type()) {
			case pbnetwork::WrapperMessage_Type_TYPE_LOGIN:
				handleLoginPayload(wrapper.payload());
				break;
			case pbnetwork::WrapperMessage_Type_TYPE_LOGOUT:
				handleLogoutPayload(wrapper.payload());
				break;
			case pbnetwork::WrapperMessage_Type_TYPE_PING:
				sendPong();
				break;
			case pbnetwork::WrapperMessage_Type_TYPE_CONV_MESSAGE:
				handleConvMessagePayload(wrapper.payload());
				break;
			case pbnetwork::WrapperMessage_Type_TYPE_JOIN_ROOM:
				handleJoinRoomPayload(wrapper.payload());
				break;
			case pbnetwork::WrapperMessage_Type_TYPE_LEAVE_ROOM:
				handleLeaveRoomPayload(wrapper.payload());
				break;
			default:
				return;
		}
	}
}

void NetworkPlugin::send(const std::string &data) {
	std::string header("    ");
// 	std::cout << data.size() << "\n";
	boost::int32_t size = data.size();
	for (int i = 0; i != 4; ++i) {
		header.at(i) = static_cast<char>(size >> (8 * (3 - i)));
// 		std::cout << std::hex << (int) header.at(i) << "\n";
	}

	m_conn->write(Swift::ByteArray(header + data));
}

void NetworkPlugin::sendPong() {
	m_pingReceived = true;
	std::string message;
	pbnetwork::WrapperMessage wrap;
	wrap.set_type(pbnetwork::WrapperMessage_Type_TYPE_PONG);
	wrap.SerializeToString(&message);

	send(message);
// 	std::cout << "SENDING PONG\n";
}

void NetworkPlugin::pingTimeout() {
	std::cout << "PINGTIMEOUT " << m_pingReceived << " " << this << "\n";
	if (m_pingReceived == false) {
		exit(1);
	}
	m_pingReceived = false;
	m_pingTimer->start();
}

}