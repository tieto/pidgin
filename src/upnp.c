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
  char* recvBuffer; /* response data */
  guint totalSizeRecv;
  gboolean done;

} HRD;


/***************************************************************
** General Defines                                             *
****************************************************************/
#define HTTP_OK "200 OK"
#define SIZEOF_HTTP 7         /* size of "http://" */
#define RECIEVE_TIMEOUT 10000
#define CONSECUTIVE_RECIEVE_TIMEOUT 500
#define DISCOVERY_TIMEOUT 1000


/***************************************************************
** Discovery/Description Defines                               *
****************************************************************/
#define NUM_UDP_ATTEMPTS 2

/* Address and port of an SSDP request used for discovery */
#define HTTPMU_HOST_ADDRESS "239.255.255.250"
#define HTTPMU_HOST_PORT 1900

#define SEARCH_REQUEST_DEVICE "urn:schemas-upnp-org:service:"      \
                              "WANIPConnection:1"

#define SEARCH_REQUEST_STRING "M-SEARCH * HTTP/1.1\r\n"            \
                              "MX: 2\r\n"                          \
                              "HOST: 239.255.255.250:1900\r\n"     \
                              "MAN: \"ssdp:discover\"\r\n"         \
                              "ST: urn:schemas-upnp-org:service:"  \
                              "WANIPConnection:1\r\n"              \
                              "\r\n"

#define MAX_DISCOVERY_RECIEVE_SIZE 400
#define MAX_DESCRIPTION_RECIEVE_SIZE 7000
#define MAX_DESCRIPTION_HTTP_HEADER_SIZE 100


/******************************************************************
** Action Defines                                                 *
*******************************************************************/
#define HTTP_HEADER_ACTION "POST %s HTTP/1.1\r\n"                          \
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



/* validate an http url without a port */
static gboolean
gaim_upnp_validate_url(const char* url)
{
  int i1, i2, i3, i4, r;

  if(url == NULL) {
    gaim_debug_info("upnp", 
          "gaim_upnp_validate_url(): url == NULL\n\n");
    return FALSE;
  }
  r = sscanf(url, "http://%3i.%3i.%3i.%3i/", &i1, &i2, &i3, &i4);
  if(r == 4) {
    return TRUE;
  }

  gaim_debug_info("upnp",
        "gaim_upnp_validate_url(): Failed In validate URL\n\n");
  return FALSE;
}


static void
gaim_upnp_timeout(gpointer data,
                  gint source, 
                  GaimInputCondition cond)
{
  HRD* hrd = data;

  gaim_input_remove(hrd->inpa);
  gaim_timeout_remove(hrd->tima);

  if(hrd->totalSizeRecv == 0) {
    free(hrd->recvBuffer);
    hrd->recvBuffer = NULL;
  } else {
    hrd->recvBuffer[hrd->totalSizeRecv] = '\0';
  }

  hrd->done = TRUE;
}


static void
gaim_upnp_http_read(gpointer data,
                    gint sock,
                    GaimInputCondition cond)
{
  int sizeRecv;
  extern int errno;
  HRD* hrd = data;

  sizeRecv = recv(sock, &(hrd->recvBuffer[hrd->totalSizeRecv]),
                 MAX_DESCRIPTION_RECIEVE_SIZE-hrd->totalSizeRecv, 0);
  if(sizeRecv < 0 && errno != EINTR) {
    gaim_debug_info("upnp",
            "gaim_upnp_http_read(): recv < 0: %i!\n\n", errno);
    free(hrd->recvBuffer);
    hrd->recvBuffer = NULL;
    gaim_timeout_remove(hrd->tima);
    gaim_input_remove(hrd->inpa);
    hrd->done = TRUE;
    return;
  }else if(errno == EINTR) {
    sizeRecv = 0;
  }
  hrd->totalSizeRecv += sizeRecv;

  if(sizeRecv == 0) {
    if(hrd->totalSizeRecv == 0) {
      gaim_debug_info("upnp",
          "gaim_upnp_http_read(): totalSizeRecv == 0\n\n");
      free(hrd->recvBuffer);
      hrd->recvBuffer = NULL;
    } else {
      hrd->recvBuffer[hrd->totalSizeRecv] = '\0';
    }
    gaim_timeout_remove(hrd->tima);
    gaim_input_remove(hrd->inpa);
    hrd->done = TRUE;
  } else {
    gaim_timeout_remove(hrd->tima);
    gaim_input_remove(hrd->inpa);
    hrd->tima = gaim_timeout_add(CONSECUTIVE_RECIEVE_TIMEOUT,
                                (GSourceFunc)gaim_upnp_timeout, hrd);
    hrd->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                              gaim_upnp_http_read, hrd);
  }

}



