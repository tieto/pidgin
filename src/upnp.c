/**
 * @file upnp.c UPnP Implementation
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "internal.h"
#include "gtkgaim.h"

#include "debug.h"
#include "util.h"
#include "proxy.h"
#include "xmlnode.h"
#include "network.h"
#include "eventloop.h"
#include "upnp.h"


/**
 * Information on the httpResponse callback
 */
typedef struct
{
  guint inpa;   /* gaim_input_add handle */
  guint tima;         /* gaim_timout_add handle */
  gchar* sendBuffer; /* send data */
  gchar* recvBuffer; /* response data */
  guint totalSizeRecv;
  gboolean done;

} NetResponseData;


/***************************************************************
** General Defines                                             *
****************************************************************/
#define HTTP_OK "200 OK"
#define DEFAULT_HTTP_PORT 80
#define MAX_PORT_SIZE 6
#define SIZEOF_HTTP 7         /* size of "http://" string */
#define RECEIVE_TIMEOUT 10000
#define CONSECUTIVE_RECEIVE_TIMEOUT 500
#define DISCOVERY_TIMEOUT 1000


/***************************************************************
** Discovery/Description Defines                               *
****************************************************************/
#define NUM_UDP_ATTEMPTS 2

/* Address and port of an SSDP request used for discovery */
#define HTTPMU_HOST_ADDRESS "239.255.255.250"
#define HTTPMU_HOST_PORT 1900

#define SEARCH_REQUEST_DEVICE "urn:schemas-upnp-org:service:"      \
                              "%s"

#define SEARCH_REQUEST_STRING "M-SEARCH * HTTP/1.1\r\n"            \
                              "MX: 2\r\n"                          \
                              "HOST: 239.255.255.250:1900\r\n"     \
                              "MAN: \"ssdp:discover\"\r\n"         \
                              "ST: urn:schemas-upnp-org:service:"  \
                              "%s\r\n"                             \
                              "\r\n"

#define MAX_DISCOVERY_RECEIVE_SIZE 400
#define MAX_DESCRIPTION_RECEIVE_SIZE 7000
#define MAX_DESCRIPTION_HTTP_HEADER_SIZE 100


/******************************************************************
** Action Defines                                                 *
*******************************************************************/
#define HTTP_HEADER_ACTION "POST /%s HTTP/1.1\r\n"                         \
                           "HOST: %s\r\n"                                  \
                           "SOAPACTION: "                                  \
                           "\"urn:schemas-upnp-org:"                       \
                           "service:%s#%s\"\r\n"                           \
                           "CONTENT-TYPE: text/xml ; charset=\"utf-8\"\r\n"\
                           "Content-Length: %i\r\n\r\n"

#define SOAP_ACTION  "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"     \
                     "<s:Envelope xmlns:s="                               \
                     "\"http://schemas.xmlsoap.org/soap/envelope/\" "     \
                     "s:encodingStyle="                                   \
                     "\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n" \
                     "<s:Body>\r\n"                                       \
                     "<u:%s xmlns:u="                                     \
                     "\"urn:schemas-upnp-org:service:%s\">\r\n%s"         \
                     "</u:%s>\r\n"                                        \
                     "</s:Body>\r\n"                                      \
                     "</s:Envelope>\r\n"

#define PORT_MAPPING_LEASE_TIME "0"
#define PORT_MAPPING_DESCRIPTION "GAIM_UPNP_PORT_FORWARD"

#define ADD_PORT_MAPPING_PARAMS "<NewRemoteHost></NewRemoteHost>\r\n"      \
                                "<NewExternalPort>%i</NewExternalPort>\r\n"\
                                "<NewProtocol>%s</NewProtocol>\r\n"        \
                                "<NewInternalPort>%i</NewInternalPort>\r\n"\
                                "<NewInternalClient>%s"                    \
                                "</NewInternalClient>\r\n"                 \
                                "<NewEnabled>1</NewEnabled>\r\n"           \
                                "<NewPortMappingDescription>"              \
                                PORT_MAPPING_DESCRIPTION                   \
                                "</NewPortMappingDescription>\r\n"         \
                                "<NewLeaseDuration>"                       \
                                PORT_MAPPING_LEASE_TIME                    \
                                "</NewLeaseDuration>\r\n"

