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
	}

	void handleGetRequest() {
		HTTPRequest *get = new HTTPRequest(factories);
		get->onResponseReceived.connect(boost::bind(&HTTPRequestTests::_responseGetCallback, this, _1));
		get->fetchURL("http://google.com");
		
	}
	void handlePostRequest() {
	}

	void _responseGetCallback(const std::string &data) {
		LOG4CXX_INFO(logger, "Data: " << data);
	}	
};

CPPUNIT_TEST_SUITE_REGISTRATION (HTTPRequestTests);