static char*
gaim_upnp_http_request(const char* address,
                       unsigned short port,
                       const char* httpRequest)
{
  int sock;
  int sizeSent, totalSizeSent = 0;
  extern int errno;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char* recvBuffer;

  HRD* hrd = (HRD*)malloc(sizeof(HRD));
  if(hrd == NULL) {
    gaim_debug_info("upnp",
      "gaim_upnp_http_request(): Failed in hrd MALLOC\n\n");
    return NULL;
  }

  hrd->recvBuffer = NULL;
  hrd->totalSizeRecv = 0;
  hrd->done = FALSE;

  hrd->recvBuffer = (char*)malloc(MAX_DESCRIPTION_RECIEVE_SIZE);
  if(hrd == NULL) {
    gaim_debug_info("upnp",
      "gaim_upnp_http_request(): Failed in recvBuffer MALLOC\n\n");
    free(hrd);
    return NULL;
  }

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    gaim_debug_info("upnp",
          "gaim_upnp_http_request(): Failed In sock creation\n\n");
    free(hrd->recvBuffer);
    free(hrd);
    return NULL;
  }

  server = gethostbyname(address);
  if(server == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_http_request(): Failed In gethostbyname\n\n");
    free(hrd->recvBuffer);
    free(hrd);
    close(sock);
    return NULL;
  }
  memset((char*)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr,
        server->h_addr_list[0],
        server->h_length);
  serv_addr.sin_port = htons(port);

  if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
    gaim_debug_info("upnp",
          "gaim_upnp_http_request(): Failed In connect\n\n");
    free(hrd->recvBuffer);
    free(hrd);
    close(sock);
    return NULL;
  }

  while(totalSizeSent < strlen(httpRequest)) {
    sizeSent = send(sock,(char*)((int)httpRequest+totalSizeSent),
                      strlen(httpRequest),0);
    if(sizeSent <= 0 && errno != EINTR) {
      gaim_debug_info("upnp",
            "gaim_upnp_http_request(): Failed In send\n\n");
      free(hrd->recvBuffer);
      free(hrd);
      close(sock);
      return NULL;
    }else if(errno == EINTR) {
      sizeSent = 0;
    }
    totalSizeSent += sizeSent;
  }

  hrd->tima = gaim_timeout_add(RECIEVE_TIMEOUT,
                                (GSourceFunc)gaim_upnp_timeout, hrd);
  hrd->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                              gaim_upnp_http_read, hrd);
  while (!hrd->done) {
    gtk_main_iteration();
  }
  close(sock);

  recvBuffer = hrd->recvBuffer;
  free(hrd);
  return recvBuffer;
}



/* This function takes the HTTP response requesting the description xml from
 * the UPnP enabled IGD. It also takes as input the URL the request was sent
 * to.
 *
 * The description contains the URL needed to control the actions of the UPnP
 * enabled IGD. This URL can be in the form of just a path, in whcih case we
 * have to append the path to the base URL. The base URL might be specified
 * in the XML, or it might not, in which case u append to the httpURL.
 */