#define DELETE_PORT_MAPPING_PARAMS "<NewRemoteHost></NewRemoteHost>\r\n" \
                                   "<NewExternalPort>%i"                 \
                                   "</NewExternalPort>\r\n"              \
                                   "<NewProtocol>%s</NewProtocol>\r\n"


static void
gaim_upnp_timeout(gpointer data,
                  gint source,
                  GaimInputCondition cond)
{
  NetResponseData* nrd = data;

  gaim_input_remove(nrd->inpa);
  gaim_timeout_remove(nrd->tima);

  if(nrd->totalSizeRecv == 0 && nrd->recvBuffer != NULL) {
    g_free(nrd->recvBuffer);
    nrd->recvBuffer = NULL;
  } else if(nrd->recvBuffer != NULL) {
    nrd->recvBuffer[nrd->totalSizeRecv] = '\0';
  }

  nrd->done = TRUE;
}


static void
gaim_upnp_http_read(gpointer data,
                    gint sock,
                    GaimInputCondition cond)
{
  int sizeRecv;
  extern int errno;
  NetResponseData* nrd = data;

  sizeRecv = recv(sock, &(nrd->recvBuffer[nrd->totalSizeRecv]),
                 MAX_DESCRIPTION_RECEIVE_SIZE-nrd->totalSizeRecv, 0);
  if(sizeRecv < 0 && errno != EINTR) {
    gaim_debug_error("upnp",
            "gaim_upnp_http_read(): recv < 0: %i!\n\n", errno);
    g_free(nrd->recvBuffer);
    nrd->recvBuffer = NULL;
    gaim_timeout_remove(nrd->tima);
    gaim_input_remove(nrd->inpa);
    nrd->done = TRUE;
    return;
  }else if(errno == EINTR) {
    sizeRecv = 0;
  }
  nrd->totalSizeRecv += sizeRecv;

  if(sizeRecv == 0) {
    gaim_timeout_remove(nrd->tima);
    gaim_input_remove(nrd->inpa);
    if(nrd->totalSizeRecv == 0) {
      gaim_debug_error("upnp",
          "gaim_upnp_http_read(): totalSizeRecv == 0\n\n");
      g_free(nrd->recvBuffer);
      nrd->recvBuffer = NULL;
    } else {
      nrd->recvBuffer[nrd->totalSizeRecv] = '\0';
    }
    nrd->done = TRUE;
  } else {
    gaim_timeout_remove(nrd->tima);
    gaim_input_remove(nrd->inpa);
    nrd->tima = gaim_timeout_add(CONSECUTIVE_RECEIVE_TIMEOUT,
                                (GSourceFunc)gaim_upnp_timeout, nrd);
    nrd->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                              gaim_upnp_http_read, nrd);
  }
}

static void 
gaim_upnp_http_send(gpointer data,
                    gint sock,
                    GaimInputCondition cond)
{
  int sizeSent, totalSizeSent = 0;
  extern int errno;
  NetResponseData* nrd = data;

  gaim_timeout_remove(nrd->tima);
  while(totalSizeSent < strlen(nrd->sendBuffer)) {
    sizeSent = send(sock,(gchar*)((int)nrd->sendBuffer+totalSizeSent),
                      strlen(nrd->sendBuffer)-totalSizeSent,0);
    if(sizeSent <= 0 && errno != EINTR) {
      gaim_debug_error("upnp",
            "gaim_upnp_http_request(): Failed In send\n\n");
      nrd->done = TRUE;
      g_free(nrd->recvBuffer);
      nrd->recvBuffer = NULL;
      close(sock);
      return;
    }else if(errno == EINTR) {
      sizeSent = 0;
    }
    totalSizeSent += sizeSent;
  }

  nrd->tima = gaim_timeout_add(RECEIVE_TIMEOUT,
                                (GSourceFunc)gaim_upnp_timeout, nrd);
  nrd->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                              gaim_upnp_http_read, nrd);
  while (!nrd->done) {
    gtk_main_iteration();
  }
  close(sock);
}

