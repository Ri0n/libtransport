/**
 * XMPP - libpurple transport
 *
 * Copyright (C) 2009, Jan Kaluza <hanzz@soc.pidgin.im>
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

#include <iostream>
#include "transport/conversation.h"
#include "transport/conversationmanager.h"
#include "transport/user.h"
#include "transport/transport.h"
#include "transport/buddy.h"
#include "transport/rostermanager.h"

namespace Transport {

Conversation::Conversation(ConversationManager *conversationManager, const std::string &legacyName, bool isMUC) : m_conversationManager(conversationManager) {
	m_legacyName = legacyName;
	m_muc = isMUC;
	m_jid = m_conversationManager->getUser()->getJID().toBare();
	m_sentInitialPresence = false;
}

Conversation::~Conversation() {
}

void Conversation::destroyRoom() {
	if (m_muc) {
		Swift::Presence::ref presence = Swift::Presence::create();
		std::string legacyName = m_legacyName;
		if (legacyName.find_last_of("@") != std::string::npos) {
			legacyName.replace(legacyName.find_last_of("@"), 1, "%"); // OK
		}
		presence->setFrom(Swift::JID(legacyName, m_conversationManager->getComponent()->getJID().toBare(), m_nickname));
		presence->setType(Swift::Presence::Unavailable);

		Swift::MUCItem item;
		item.affiliation = Swift::MUCOccupant::NoAffiliation;
		item.role = Swift::MUCOccupant::NoRole;
		item.actor = "Transport";
		item.reason = "Spectrum 2 transport is being shut down.";
		Swift::MUCUserPayload *p = new Swift::MUCUserPayload ();
		p->addItem(item);

		Swift::MUCUserPayload::StatusCode c;
		c.code = 332;
		p->addStatusCode(c);
		Swift::MUCUserPayload::StatusCode c2;
		c2.code = 307;
		p->addStatusCode(c2);

		presence->addPayload(boost::shared_ptr<Swift::Payload>(p));
		BOOST_FOREACH(const Swift::JID &jid, m_jids) {
			presence->setTo(jid);
			m_conversationManager->getComponent()->getStanzaChannel()->sendPresence(presence);
		}
	}
}

void Conversation::setRoom(const std::string &room) {
	m_room = room;
	m_legacyName = m_room + "/" + m_legacyName;
}

void Conversation::handleMessage(boost::shared_ptr<Swift::Message> &message, const std::string &nickname) {
	if (m_muc) {
		message->setType(Swift::Message::Groupchat);
	}
	else {
		if (message->getType() == Swift::Message::Headline) {
			if (m_conversationManager->getUser()->getUserSetting("send_headlines") != "1") {
				message->setType(Swift::Message::Chat);
			}
		}
		else {
			message->setType(Swift::Message::Chat);
		}
	}

	std::string n = nickname;
	if (n.empty() && !m_room.empty() && !m_muc) {
		n = m_nickname;
	}

	if (message->getType() != Swift::Message::Groupchat) {
		message->setTo(m_jid);
		// normal message
		if (n.empty()) {
			Buddy *buddy = m_conversationManager->getUser()->getRosterManager()->getBuddy(m_legacyName);
			if (buddy) {
				message->setFrom(buddy->getJID());
			}
			else {
				std::string name = m_legacyName;
				if (CONFIG_BOOL_DEFAULTED(m_conversationManager->getComponent()->getConfig(), "service.jid_escaping", true)) {
					name = Swift::JID::getEscapedNode(m_legacyName);
				}
				else {
					if (name.find_last_of("@") != std::string::npos) {
						name.replace(name.find_last_of("@"), 1, "%");
					}
				}

				message->setFrom(Swift::JID(name, m_conversationManager->getComponent()->getJID().toBare(), "bot"));
			}
		}
		// PM message
		else {
			if (m_room.empty()) {
				message->setFrom(Swift::JID(n, m_conversationManager->getComponent()->getJID().toBare(), "user"));
			}
			else {
				message->setFrom(Swift::JID(m_room, m_conversationManager->getComponent()->getJID().toBare(), n));
			}
		}

		if (m_conversationManager->getComponent()->inServerMode() && m_conversationManager->getUser()->shouldCacheMessages()) {
			boost::posix_time::ptime timestamp = boost::posix_time::second_clock::universal_time();
			boost::shared_ptr<Swift::Delay> delay(boost::make_shared<Swift::Delay>());
			delay->setStamp(timestamp);
			message->addPayload(delay);
			m_cachedMessages.push_back(message);
			if (m_cachedMessages.size() > 100) {
				m_cachedMessages.pop_front();
			}
		}
		else {
			m_conversationManager->getComponent()->getStanzaChannel()->sendMessage(message);
		}
	}
	else {
		std::string legacyName = m_legacyName;
		if (legacyName.find_last_of("@") != std::string::npos) {
			legacyName.replace(legacyName.find_last_of("@"), 1, "%"); // OK
		}

		std::string n = nickname;
		if (n.empty()) {
			n = " ";
		}

		message->setFrom(Swift::JID(legacyName, m_conversationManager->getComponent()->getJID().toBare(), n));

		if (m_jids.empty()) {
			boost::posix_time::ptime timestamp = boost::posix_time::second_clock::universal_time();
			boost::shared_ptr<Swift::Delay> delay(boost::make_shared<Swift::Delay>());
			delay->setStamp(timestamp);
			message->addPayload(delay);
			m_cachedMessages.push_back(message);
			if (m_cachedMessages.size() > 100) {
				m_cachedMessages.pop_front();
			}
		}
		else {
			BOOST_FOREACH(const Swift::JID &jid, m_jids) {
				message->setTo(jid);
				// Subject has to be sent after our own presence (the one with code 110)
				if (!message->getSubject().empty() && m_sentInitialPresence == false) {
					m_subject = message;
					return;
				}
				m_conversationManager->getComponent()->getStanzaChannel()->sendMessage(message);
			}
		}
	}
	if (m_conversationManager->getUser()->getUserSetting("enable_notifications") == "1"
		&& m_conversationManager->getUser()->shouldCacheMessages()) {
			std::cout << "Should send notification!" << std::endl;
	}
}

void Conversation::sendParticipants(const Swift::JID &to) {
	for (std::map<std::string, Participant>::iterator it = m_participants.begin(); it != m_participants.end(); it++) {
		Swift::Presence::ref presence = generatePresence(it->first, it->second.flag, it->second.status, it->second.statusMessage, "");
		presence->setTo(to);
		m_conversationManager->getComponent()->getStanzaChannel()->sendPresence(presence);
	}
}

void Conversation::sendCachedMessages(const Swift::JID &to) {
	for (std::list<boost::shared_ptr<Swift::Message> >::const_iterator it = m_cachedMessages.begin(); it != m_cachedMessages.end(); it++) {
		if (to.isValid()) {
			(*it)->setTo(to);
		}
		else {
			(*it)->setTo(m_jid.toBare());
		}
		m_conversationManager->getComponent()->getStanzaChannel()->sendMessage(*it);
	}
	m_cachedMessages.clear();
}

Swift::Presence::ref Conversation::generatePresence(const std::string &nick, int flag, int status, const std::string &statusMessage, const std::string &newname) {
	std::string nickname = nick;
	Swift::Presence::ref presence = Swift::Presence::create();
	std::string legacyName = m_legacyName;
	if (m_muc) {
		if (legacyName.find_last_of("@") != std::string::npos) {
			legacyName.replace(legacyName.find_last_of("@"), 1, "%"); // OK
		}
	}
	presence->setFrom(Swift::JID(legacyName, m_conversationManager->getComponent()->getJID().toBare(), nickname));
	presence->setType(Swift::Presence::Available);

	if (!statusMessage.empty())
		presence->setStatus(statusMessage);

	Swift::StatusShow s((Swift::StatusShow::Type) status);

	if (s.getType() == Swift::StatusShow::None) {
		presence->setType(Swift::Presence::Unavailable);
	}

	presence->setShow(s.getType());

	Swift::MUCUserPayload *p = new Swift::MUCUserPayload ();
	if (m_nickname == nickname) {
		Swift::MUCUserPayload::StatusCode c;
		c.code = 110;
		p->addStatusCode(c);
		m_sentInitialPresence = true;
	}

	
	Swift::MUCItem item;
	
	item.affiliation = Swift::MUCOccupant::Member;
	item.role = Swift::MUCOccupant::Participant;

	if (flag & Moderator) {
		item.affiliation = Swift::MUCOccupant::Admin;
		item.role = Swift::MUCOccupant::Moderator;
	}

	if (!newname.empty()) {
		item.nick = newname;
		Swift::MUCUserPayload::StatusCode c;
		c.code = 303;
		p->addStatusCode(c);
		presence->setType(Swift::Presence::Unavailable);
	}

	p->addItem(item);
	presence->addPayload(boost::shared_ptr<Swift::Payload>(p));
	return presence;
}

void Conversation::handleParticipantChanged(const std::string &nick, int flag, int status, const std::string &statusMessage, const std::string &newname) {
	Swift::Presence::ref presence = generatePresence(nick, flag, status, statusMessage, newname);

	if (presence->getType() == Swift::Presence::Unavailable) {
		m_participants.erase(nick);
	}
	else {
		Participant p;
		p.flag = flag;
		p.status = status;
		p.statusMessage = statusMessage;
		m_participants[nick] = p;
	}


	BOOST_FOREACH(const Swift::JID &jid, m_jids) {
		presence->setTo(jid);
		m_conversationManager->getComponent()->getStanzaChannel()->sendPresence(presence);
	}
	if (!newname.empty()) {
		handleParticipantChanged(newname, flag, status, statusMessage);
	}

	if (m_sentInitialPresence && m_subject) {
		m_conversationManager->getComponent()->getStanzaChannel()->sendMessage(m_subject);
		m_subject.reset();
	}
}

}