/* at some point, maybe parse the description response using an xml library */
static char*
gaim_upnp_parse_description_response(const char* httpResponse, 
                                     const char* httpURL)
{
  char* wanIPConnectionStart;
  char urlBaseTag[] = "<URLBase>";
  char urlBaseEndTag[] = "</URLBase>";
  char controlURLTag[] = "<controlURL>";
  char controlURLEndTag[] = "</controlURL>";
  char* urlBaseTagLoc;
  char* urlBaseEndTagLoc;
  char* controlURLTagLoc;
  char* controlURLEndTagLoc;
  int controlURLSize;
  int baseURLSize;
  char* controlURL;
  char* baseURL;

  if(strstr(httpResponse, HTTP_OK) == NULL) {
    gaim_debug_info("upnp",
          "parse_description_response(): Failed In HTTP_OK\n\n");
    return NULL;
  }

  if((wanIPConnectionStart = 
      strstr(httpResponse, SEARCH_REQUEST_DEVICE)) == NULL) {
    gaim_debug_info("upnp",
          "parse_description_response(): Failed SEARCH_REQUEST_DEVICE\n\n");
    return NULL;
  }
  if((controlURLTagLoc = strstr(wanIPConnectionStart, controlURLTag))
                                                                == NULL) {
    gaim_debug_info("upnp",
          "parse_description_response(): Failed In controlURLTagLoc\n\n");
    return NULL;
  }
  if((controlURLEndTagLoc = 
    strstr(wanIPConnectionStart, controlURLEndTag)) == NULL) {
    gaim_debug_info("upnp",
          "parse_description_response(): Failed In controlURLEndTagLoc\n\n");
    return NULL;
  }


  controlURLSize =
    controlURLEndTagLoc-&controlURLTagLoc[strlen(controlURLTag)];
  if((controlURL = (char*)malloc(controlURLSize+1)) == NULL) {
    gaim_debug_info("upnp",
          "parse_description_response(): Failed In MALLOC controlURL\n\n");
    return NULL;
  }
  memcpy(controlURL, &controlURLTagLoc[strlen(controlURLTag)],
          controlURLSize);
  controlURL[controlURLSize] = '\0';


  if((urlBaseTagLoc = strstr(httpResponse, urlBaseTag)) == NULL) {
    if((baseURL = (char*)malloc(strlen(httpURL)+controlURLSize+1)) == NULL) {
      gaim_debug_info("upnp",
            "parse_description_response(): Failed In MALLOC baseURL\n\n");
      return NULL;
    }
    memcpy(baseURL, httpURL, strlen(httpURL));
    baseURL[strlen(httpURL)] = '\0';
  } else {
    if((urlBaseEndTagLoc = strstr(httpResponse, urlBaseEndTag)) == NULL) {
      gaim_debug_info("upnp",
            "parse_description_response(): Failed In urlBaseEndTagLoc\n\n");
      return NULL;
    }
    baseURLSize =
      urlBaseEndTagLoc - &urlBaseTagLoc[strlen(urlBaseTag)];
    if((baseURL = (char*)malloc(baseURLSize+controlURLSize+1)) == NULL) {
      gaim_debug_info("upnp",
            "parse_description_response(): Failed 2nd MALLOC baseURL\n\n");
      return NULL;
    }
    memcpy(baseURL, &urlBaseTagLoc[strlen(urlBaseTag)], baseURLSize);
    baseURL[baseURLSize] = '\0';
  }

  if(strstr(controlURL, "http://") == NULL) {
    memcpy(&baseURL[strlen(baseURL)], controlURL, strlen(controlURL)+1);
    free(controlURL);
    controlURL = baseURL;
  }else{
    free(baseURL);
  }

  return controlURL;
}




/* parse a url into it's approrpiate parts:
   address, port, path. return true on success, 
   false on failure */