static gchar*
gaim_upnp_http_request(const gchar* address,
                       int port,
                       gchar* httpRequest)
{
  gchar* recvBuffer;
  NetResponseData* nrd = (NetResponseData*)g_malloc0(sizeof(NetResponseData));
  nrd->sendBuffer = httpRequest;
  nrd->recvBuffer = (gchar*)g_malloc(MAX_DESCRIPTION_RECEIVE_SIZE);

  nrd->tima = gaim_timeout_add(RECEIVE_TIMEOUT,
                               (GSourceFunc)gaim_upnp_timeout, nrd);

  if(gaim_proxy_connect(NULL, address, port, 
      gaim_upnp_http_send, nrd)) {

    gaim_debug_error("upnp", "Connect Failed: Address: %s @@@ Port %d @@@ Request %s\n\n", 
                    address, port, nrd->sendBuffer);

    gaim_timeout_remove(nrd->tima);
    g_free(nrd->recvBuffer);
    nrd->recvBuffer = NULL;
  } else {
    while (!nrd->done) {
      gtk_main_iteration();
    }
  }

  recvBuffer = nrd->recvBuffer;
  g_free(nrd);

  return recvBuffer;
}



static gboolean
gaim_upnp_compare_device(const xmlnode* device,
                         const gchar* deviceType)
{
  xmlnode* deviceTypeNode = xmlnode_get_child(device, "deviceType");
  if(deviceTypeNode == NULL) {
    return FALSE;
  }
  return !g_ascii_strcasecmp(xmlnode_get_data(deviceTypeNode), deviceType);
}


static gboolean
gaim_upnp_compare_service(const xmlnode* service,
                          const gchar* serviceType)
{
  xmlnode* serviceTypeNode = xmlnode_get_child(service, "serviceType");
  if(serviceTypeNode == NULL) {
    return FALSE;
  }
  return !g_ascii_strcasecmp(xmlnode_get_data(serviceTypeNode), serviceType);
}



