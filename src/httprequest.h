#pragma once

// Transport includes
#include "transport/config.h"
#include "transport/networkplugin.h"
#include "transport/logging.h"

// Swiften
#include "Swiften/Swiften.h"
#include "Swiften/TLS/OpenSSL/OpenSSLContextFactory.h"

// Boost
#include <boost/algorithm/string.hpp>

using namespace boost::filesystem;
using namespace boost::program_options;

namespace  Transport {

	class HTTPRequest {
	public:
		HTTPRequest(Swift::BoostIOServiceThread *ioSerice, Swift::ConnectionFactory *factory);
		virtual ~HTTPRequest();

		bool fetchURL(const std::string &url);
		bool postData(const std::string &url, const std::string &contentType, const std::string &data);

		boost::signal<void (const std::string &data)> onResponseReceived;		

	private:
		void _fetchCallback(boost::shared_ptr<Swift::Connection> conn, const std::string url, bool error);
		void _disconnected(boost::shared_ptr<Swift::Connection> conn);
		void _read(boost::shared_ptr<Swift::Connection> conn, boost::shared_ptr<Swift::SafeByteArray> data);
		void _postCallback(boost::shared_ptr<Swift::Connection> conn, const std::string url, const std::string contentType, const std::string data, bool error);
		
		Swift::BoostIOServiceThread *m_ioService;
		Swift::ConnectionFactory *m_factory;
		std::string m_buffer;
		bool m_afterHeader;
	};
}