static gboolean
gaim_upnp_parse_url(const char* url, char** path,
                    char** address, char** port)
{
  char* temp, *temp2;

  /* get the path */
  if((temp = strchr(&url[SIZEOF_HTTP], '/')) < 0) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_url(): Failed In 1\n\n");
    return FALSE;
  }
  if((*path = (char*)malloc(strlen(temp)+1)) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_url(): Failed In 2\n\n");
    return FALSE;
  }
  memcpy(*path, temp, strlen(temp));
  (*path)[strlen(temp)] = '\0';

  /* get the port */
  if((temp2 = strchr(&url[SIZEOF_HTTP], ':')) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_url(): Failed In 3\n\n");
    free(*path);
    return FALSE;
  }
  if((*port = (char*)malloc(temp-temp2)) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_url(): Failed In 4\n\n");
    free(*path);
    return FALSE;
  }
  memcpy(*port, &temp2[1], (temp-1)-temp2);
  (*port)[(temp-1)-temp2] = '\0';

  /* get the address */
  if((*address = (char*)malloc((temp2-&url[SIZEOF_HTTP])+1)) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_url(): Failed In 5\n\n");
    free(*path);
    free(*port);
    return FALSE;
  }
  memcpy(*address, &url[SIZEOF_HTTP],
           temp2-&url[SIZEOF_HTTP]);
  (*address)[temp2-&url[SIZEOF_HTTP]] = '\0';

  return TRUE;
}



/*
 * This function takes the URL where the UPnP enabled IGD description is
 * located at, and sends an HTTP request to retrieve that description. Once
 * the description is retrieved, we send the response to the 
 * gaim_upnp_parse_description_response() function. This will parse the
 * description, and return the control URL needed to control the IGD, which
 * this function returns.
 */
static char*
gaim_upnp_parse_description(const char* descriptionURL)
{
  char* fullURL;
  char* controlURL;
  char* httpResponse;
  char httpRequest[MAX_DESCRIPTION_HTTP_HEADER_SIZE];

  char* descriptionXMLAddress;
  char* descriptionAddressPort;
  char* descriptionAddress;
  char* descriptionPort;
  unsigned short port;

  /* parse the 4 above variables out of the descriptionURL 
     example description URL: http://192.168.1.1:5678/rootDesc.xml */

  /* parse the url into address, port, path variables */
  if(!gaim_upnp_parse_url(descriptionURL, &descriptionXMLAddress,
                          &descriptionAddress, &descriptionPort)) {
    return NULL;
  }
  if((port = atoi(descriptionPort)) == 0) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_description(): failed atoi\n\n");
    free(descriptionXMLAddress);
    free(descriptionAddress);
    free(descriptionPort);
    return NULL;
  }

  if((descriptionAddressPort = (char*)malloc(strlen(descriptionAddress) +
                                       strlen(descriptionPort) + 2))
                                                               == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_description(): failed MALLOC desAddPort\n\n");
    free(descriptionXMLAddress);
    free(descriptionAddress);
    free(descriptionPort);
    return NULL;
  }
  sprintf(descriptionAddressPort, "%s:%s",
          descriptionAddress, descriptionPort);

  if((fullURL = (char*)malloc(strlen(descriptionAddressPort) +
                                         SIZEOF_HTTP + 1)) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_description(): failed MALLOC fullURL\n\n");
    free(descriptionXMLAddress);
    free(descriptionAddress);
    free(descriptionPort);
    free(descriptionAddressPort);
  }
  sprintf(fullURL, "http://%s", descriptionAddressPort);

  /* for example...
     GET /rootDesc.xml HTTP/1.1\r\nHost: 192.168.1.1:5678\r\n\r\n */
  sprintf(httpRequest, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
          descriptionXMLAddress, descriptionAddressPort); 

  httpResponse = gaim_upnp_http_request(descriptionAddress,
                                        port, httpRequest);
  if(httpResponse == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_description(): httpResponse is NULL\n\n");
    free(descriptionXMLAddress);
    free(descriptionAddress);
    free(descriptionPort);
    free(descriptionAddressPort);
    free(fullURL);
    return NULL;
  }

  controlURL = gaim_upnp_parse_description_response(httpResponse, fullURL);

  free(descriptionXMLAddress);
  free(descriptionAddress);
  free(descriptionPort);
  free(descriptionAddressPort);
  free(fullURL);
  free(httpResponse);

  if(controlURL == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_parse_description(): controlURL is NULL\n\n");
    return NULL;
  }

  return controlURL;
}