static gchar*
gaim_upnp_parse_description_response(const gchar* httpResponse, 
                                     const gchar* httpURL,
                                     const gchar* serviceType)
{
  gchar* xmlRoot;
  gchar* baseURL;
  gchar* controlURL;
  gchar* service;
  xmlnode* xmlRootNode;
  xmlnode* serviceTypeNode;
  xmlnode* controlURLNode;
  xmlnode* baseURLNode;

  /* make sure we have a valid http response */
  if(g_strstr_len(httpResponse, strlen(httpResponse), HTTP_OK) == NULL) {
    gaim_debug_error("upnp",
          "parse_description_response(): Failed In HTTP_OK\n\n");
    return NULL;
  }

  /* find the root of the xml document */
  if((xmlRoot = g_strstr_len(httpResponse, strlen(httpResponse), 
                            "<root")) == NULL) {
    gaim_debug_error("upnp",
          "parse_description_response(): Failed finding root\n\n");
    return NULL;
  }

  /* create the xml root node */
  if((xmlRootNode = xmlnode_from_str(xmlRoot, -1)) == NULL) {
    gaim_debug_error("upnp",
          "parse_description_response(): Could not parse xml root node\n\n");
    return NULL;
  }

  /* get the baseURL of the device */
  if((baseURLNode = xmlnode_get_child(xmlRootNode,
                                      "URLBase")) != NULL) {
    baseURL = g_strdup(xmlnode_get_data(baseURLNode));
  } else {
    baseURL = g_strdup(httpURL);
  }

  /* get the serviceType child that has the service type as it's data */

  /* get urn:schemas-upnp-org:device:InternetGatewayDevice:1 
     and it's devicelist */
  serviceTypeNode = xmlnode_get_child(xmlRootNode, "device");
  while(!gaim_upnp_compare_device(serviceTypeNode,
        "urn:schemas-upnp-org:device:InternetGatewayDevice:1") &&
        serviceTypeNode != NULL) {
    serviceTypeNode = xmlnode_get_next_twin(serviceTypeNode);
  }
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 1\n\n");
    return NULL;
  }
  serviceTypeNode = xmlnode_get_child(serviceTypeNode, "deviceList");
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 2\n\n");
    return NULL;
  }

  /* get urn:schemas-upnp-org:device:WANDevice:1 
     and it's devicelist */
  serviceTypeNode = xmlnode_get_child(serviceTypeNode, "device");
  while(!gaim_upnp_compare_device(serviceTypeNode,
        "urn:schemas-upnp-org:device:WANDevice:1") &&
        serviceTypeNode != NULL) {
    serviceTypeNode = xmlnode_get_next_twin(serviceTypeNode);
  }
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 3\n\n");
    return NULL;
  }
  serviceTypeNode = xmlnode_get_child(serviceTypeNode, "deviceList");
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 4\n\n");
    return NULL;
  }

  /* get urn:schemas-upnp-org:device:WANConnectionDevice:1 
     and it's servicelist */
  serviceTypeNode = xmlnode_get_child(serviceTypeNode, "device");
  while(!gaim_upnp_compare_device(serviceTypeNode,
        "urn:schemas-upnp-org:device:WANConnectionDevice:1") &&
        serviceTypeNode != NULL) {
    serviceTypeNode = xmlnode_get_next_twin(serviceTypeNode);
  }
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 5\n\n");
    return NULL;
  }
  serviceTypeNode = xmlnode_get_child(serviceTypeNode, "serviceList");
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 6\n\n");
    return NULL;
  }

  /* get the serviceType variable passed to this function */
  service = g_strdup_printf(SEARCH_REQUEST_DEVICE, serviceType);
  serviceTypeNode = xmlnode_get_child(serviceTypeNode, "service");
  while(!gaim_upnp_compare_service(serviceTypeNode, service) &&
        serviceTypeNode != NULL) {
    serviceTypeNode = xmlnode_get_next_twin(serviceTypeNode);
  }

  g_free(service);
  if(serviceTypeNode == NULL) {
    gaim_debug_error("upnp",
      "parse_description_response(): could not get serviceTypeNode 7\n\n");
    return NULL;
  }

  /* get the controlURL of the service */
  if((controlURLNode = xmlnode_get_child(serviceTypeNode, 
                                         "controlURL")) == NULL) {
    gaim_debug_error("upnp",
         "parse_description_response(): Could not find controlURL\n\n");
    return NULL;
  }

  if(g_strstr_len(xmlnode_get_data(controlURLNode), 
                  SIZEOF_HTTP, "http://") == NULL &&
     g_strstr_len(xmlnode_get_data(controlURLNode), 
                  SIZEOF_HTTP, "HTTP://") == NULL) {
    controlURL = g_strdup_printf("%s%s", baseURL, 
                                 xmlnode_get_data(controlURLNode));
  }else{
    controlURL = g_strdup(xmlnode_get_data(controlURLNode));
  }
  g_free(baseURL);
  xmlnode_free(xmlRootNode);

  return controlURL;
}


static gchar*
gaim_upnp_parse_description(const gchar* descriptionURL,
                            const gchar* serviceType)
{
  gchar* fullURL;
  gchar* controlURL;
  gchar* httpResponse;
  gchar* httpRequest;

  gchar* descriptionXMLAddress;
  gchar* descriptionAddressPort;
  gchar* descriptionAddress;
  gchar descriptionPort[MAX_PORT_SIZE];
  int port = 0;

  /* parse the 4 above variables out of the descriptionURL 
     example description URL: http://192.168.1.1:5678/rootDesc.xml */

  /* parse the url into address, port, path variables */
  if(!gaim_url_parse(descriptionURL, &descriptionAddress,
                          &port, &descriptionXMLAddress, NULL, NULL)) {
    return NULL;
  }
  if(port == 0 || port == -1) {
    port = DEFAULT_HTTP_PORT;
  }
  g_ascii_dtostr(descriptionPort, MAX_PORT_SIZE, port);
  descriptionAddressPort = g_strdup_printf("%s:%s", descriptionAddress, 
                                           descriptionPort);

  fullURL = g_strdup_printf("http://%s", descriptionAddressPort);

  /* for example...
     GET /rootDesc.xml HTTP/1.1\r\nHost: 192.168.1.1:5678\r\n\r\n */
  httpRequest = g_strdup_printf("GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n",
                    descriptionXMLAddress, descriptionAddressPort);

  httpResponse = gaim_upnp_http_request(descriptionAddress,
                                        port, httpRequest);
  if(httpResponse == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_parse_description(): httpResponse is NULL\n\n");
    g_free(descriptionXMLAddress);
    g_free(descriptionAddress);
    g_free(descriptionAddressPort);
    g_free(httpRequest);
    g_free(fullURL);
    return NULL;
  }

  controlURL = gaim_upnp_parse_description_response(httpResponse, 
                                            fullURL, serviceType);

  g_free(descriptionXMLAddress);
  g_free(descriptionAddress);
  g_free(descriptionAddressPort);
  g_free(fullURL);
  g_free(httpRequest);
  g_free(httpResponse);

  if(controlURL == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_parse_description(): controlURL is NULL\n\n");
    return NULL;
  }

  return controlURL;
}


