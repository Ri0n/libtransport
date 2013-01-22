#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "basictest.h"
#include "transport/httprequest.h"

#include <Swiften/Network/Connector.h>
#include <Swiften/Network/Connection.h>
#include <Swiften/Network/ConnectionFactory.h>
#include <Swiften/Network/HostAddressPort.h>
#include <Swiften/Network/StaticDomainNameResolver.h>
#include <Swiften/Network/DummyTimerFactory.h>
#include <Swiften/EventLoop/DummyEventLoop.h>
#include <Swiften/Network/DomainNameAddressQuery.h>

#include "transport/logging.h"

DEFINE_LOGGER(logger, "HTTPRequestTests");

using namespace Transport;
using namespace Swift;

class HTTPRequestTests : public CPPUNIT_NS :: TestFixture, public BasicTest {
	CPPUNIT_TEST_SUITE(HTTPRequestTests);
	CPPUNIT_TEST(handleGetRequest);
	CPPUNIT_TEST(handlePostRequest);
	CPPUNIT_TEST_SUITE_END();
public:
	void setUp (void) {
		setMeUp();	
		eventLoop = new DummyEventLoop();
		resolver = new StaticDomainNameResolver(eventLoop);
		connectionFactory = new MockConnectionFactory(eventLoop);
		timerFactory = new DummyTimerFactory();
		resolver->addAddress("google.com",  HostAddress("1.1.1.1"));
	}

	void tearDown (void) {
		received.clear();
		tearMeDown();
	}


	void handleGetRequest() {
		HTTPRequest *get = new HTTPRequest(resolver, connectionFactory, timerFactory);
		get->onResponseReceived.connect(boost::bind(&HTTPRequestTests::_responseGetCallback, this, _1));
		get->fetchURL("http://google.com");
		eventLoop->processEvents();

	}
	void handlePostRequest() {
	}

	void _responseGetCallback(const std::string &data) {
		LOG4CXX_INFO(logger, "Data: " << data);
	}	
	struct MockConnection : public Connection {
	public:
		MockConnection(const std::vector<HostAddressPort>& failingPorts, bool isResponsive, EventLoop* eventLoop) : eventLoop(eventLoop), failingPorts(failingPorts), isResponsive(isResponsive) {}

		void listen() { assert(false); }
		void connect(const HostAddressPort& address) {
			hostAddressPort = address;
			if (isResponsive) {
				bool fail = std::find(failingPorts.begin(), failingPorts.end(), address) != failingPorts.end();
				eventLoop->postEvent(boost::bind(boost::ref(onConnectFinished), fail));
			}
		}

		HostAddressPort getLocalAddress() const { return HostAddressPort(); }
		void disconnect() { assert(false); }
		void write(const SafeByteArray&) { assert(false); }

		EventLoop* eventLoop;
		boost::optional<HostAddressPort> hostAddressPort;
		std::vector<HostAddressPort> failingPorts;
		bool isResponsive;
	};

	struct MockConnectionFactory : public ConnectionFactory {
		MockConnectionFactory(EventLoop* eventLoop) : eventLoop(eventLoop), isResponsive(true) {
		}

		boost::shared_ptr<Connection> createConnection() {
			return boost::shared_ptr<Connection>(new MockConnection(failingPorts, isResponsive, eventLoop));
		}

		EventLoop* eventLoop;
		bool isResponsive;
		std::vector<HostAddressPort> failingPorts;
	};
		DummyEventLoop* eventLoop;
		StaticDomainNameResolver* resolver;
		MockConnectionFactory* connectionFactory;
		DummyTimerFactory* timerFactory;


};

CPPUNIT_TEST_SUITE_REGISTRATION (HTTPRequestTests);