static char*
gaim_upnp_parse_discover_response(const char* buf,
                                  unsigned int bufSize)
{
  char* startDescURL;
  char* endDescURL;
  char* descURL;
  unsigned int descURLSize;
  char* retVal;

  if(strstr(buf, HTTP_OK) == NULL) {
    gaim_debug_info("upnp",
          "parse_discover_response(): Failed In HTTP_OK\n\n");
    return NULL;
  }

  if((startDescURL = strstr(buf, "http://")) == NULL) {
    gaim_debug_info("upnp",
          "parse_description_response(): Failed In finding http://\n\n");
    return NULL;
  }

  endDescURL = strchr(startDescURL, '\r');
  if(endDescURL == NULL) {
    endDescURL = strchr(startDescURL, '\n');
    if(endDescURL == NULL) {
      gaim_debug_info("upnp",
            "parse_description_response(): Failed In endDescURL\n\n");
      return NULL;
    }else if(endDescURL == startDescURL) {
      gaim_debug_info("upnp",
            "parse_description_response(): endDescURL == startDescURL\n\n");
      return NULL;
    }
  }else if(endDescURL == startDescURL) {
    gaim_debug_info("upnp",
        "parse_description_response(): 2nd endDescURL == startDescURL\n\n");
    return NULL;
  }

  descURLSize = (endDescURL-startDescURL)+1;
  descURL = (char*)malloc(descURLSize);
  memcpy(descURL, startDescURL, descURLSize-1);
  descURL[descURLSize-1] = '\0';

  retVal = gaim_upnp_parse_description(descURL);
  free(descURL);
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
  HRD* hrd = data;

  gaim_timeout_remove(hrd->tima);
  length = sizeof(struct sockaddr_in);
  hrd->recvBuffer = (char*)malloc(MAX_DISCOVERY_RECIEVE_SIZE);

  do {
    sizeRecv = recvfrom(sock, hrd->recvBuffer,
                        MAX_DISCOVERY_RECIEVE_SIZE, 0,
                        (struct sockaddr*)&from, &length);

    if(sizeRecv > 0) {
      hrd->recvBuffer[sizeRecv] = '\0';
    }else if(errno != EINTR) {
      free(hrd->recvBuffer);
      hrd->recvBuffer = NULL;
    }
  }while(errno == EINTR);

  gaim_input_remove(hrd->inpa);
  hrd->done = TRUE;
  return;
}



const char*
gaim_upnp_discover(void)
{
  int sock, i;
  int sizeSent, totalSizeSent;
  extern int errno;
  gboolean sentSuccess, recvSuccess;
  struct sockaddr_in server;
  struct hostent *hp;
  char sendMessage[] = SEARCH_REQUEST_STRING;
  char *controlURL = NULL;

  HRD* hrd = (HRD*)malloc(sizeof(HRD));
  hrd->recvBuffer = NULL;
  hrd->done = FALSE;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    close(sock);
    gaim_debug_info("upnp",
          "gaim_upnp_discover(): Failed In sock creation\n\n");
    return NULL;
  }

  memset(&server, 0, sizeof(struct sockaddr));
  server.sin_family = AF_INET;
  if((hp = gethostbyname(HTTPMU_HOST_ADDRESS)) == NULL) {
    close(sock);
    gaim_debug_info("upnp",
          "gaim_upnp_discover(): Failed In gethostbyname\n\n");
    return NULL;
  }

  memcpy(&server.sin_addr,
        hp->h_addr_list[0],
        hp->h_length);
  server.sin_port = htons(HTTPMU_HOST_PORT);

  /* because we are sending over UDP, if there is a failure
     we should retry the send NUM_UDP_ATTEMPTS times */
  for(i = 0; i < NUM_UDP_ATTEMPTS; i++) {
    sentSuccess = TRUE;
    recvSuccess = TRUE;
    totalSizeSent = 0;

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
      hrd->tima = gaim_timeout_add(DISCOVERY_TIMEOUT, 
                                  (GSourceFunc)gaim_upnp_timeout, hrd);
      hrd->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                                 gaim_upnp_discover_udp_read, hrd);
      while (!hrd->done) {
        gtk_main_iteration();
      }
      if(hrd->recvBuffer == NULL) {
        recvSuccess = FALSE;
      } else {
        /* parse the response, and see if it was a success */
        close(sock);
        if((controlURL=
            gaim_upnp_parse_discover_response(hrd->recvBuffer,
                                         strlen(hrd->recvBuffer)))==NULL) {
          gaim_debug_info("upnp",
                       "gaim_upnp_discover(): Failed In parse response\n\n");
          return NULL;
        }
      }
    }

    /* if sent success and recv successful, then break */
    if(sentSuccess && recvSuccess) {
      i = NUM_UDP_ATTEMPTS;
    }
  }
  if(!sentSuccess || !recvSuccess) {
    close(sock);
    gaim_debug_info("upnp",
          "gaim_upnp_discover(): Failed In sent/recv success\n\n");
    return NULL;
  }

  return controlURL;
}