static gchar*
gaim_upnp_parse_discover_response(const gchar* buf,
                                  unsigned int bufSize,
                                  const gchar* serviceType)
{
  gchar* startDescURL;
  gchar* endDescURL;
  gchar* descURL;
  gchar* retVal;

  if(g_strstr_len(buf, strlen(buf), HTTP_OK) == NULL) {
    gaim_debug_error("upnp",
          "parse_discover_response(): Failed In HTTP_OK\n\n");
    return NULL;
  }

  if((startDescURL = g_strstr_len(buf, strlen(buf), "http://")) == NULL) {
    gaim_debug_error("upnp",
          "parse_discover_response(): Failed In finding http://\n\n");
    return NULL;
  }

  endDescURL = g_strstr_len(startDescURL, strlen(startDescURL), "\r");
  if(endDescURL == NULL) {
    endDescURL = g_strstr_len(startDescURL, strlen(startDescURL), "\n");
    if(endDescURL == NULL) {
      gaim_debug_error("upnp",
            "parse_discover_response(): Failed In endDescURL\n\n");
      return NULL;
    }else if(endDescURL == startDescURL) {
      gaim_debug_error("upnp",
            "parse_discover_response(): endDescURL == startDescURL\n\n");
      return NULL;
    }
  }else if(endDescURL == startDescURL) {
    gaim_debug_error("upnp",
        "parse_discover_response(): 2nd endDescURL == startDescURL\n\n");
    return NULL;
  }
  descURL = g_strndup(startDescURL, endDescURL-startDescURL);

  retVal = gaim_upnp_parse_description(descURL, serviceType);
  g_free(descURL);
  return retVal;
}



static void
gaim_upnp_discover_udp_read(gpointer data,
                            gint sock,
                            GaimInputCondition cond)
{
  unsigned int length;
  extern int errno;
  struct sockaddr_in from;
  int sizeRecv;
  NetResponseData* nrd = data;

  gaim_timeout_remove(nrd->tima);
  length = sizeof(struct sockaddr_in);

  do {
    sizeRecv = recvfrom(sock, nrd->recvBuffer,
                        MAX_DISCOVERY_RECEIVE_SIZE, 0,
                        (struct sockaddr*)&from, &length);

    if(sizeRecv > 0) {
      nrd->recvBuffer[sizeRecv] = '\0';
    }else if(errno != EINTR) {
      g_free(nrd->recvBuffer);
      nrd->recvBuffer = NULL;
    }
  }while(errno == EINTR);

  gaim_input_remove(nrd->inpa);
  nrd->done = TRUE;
  return;
}



