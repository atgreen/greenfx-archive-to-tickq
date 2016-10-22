// --------------------------------------------------------------------------
//  _____                    ________   __
// |  __ \                   |  ___\ \ / /
// | |  \/_ __ ___  ___ _ __ | |_   \ V /          Open Source Tools for
// | | __| '__/ _ \/ _ \ '_ \|  _|  /   \            Automated Algorithmic
// | |_\ \ | |  __/  __/ | | | |   / /^\ \             Currency Trading
//  \____/_|  \___|\___|_| |_\_|   \/   \/
//
// --------------------------------------------------------------------------

// Copyright (C) 2014, 2016 Anthony Green <green@spindazzle.org>
// Distrubuted under the terms of the GPL v3 or later.

// This progam reads json from an archive file and pushes it to an
// ActiveMQ message broker.

#include <cstdlib>
#include <memory>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include <activemq/library/ActiveMQCPP.h>
#include <activemq/core/ActiveMQConnectionFactory.h>

using namespace activemq;
using namespace cms;
using namespace std;

#define AMQ_URL "tcp://broker-amq-tcp:61616?wireFormat=openwire"

static char *archive;

static Session *session;
static MessageProducer *producer;

struct MemoryStruct {
  char *memory;
  size_t size;
};

static void fatal (const char *msg)
{
  fprintf (stderr, "%s\n", msg);
  exit (1);
}

static size_t httpCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  struct MemoryStruct *mem = (struct MemoryStruct *) userp;
  char *ptr;
  static bool first_connect = true;
  
  if (first_connect)
    {
      std::cout << "SUCCESS" << std::endl;
      first_connect = false;
    }
  
  // Allocate new space for our buffer...
  mem->memory = ptr = (char *) realloc (mem->memory, mem->size + (size * nmemb));
  if (ptr == NULL)
    fatal ("Out of memory");
  
  // Copy the new data to the end of our buffer...
  memcpy (&(ptr[mem->size - 1]), contents, size * nmemb);
  mem->size += size * nmemb;
  ptr[mem->size - 1] = 0;

  do
    {
      // Scan for json delimiters
      int start = 0, brace = 0, end;
      while (ptr[start] && ptr[start] != '{') start++;
      if (ptr[start] == 0)
	return size * nmemb;;
      end = start;
      do {
	switch (ptr[end])
	  {
	  case 0: // incomplete. wait for more.
	    return size * nmemb;
	  case '{': brace++; break;
	  case '}': brace--; break;
	  default: break;
	  }
	end++;
      } while (brace != 0);

      // Temporatily NULL terminate the substring, and parse the json.
      char oldc = ptr[end];
      ptr[end] = 0;
      json_object *jobj = json_tokener_parse (&ptr[start]);

      if (json_object_object_get_ex (jobj, "tick", NULL))
	{
	  std::auto_ptr<TextMessage> message(session->createTextMessage(ptr));
	  producer->send(message.get());
	}
      else
	{
	  if (! json_object_object_get_ex (jobj, "heartbeat", NULL))
	    fprintf (stderr, "Unrecognized data from OANDA: %s\n", &ptr[start]);
	}

      json_object_put (jobj);
  
      // Restore the character we temporarily NULLd
      ptr[end] = oldc;

      // Copy the rest back to the beginning and try again.
      memmove (mem->memory,
	       &ptr[end], 
	       mem->size - end);
      mem->size -= end;
    } while (mem->size > 0);
  
  return size * nmemb;
}

char *getenv_checked (const char *e)
{
  char *v = getenv (e);
  if (!v)
    {
      fprintf (stderr, "ERROR: environment variable '%s' not set.\n", e);
      exit (1);
    }

  return v;
}

void config()
{
  archive = getenv_checked ("GREENFX_ARCHIVE");
}

int main(void)
{
  CURL *curl_handle;
  CURLcode res;
  char authHeader[100];
  char url[100];
  struct curl_slist *chunk = NULL;

  Connection *connection;
  Destination *destination;

  std::cout << GFX_VERSION_STR " Copyright (C) 2014, 2016  Anthony Green\n" << std::endl;

  config();
  
  printf ("Program started by User %d\n", getuid());

  activemq::library::ActiveMQCPP::initializeLibrary();

  std::cout << "Connecting to " AMQ_URL " : ";

  try {
      
    // Create a ConnectionFactory
    std::auto_ptr<ConnectionFactory> 
      connectionFactory(ConnectionFactory::createCMSConnectionFactory(AMQ_URL));

    // Create a Connection
    connection = connectionFactory->createConnection(getenv_checked ("AMQ_USER"),
						     getenv_checked ("AMQ_PASSWORD"));
    connection->start();

    session = connection->createSession(Session::AUTO_ACKNOWLEDGE);
    destination = session->createTopic("OANDA.TICK");

    producer = session->createProducer(destination);
    producer->setDeliveryMode(DeliveryMode::NON_PERSISTENT);

  } catch (CMSException& e) {

    std::cout << "FAILED" << std::endl;
    // This seems to always print "Success".
    // std::cout << e.getMessage() << std::endl;
    exit (1);
  }

  std::cout << "SUCCESS\nReading from " << archive << " : ";

  

  printf ("Program ended\n");

  return 0;
}