static char*
gaim_upnp_generate_action_message_and_send(const char* controlURL,
                                          const char* actionName, 
                                          const char* actionParams)
{
  char serviceType[] = "WANIPConnection:1";
  char* actionMessage;
  char* soapMessage;
  char* httpResponse;

  char* pathOfControl;
  char* addressOfControl;
  char* addressPortOfControl;
  char* portOfControl;
  unsigned short port;

  /* set the soap message */
  if((soapMessage = (char*)malloc(strlen(SOAP_ACTION) +
                              strlen(serviceType) +
                              strlen(actionParams) +
                              strlen(actionName) +
                              strlen(actionName))) == NULL) {
      gaim_debug_info("upnp",
        "generate_action_message_and_send(): Failed MALLOC soapMessage\n\n");
      return NULL;
  }
  sprintf(soapMessage, SOAP_ACTION, actionName, serviceType, 
          actionParams, actionName);

  /* parse the url into address, port, path variables */
  if(!gaim_upnp_parse_url(controlURL, &pathOfControl,
                          &addressOfControl, &portOfControl)) {
    gaim_debug_info("upnp",
          "generate_action_message_and_send(): Failed In Parse URL\n\n");
    free(soapMessage);
    return NULL;
  }

  /* set the addressPortOfControl variable which should have a
     form like the following: 192.168.1.1:8000 */
  if((addressPortOfControl = (char*)malloc(strlen(addressOfControl) +
                                           strlen(portOfControl) + 
                                           2)) == NULL) {
    gaim_debug_info("upnp",
      "generate_action_message_and_send(): MALLOC addressPortOfControl\n\n");
    free(soapMessage);
    free(pathOfControl);
    free(addressOfControl);
    free(portOfControl);
    return NULL;
  }
  sprintf(addressPortOfControl, "%s:%s", addressOfControl, portOfControl);
  if((port = atoi(portOfControl)) == 0) {
    gaim_debug_info("upnp",
          "generate_action_message_and_send(): Failed In port = atoi\n\n");
    free(soapMessage);
    free(pathOfControl);
    free(addressOfControl);
    free(portOfControl);
    free(addressPortOfControl);
    return NULL;
  }

  /* set the HTTP Header */
  if((actionMessage = (char*)malloc(strlen(HTTP_HEADER_ACTION) +
                             strlen(pathOfControl) +
                             strlen(addressPortOfControl) +
                             strlen(serviceType) +
                             strlen(actionName) +
                             strlen(soapMessage))) == NULL) {
    gaim_debug_info("upnp", 
      "generate_action_message_and_send(): Failed MALLOC actionMessage\n\n");
    free(soapMessage);
    free(pathOfControl);
    free(addressOfControl);
    free(portOfControl);
    free(addressPortOfControl);
    return NULL;
  }
  sprintf(actionMessage, HTTP_HEADER_ACTION,
          pathOfControl, addressPortOfControl,
          serviceType, actionName, strlen(soapMessage));

  /* append to the header the body */
  strcat(actionMessage, soapMessage);

  /* get the return of the http response */
  httpResponse = gaim_upnp_http_request(addressOfControl, 
                                        port, actionMessage);
  if(httpResponse == NULL) {
    gaim_debug_info("upnp",
          "generate_action_message_and_send(): Failed In httpResponse\n\n");
  }

  free(actionMessage);
  free(soapMessage);
  free(pathOfControl);
  free(addressOfControl);
  free(portOfControl);
  free(addressPortOfControl);

  return httpResponse;
}