GaimUPnPControlInfo*
gaim_upnp_discover(void)
{
  /* Socket Setup Variables */
  int sock, i;
  extern int errno;
  struct sockaddr_in server;
  struct hostent* hp;

  /* UDP SEND VARIABLES */
  gboolean sentSuccess, recvSuccess;
  int sizeSent, totalSizeSent;
  gchar wanIP[] = "WANIPConnection:1";
  gchar wanPPP[] = "WANPPPConnection:1";
  gchar* serviceToUse;
  gchar* sendMessage = NULL;
  
  /* UDP RECEIVE VARIABLES */
  GaimUPnPControlInfo* controlInfo = g_malloc(sizeof(GaimUPnPControlInfo));
  NetResponseData* nrd = g_malloc(sizeof(NetResponseData));
  
  /* Set up the sockets */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    close(sock);
    gaim_debug_error("upnp",
          "gaim_upnp_discover(): Failed In sock creation\n\n");
    g_free(nrd);
    g_free(controlInfo);
    return NULL;
  }
  memset(&server, 0, sizeof(struct sockaddr));
  server.sin_family = AF_INET;
  if((hp = gethostbyname(HTTPMU_HOST_ADDRESS)) == NULL) {
    close(sock);
    gaim_debug_error("upnp",
          "gaim_upnp_discover(): Failed In gethostbyname\n\n");
    g_free(nrd);
    g_free(controlInfo);
    return NULL;
  }
  memcpy(&server.sin_addr,
        hp->h_addr_list[0],
        hp->h_length);
  server.sin_port = htons(HTTPMU_HOST_PORT);

  /* because we are sending over UDP, if there is a failure
     we should retry the send NUM_UDP_ATTEMPTS times. Also,
     try different requests for WANIPConnection and 
     WANPPPConnection*/
  for(i = 0; i < NUM_UDP_ATTEMPTS; i++) {
    sentSuccess = TRUE;
    recvSuccess = TRUE;
    totalSizeSent = 0;

    nrd->recvBuffer = NULL;
    nrd->totalSizeRecv = 0;
    nrd->done = FALSE;

    if(sendMessage != NULL) {
      g_free(sendMessage);
    }

    if(i%2 == 0) {
      serviceToUse = wanIP;
    } else {
      serviceToUse = wanPPP;
    }
    sendMessage = g_strdup_printf(SEARCH_REQUEST_STRING, serviceToUse);

    nrd->recvBuffer = (char*)g_malloc(MAX_DISCOVERY_RECEIVE_SIZE);

    while(totalSizeSent < strlen(sendMessage)) {
      sizeSent = sendto(sock,(void*)&sendMessage[totalSizeSent],
                        strlen(&sendMessage[totalSizeSent]),0,
                        (struct sockaddr*)&server,
                        sizeof(struct sockaddr_in));
      if(sizeSent <= 0 && errno != EINTR) {
        sentSuccess = FALSE;
        break;
      }else if(errno == EINTR) {
        sizeSent = 0;
      }
      totalSizeSent += sizeSent;
    }

    if(sentSuccess) {
      nrd->tima = gaim_timeout_add(DISCOVERY_TIMEOUT, 
                                  (GSourceFunc)gaim_upnp_timeout, nrd);
      nrd->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                                 gaim_upnp_discover_udp_read, nrd);
      while (!nrd->done) {
        gtk_main_iteration();
      }
      if(nrd->recvBuffer == NULL) {
        recvSuccess = FALSE;
      } else {
        /* parse the response, and see if it was a success */
        close(sock);
        if((controlInfo->controlURL=
            gaim_upnp_parse_discover_response(nrd->recvBuffer,
                                         strlen(nrd->recvBuffer),
                                         serviceToUse))==NULL) {
          gaim_debug_error("upnp",
                       "gaim_upnp_discover(): Failed In parse response\n\n");
          g_free(nrd->recvBuffer);
          g_free(nrd);
          g_free(controlInfo);
          return NULL;
        }

        controlInfo->serviceType = g_strdup(serviceToUse);
      }
    }

    /* if sent success and recv successful, then break */
    if(sentSuccess && recvSuccess) {
      i = NUM_UDP_ATTEMPTS;
    }
  }

  if(nrd->recvBuffer != NULL) {
    g_free(nrd->recvBuffer);
  }
  g_free(sendMessage);
  g_free(nrd);

  if(!sentSuccess || !recvSuccess) {
    close(sock);
    gaim_debug_error("upnp",
          "gaim_upnp_discover(): Failed In sent/recv success\n\n");
    g_free(controlInfo);
    return NULL;
  }

  return controlInfo;
}


