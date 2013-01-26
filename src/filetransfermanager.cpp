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

#include "transport/filetransfermanager.h"
#include "transport/transport.h"
#include "transport/usermanager.h"
#include "transport/user.h"
#include "transport/buddy.h"
#include "transport/logging.h"
#include "Swiften/Network/ConnectionServerFactory.h"

namespace Transport {

DEFINE_LOGGER(logger, "FileTransferManager");

FileTransferManager::FileTransferManager(Component *component, UserManager *userManager) {
	m_component = component;
	m_userManager = userManager;

	m_jingleSessionManager = new Swift::JingleSessionManager(m_component->getIQRouter());
	m_connectivityManager = new Swift::ConnectivityManager(m_component->getNetworkFactories()->getNATTraverser());
	m_bytestreamRegistry = new Swift::SOCKS5BytestreamRegistry();
	m_bytestreamProxy = new Swift::SOCKS5BytestreamProxy(m_component->getNetworkFactories()->getConnectionFactory(), m_component->getNetworkFactories()->getTimerFactory());

	m_localCandidateGeneratorFactory = new Swift::DefaultLocalJingleTransportCandidateGeneratorFactory(m_connectivityManager, m_bytestreamRegistry, m_bytestreamProxy, "thishouldnotbeused");
	m_remoteCandidateSelectorFactory = new Swift::DefaultRemoteJingleTransportCandidateSelectorFactory(m_component->getNetworkFactories()->getConnectionFactory(), m_component->getNetworkFactories()->getTimerFactory());

	boost::shared_ptr<Swift::ConnectionServer> server = m_component->getNetworkFactories()->getConnectionServerFactory()->createConnectionServer(19645);
	server->start();
	m_bytestreamServer = new Swift::SOCKS5BytestreamServer(server, m_bytestreamRegistry);
	m_bytestreamServer->start();

	m_outgoingFTManager = new Swift::CombinedOutgoingFileTransferManager(m_jingleSessionManager, m_component->getIQRouter(),
																	m_userManager, m_remoteCandidateSelectorFactory, 
																	m_localCandidateGeneratorFactory, m_bytestreamRegistry, 
																	m_bytestreamProxy, m_component->getPresenceOracle(),
																	m_bytestreamServer);

// WARNING: Swiften crashes when this is uncommented... But we probably need it for working Jingle FT
// 	m_connectivityManager->addListeningPort(19645);
}

FileTransferManager::~FileTransferManager() {
	m_bytestreamServer->stop();
	delete m_outgoingFTManager;
	delete m_remoteCandidateSelectorFactory;
	delete m_localCandidateGeneratorFactory;
	delete m_jingleSessionManager;
	delete m_bytestreamRegistry;
	delete m_bytestreamServer;
	delete m_bytestreamProxy;
	delete m_connectivityManager;
}

FileTransferManager::Transfer FileTransferManager::sendFile(User *user, Buddy *buddy, boost::shared_ptr<Swift::ReadBytestream> byteStream, const Swift::StreamInitiationFileInfo &info) {
	FileTransferManager::Transfer transfer;
	transfer.from = buddy->getJID();
	transfer.to = user->getJID();
	transfer.readByteStream = byteStream;

	LOG4CXX_INFO(logger, "Starting FT from '" << transfer.from << "' to '" << transfer.to << "'")

	transfer.ft = m_outgoingFTManager->createOutgoingFileTransfer(transfer.from, transfer.to, transfer.readByteStream, info);
// 	if (transfer.ft) {
// 		m_filetransfers.push_back(ft);
// 		ft->onStateChange.connect(boost::bind(&User::handleFTStateChanged, this, _1, Buddy::JIDToLegacyName(from), info.getName(), info.getSize(), id));
// 		transfer.ft->start();
// 	}
	return transfer;
}

}