const char*
gaim_upnp_get_public_ip(const char* controlURL)
{
  char* extIPAddress;
  char* httpResponse;
  char actionName[] = "GetExternalIPAddress";
  char actionParams[] = "";
  char* temp, *temp2;

  /* make sure controlURL is a valid URL */
  if(!gaim_upnp_validate_url(controlURL)) {
    gaim_debug_info("upnp",
          "gaim_upnp_get_public_ip(): Failed In Validate URL\n\n");
    return NULL;
  }

  httpResponse = gaim_upnp_generate_action_message_and_send(controlURL,
                                                            actionName, 
                                                            actionParams);
  if(httpResponse == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_get_public_ip(): Failed In httpResponse\n\n");
    return NULL;
  }

  /* extract the ip, or see if there is an error */
  /* at some point, maybe extract the ip using an xml library */
  if((temp = strstr(httpResponse, "<NewExternalIPAddress")) == NULL) {
    gaim_debug_info("upnp",
      "gaim_upnp_get_public_ip(): Failed Finding <NewExternalIPAddress\n\n");
    free(httpResponse);
    return NULL;
  }
  if((temp = strchr(temp, '>')) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_get_public_ip(): Failed In Finding >\n\n");
    free(httpResponse);
    return NULL;
  }
  if((temp2 = strchr(temp, '<')) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_get_public_ip(): Failed In Finding <\n\n");
    free(httpResponse);
    return NULL;
  }
  if((extIPAddress = (char*)malloc(temp2-temp)) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_get_public_ip(): Failed In MALLOC extIPAddress\n\n");
    free(httpResponse);
    return NULL;
  }
  memcpy(extIPAddress, &temp[1], (temp2-1)-temp);
  extIPAddress[(temp2-1)-temp] = '\0';

  free(httpResponse);
  gaim_debug_info("upnp",
        "gaim_upnp_get_public_ip() IP: %s\n\n", extIPAddress);
  return extIPAddress;
}



static const char*
gaim_upnp_get_local_ip_address(const char* address) {
  const char* ip;
  int sock;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char* pathOfControl;
  char* addressOfControl;
  char* portOfControl;
  unsigned short port;

  /* parse the url into address, port, path variables */
  if(!gaim_upnp_parse_url(address, &pathOfControl,
                          &addressOfControl, &portOfControl)) {
    gaim_debug_info("upnp",
          "get_local_ip_address(): Failed In Parse URL\n\n");
    return NULL;
  }
  if((port = atoi(portOfControl)) == 0) {
    gaim_debug_info("upnp",
          "get_local_ip_address(): Failed In port = atoi\n\n");
    return NULL;
  }

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    gaim_debug_info("upnp",
          "get_local_ip_address(): Failed In sock creation\n\n");
    return NULL;
  }

  server = gethostbyname(addressOfControl);
  if(server == NULL) {
    gaim_debug_info("upnp",
          "get_local_ip_address(): Failed In gethostbyname\n\n");
    close(sock);
    return NULL;
  }
  memset((char*)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr,
        server->h_addr_list[0],
        server->h_length);
  serv_addr.sin_port = htons(port);

  if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
    gaim_debug_info("upnp",
          "get_local_ip_address(): Failed In connect\n\n");
    close(sock);
    return NULL;
  }

  ip = gaim_network_get_local_system_ip(sock);

  close(sock);
  return ip;
}