static char*
gaim_upnp_generate_action_message_and_send(const GaimUPnPControlInfo* controlInfo,
                                          const gchar* actionName, 
                                          const gchar* actionParams)
{
  gchar* actionMessage;
  gchar* soapMessage;
  gchar* totalSendMessage;
  gchar* httpResponse;

  gchar* pathOfControl;
  gchar* addressOfControl;
  gchar* addressPortOfControl;
  gchar portOfControl[MAX_PORT_SIZE];
  int port=0;

  /* set the soap message */
  soapMessage = g_strdup_printf(SOAP_ACTION, actionName,
                                controlInfo->serviceType,
                                actionParams, actionName);

  /* parse the url into address, port, path variables */
  if(!gaim_url_parse(controlInfo->controlURL, &addressOfControl,
                     &port, &pathOfControl, NULL, NULL)) {
    gaim_debug_error("upnp",
          "generate_action_message_and_send(): Failed In Parse URL\n\n");
    g_free(soapMessage);
    return NULL;
  }
  if(port == 0 || port == -1) {
    port = DEFAULT_HTTP_PORT;
  }
  g_ascii_dtostr(portOfControl, MAX_PORT_SIZE, port);

  /* set the addressPortOfControl variable which should have a
     form like the following: 192.168.1.1:8000 */
  addressPortOfControl = g_strdup_printf("%s:%s",
                                         addressOfControl, portOfControl);

  /* set the HTTP Header */
  actionMessage = g_strdup_printf(HTTP_HEADER_ACTION,
                  pathOfControl, addressPortOfControl,
                  controlInfo->serviceType, actionName, 
                  strlen(soapMessage));

  /* append to the header the body */
  totalSendMessage = g_strdup_printf("%s%s", actionMessage, soapMessage);

  /* get the return of the http response */
  httpResponse = gaim_upnp_http_request(addressOfControl, 
                                        port, totalSendMessage);
  if(httpResponse == NULL) {
    gaim_debug_error("upnp",
          "generate_action_message_and_send(): Failed In httpResponse\n\n");
  }

  g_free(actionMessage);
  g_free(soapMessage);
  g_free(totalSendMessage);
  g_free(pathOfControl);
  g_free(addressOfControl);
  g_free(addressPortOfControl);

  return httpResponse;
}




gchar*
gaim_upnp_get_public_ip(const GaimUPnPControlInfo* controlInfo)
{
  gchar* extIPAddress;
  gchar* httpResponse;
  gchar actionName[] = "GetExternalIPAddress";
  gchar actionParams[] = "";
  gchar* temp, *temp2;

  httpResponse = gaim_upnp_generate_action_message_and_send(controlInfo,
                                                            actionName, 
                                                            actionParams);
  if(httpResponse == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_get_public_ip(): Failed In httpResponse\n\n");
    return NULL;
  }

  /* extract the ip, or see if there is an error */
  if((temp = g_strstr_len(httpResponse, strlen(httpResponse),
                              "<NewExternalIPAddress")) == NULL) {
    gaim_debug_error("upnp",
      "gaim_upnp_get_public_ip(): Failed Finding <NewExternalIPAddress\n\n");
    g_free(httpResponse);
    return NULL;
  }
  if((temp = g_strstr_len(temp, strlen(temp), ">")) == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_get_public_ip(): Failed In Finding >\n\n");
    g_free(httpResponse);
    return NULL;
  }
  if((temp2 = g_strstr_len(temp, strlen(temp), "<")) == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_get_public_ip(): Failed In Finding <\n\n");
    g_free(httpResponse);
    return NULL;
  }

  extIPAddress = g_strndup(&temp[1], (temp2-1)-temp);

  g_free(httpResponse);

  gaim_debug_info("upnp", "NAT Returned IP: %s\n", extIPAddress);
  return extIPAddress;
}

static void
gaim_upnp_get_local_system_ip(gpointer data,
                              gint sock,
                              GaimInputCondition cond)
{
  NetResponseData* nrd = data;
  nrd->recvBuffer = g_strdup(gaim_network_get_local_system_ip(sock));

  gaim_timeout_remove(nrd->tima);
  nrd->done = TRUE;

  close(sock);
}