gboolean
gaim_upnp_set_port_mapping(const char* controlURL, unsigned short portMap,
                           const char* protocol)
{
  char* httpResponse;
  char actionName[] = "AddPortMapping";
  char* actionParams;
  const char* internalIP;

  /* make sure controlURL is a valid URL */
  if(!gaim_upnp_validate_url(controlURL)) {
    gaim_debug_info("upnp",
          "gaim_upnp_set_port_mapping(): Failed In Validate URL\n\n");
    return FALSE;
  }

  /* get the internal IP */
  if((internalIP = gaim_upnp_get_local_ip_address(controlURL)) == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_set_port_mapping(): couldn't get local ip\n\n");
    return FALSE;
  }

  /* make the portMappingParams variable */
  if((actionParams = (char*)malloc(strlen(ADD_PORT_MAPPING_PARAMS) +
                                        strlen(internalIP) +
                                        10 +    /* size of port as int * 2 */
                                        strlen(protocol))) == NULL) {
    gaim_debug_info("upnp",
        "gaim_upnp_set_port_mapping(): Failed MALLOC portMappingParams\n\n");
    return FALSE;
  }
  sprintf(actionParams, ADD_PORT_MAPPING_PARAMS, portMap, 
          protocol, portMap, internalIP);

  httpResponse = gaim_upnp_generate_action_message_and_send(controlURL,
                                                            actionName, 
                                                            actionParams);

  if(httpResponse == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_set_port_mapping(): Failed In httpResponse\n\n");
    free(actionParams);
    return FALSE;
  }

  /* determine if port mapping was a success */
  if(strstr(httpResponse, HTTP_OK) == NULL) {
    gaim_debug_info("upnp",
     "gaim_upnp_set_port_mapping(): Failed HTTP_OK\n\n%s\n\n", httpResponse);
    free(actionParams);
    free(httpResponse);
    return FALSE;
  }

  free(actionParams);
  free(httpResponse);
  return TRUE;
}


gboolean
gaim_upnp_remove_port_mapping(const char* controlURL, unsigned short portMap,
                           const char* protocol)
{
  char* httpResponse;
  char actionName[] = "DeletePortMapping";
  char* actionParams;

  /* make sure controlURL is a valid URL */
  if(!gaim_upnp_validate_url(controlURL)) {
    gaim_debug_info("upnp",
          "gaim_upnp_set_port_mapping(): Failed In Validate URL\n\n");
    return FALSE;
  }

  /* make the portMappingParams variable */
  if((actionParams = (char*)malloc(strlen(DELETE_PORT_MAPPING_PARAMS) +
                                        5 +    /* size of port as int */
                                        strlen(protocol))) == NULL) {
    gaim_debug_info("upnp",
       "gaim_upnp_set_port_mapping(): Failed MALLOC portMappingParams\n\n");
    return FALSE;
  }
  sprintf(actionParams, DELETE_PORT_MAPPING_PARAMS, 
          portMap, protocol);

  httpResponse = gaim_upnp_generate_action_message_and_send(controlURL,
                                                            actionName, 
                                                            actionParams);

  if(httpResponse == NULL) {
    gaim_debug_info("upnp",
          "gaim_upnp_set_port_mapping(): Failed In httpResponse\n\n");
    free(actionParams);
    return FALSE;
  }

  /* determine if port mapping was a success */
  if(strstr(httpResponse, HTTP_OK) == NULL) {
    gaim_debug_info("upnp",
     "gaim_upnp_set_port_mapping(): Failed HTTP_OK\n\n%s\n\n", httpResponse);
    free(actionParams);
    free(httpResponse);
    return FALSE;
  }

  free(actionParams);
  free(httpResponse);
  return TRUE;
}