static gchar*
gaim_upnp_get_local_ip_address(const gchar* address) 
{
  gchar* ip;
  gchar* pathOfControl;
  gchar* addressOfControl;
  int port = 0;
  NetResponseData* nrd = (NetResponseData*)g_malloc0(sizeof(NetResponseData));
  
  if(!gaim_url_parse(address, &addressOfControl,
                     &port, &pathOfControl, NULL, NULL)) {
    gaim_debug_error("upnp",
          "get_local_ip_address(): Failed In Parse URL\n\n");
    return NULL;
  }
  if(port == 0 || port == -1) {
    port = DEFAULT_HTTP_PORT;
  }

  nrd->tima = gaim_timeout_add(RECEIVE_TIMEOUT,
                               (GSourceFunc)gaim_upnp_timeout, nrd);

  if(gaim_proxy_connect(NULL, addressOfControl, port, 
      gaim_upnp_get_local_system_ip, nrd)) {

    gaim_debug_error("upnp", "Get Local IP Connect Failed: Address: %s @@@ Port %d @@@ Request %s\n\n", 
                    address, port, nrd->sendBuffer);

    gaim_timeout_remove(nrd->tima);
  } else {
    while (!nrd->done) {
      gtk_main_iteration();
    }
  }

  ip = nrd->recvBuffer;
  g_free(nrd);

  gaim_debug_info("upnp", "local ip: %s\n", ip);

  return ip;
}



gboolean
gaim_upnp_set_port_mapping(const GaimUPnPControlInfo* controlInfo, 
                           unsigned short portMap,
                           const gchar* protocol)
{
  gchar* httpResponse;
  gchar actionName[] = "AddPortMapping";
  gchar* actionParams;
  gchar* internalIP;

  /* get the internal IP */
  if((internalIP = gaim_upnp_get_local_ip_address(controlInfo->controlURL))
                                                                  == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_set_port_mapping(): couldn't get local ip\n\n");
    return FALSE;
  }

  /* make the portMappingParams variable */
  actionParams = g_strdup_printf(ADD_PORT_MAPPING_PARAMS, portMap, 
                                 protocol, portMap, internalIP);

  httpResponse = gaim_upnp_generate_action_message_and_send(controlInfo,
                                                            actionName, 
                                                            actionParams);
  if(httpResponse == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_set_port_mapping(): Failed In httpResponse\n\n");
    g_free(actionParams);
    g_free(internalIP);
    return FALSE;
  }

  /* determine if port mapping was a success */
  if(strstr(httpResponse, HTTP_OK) == NULL) {
    gaim_debug_error("upnp",
     "gaim_upnp_set_port_mapping(): Failed HTTP_OK\n\n%s\n\n", httpResponse);
    g_free(actionParams);
    g_free(httpResponse);
    g_free(internalIP);
    return FALSE;
  }

  g_free(actionParams);
  g_free(httpResponse);

  gaim_debug_info("upnp", "NAT Added Port Forward On Port: %d: To IP: %s\n", portMap, internalIP);
  g_free(internalIP);
  return TRUE;
}


gboolean
gaim_upnp_remove_port_mapping(const GaimUPnPControlInfo* controlInfo, 
                              unsigned short portMap,
                              const char* protocol)
{
  gchar* httpResponse;
  gchar actionName[] = "DeletePortMapping";
  gchar* actionParams;

  /* make the portMappingParams variable */
  actionParams = g_strdup_printf(DELETE_PORT_MAPPING_PARAMS,
                                 portMap, protocol);

  httpResponse = gaim_upnp_generate_action_message_and_send(controlInfo,
                                                            actionName, 
                                                            actionParams);

  if(httpResponse == NULL) {
    gaim_debug_error("upnp",
          "gaim_upnp_remove_port_mapping(): Failed In httpResponse\n\n");
    g_free(actionParams);
    return FALSE;
  }

  /* determine if port mapping was a success */
  if(strstr(httpResponse, HTTP_OK) == NULL) {
    gaim_debug_error("upnp",
     "gaim_upnp_set_port_mapping(): Failed HTTP_OK\n\n%s\n\n", httpResponse);
    g_free(actionParams);
    g_free(httpResponse);
    return FALSE;
  }

  g_free(actionParams);
  g_free(httpResponse);

  gaim_debug_info("upnp", "NAT Removed Port Forward On Port: %d\n", portMap);
  return TRUE;
}
