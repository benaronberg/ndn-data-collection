/**
 * @file ndnClientPerformanceCollector.c
 *  Receive interest packet and respond with sending data packets include NDN traffic performance details
 *
 * A NDNx command-line utility.
 *
 * Copyright (C) 2008-2010 Palo Alto Research Center, Inc.
 *
 * This work is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This work is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ndn/ndn.h>
#include <ndn/uri.h>
#include <ndn/keystore.h>
#include <ndn/signing.h>
#include <unistd.h>
#include <ndn/ndnd.h>
#include <ndn/ndn_private.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <expat.h>


#define DEBUG 0
#define MON_NAME_PREFIX "ndn:/ndn/edu/wustl/ndnstatus"
#define INTEREST_LIFETIME_SEC 1

int parser_on = 0;
char current_element[256];
char bandwidth_type[16];
char myipaddr[20];
int Depth;
struct ndn *ndn = NULL;
//struct ndn_closure *in_data_ptr = NULL;
char progname[64];
char current_time[50];

struct mydata {
	void* none;
};

struct face_status
{
  char ipaddr[50];
  char tx[50];
  char rx[50];
  int id;
};

struct face_status* current_face = NULL;

struct interestData {
  bool valid;
  char ipaddr[50];
  unsigned long tx;
  unsigned long rx;
  char current_time[50];
};

#define NUM_RECORDED_INTERESTS 2048

struct interestData recordedInterestData[NUM_RECORDED_INTERESTS];
int recordedInterestIndex=0;

void 
initInterests()
{
  int i;
  for (i=0; i < NUM_RECORDED_INTERESTS; i++)
    recordedInterestData[i].valid = false;
  recordedInterestIndex = 0;
}

void 
printInterests()
{
  int i;
  for (i=0; i < recordedInterestIndex; i++) {
    if (recordedInterestData[i].valid == false)
      return;
    else {
      printf("interest[%i]: %s %ld %ld %s\n", i, recordedInterestData[i].ipaddr ,recordedInterestData[i].tx ,recordedInterestData[i].rx ,recordedInterestData[i].current_time);
    }
  }
}

void
recordInterest(char *ipaddr, char *rx, char *tx, char *time)
{
  int i;

  for (i=0; i < recordedInterestIndex; i++) {
    if (recordedInterestData[i].valid && !strcmp(recordedInterestData[i].ipaddr, ipaddr)) {
      // Found it!
      //printf("recordInterest: Found an already recorded ipaddr: %s\n", ipaddr);
      recordedInterestData[i].tx += atol(tx);
      recordedInterestData[i].rx += atol(rx);
      strcpy(recordedInterestData[i].current_time, time);
      return;
    }
  }
  if (i < NUM_RECORDED_INTERESTS) {
    recordedInterestData[i].valid = true;
    recordedInterestData[i].tx = atol(tx);
    recordedInterestData[i].rx = atol(rx);
    strcpy(recordedInterestData[i].current_time, time);
    strcpy(recordedInterestData[i].ipaddr, ipaddr);
    recordedInterestIndex++;
  }
  else {
    printf("recordInterest: TOO MANY FACES!!!\n");
  }

}


void
start_element(void *data, const char *el, const char **attr) {
  /*int i;
  
  for (i = 0; i < Depth; i++)
    printf("  ");

  printf("%s", el);

  for (i = 0; attr[i]; i += 2) {
    printf(" %s='%s'", attr[i], attr[i + 1]);
  }

  printf("\n");*/
  Depth++;

  if (strcmp(el, "ndnd") == 0)
    parser_on = 1;
  if (!parser_on)
    return;
  strcpy(current_element, el);
  if ((strcmp(el, "bytein") == 0) || (strcmp(el, "byteout") == 0))
    strcpy(bandwidth_type, el);
}  /* End of start handler */


struct ndn_charbuf*
make_template(void)
{
  intmax_t lifetime = INTEREST_LIFETIME_SEC << 2;
  struct ndn_charbuf *templ = ndn_charbuf_create();
  ndn_charbuf_append_tt(templ, NDN_DTAG_Interest, NDN_DTAG);
  ndn_charbuf_append_tt(templ, NDN_DTAG_Name, NDN_DTAG);
  ndn_charbuf_append_closer(templ); /* </Name> */
  ndn_charbuf_append_tt(templ, NDN_DTAG_MaxSuffixComponents, NDN_DTAG);
  ndnb_append_number(templ, 1);
  ndn_charbuf_append_closer(templ); // </MaxSuffixComponents>
  ndnb_append_tagged_binary_number(templ, NDN_DTAG_InterestLifetime, lifetime);
  ndn_charbuf_append_closer(templ); // </InterestLifetime>
  //ndn_charbuf_append_closer(templ); /* </Interest> */
  return(templ);
}


// cb for incoming packets
enum ndn_upcall_res
incoming_data(
    struct ndn_closure *selfp,
    enum ndn_upcall_kind kind,
    struct ndn_upcall_info *info)
{
  switch (kind)
    {
    case NDN_UPCALL_FINAL:
      if (DEBUG)
	printf("received NDN_UPCALL_FINAL\n");
      free(selfp);
      break;
    case NDN_UPCALL_INTEREST:
      if (DEBUG)
	printf("received interest\n");
      break;
    case NDN_UPCALL_INTEREST_TIMED_OUT:
      if (DEBUG)
	printf("Interest timeout...\n");
      break;
    case NDN_UPCALL_CONTENT:
      if (DEBUG)
	printf("content\n");
      break;
    default:
      if (DEBUG)
	printf("something else...kind = %d\n",kind);
      break;
    }

  return(NDN_UPCALL_RESULT_OK);
}

int
send_interest(struct face_status* face)
{
  if (strlen(face->ipaddr) <= 0) return 0;
  struct ndn_charbuf *templ;
  int res;
  // set the callback for incoming interests.
  //struct ndn_closure in_interest; 
  // Create the name for the prefix we're interested in
  struct ndn_charbuf *name = NULL;
  char tmp_name[512];
  
  name = ndn_charbuf_create();

  
  sprintf(tmp_name, "%s/%s/%s/%s/%s/%s", MON_NAME_PREFIX, myipaddr, face->ipaddr, current_time, face->tx, face->rx);
  //printf("tmp_name: %s\n", tmp_name);
  recordInterest(face->ipaddr, face->rx, face->tx, current_time);
/*

  res = ndn_name_from_uri(name, tmp_name);
  if (res < 0)
    {
      fprintf(stderr, "%s: bad ndn URI: %s\n", progname, tmp_name);
    }
  
  // Allocate some working space for storing content
  //in_interest.data = ndn;
    
  templ = make_template();
  //res = 0;
  struct ndn_closure* in_data_ptr = malloc(sizeof(*in_data_ptr));
  in_data_ptr->p = &incoming_data;
  in_data_ptr->data = NULL;
  in_data_ptr->refcount = 0;
//  res = ndn_express_interest(ndn, name, in_data_ptr, templ);
  //res = ndn_express_interest(ndn, name, NULL, templ);
  if(res < 0) fprintf(stderr, "%s: error expressing interest: %s\n", progname, tmp_name);
  else if (DEBUG)
    printf("Interest sent! %s\n", tmp_name);

  ndn_charbuf_destroy(&templ);
  ndn_charbuf_destroy(&name);
*/
  return res;
}
void
sendRecordedInterests()
{
  struct ndn_charbuf *templ;
  int res;
  // set the callback for incoming interests.
  //struct ndn_closure in_interest; 
  // Create the name for the prefix we're interested in
  struct ndn_charbuf *name = NULL;
  char tmp_name[512];
  int i;

  for (i=0; i < recordedInterestIndex; i++) {
    if (recordedInterestData[i].valid == false)
      return;
    else {
      //printf("sendRecordedInterests(): interest[%i]: %s %ld %ld %s\n", i, recordedInterestData[i].ipaddr ,
      //                                                                  recordedInterestData[i].tx ,
      //                                                                  recordedInterestData[i].rx ,
      //                                                                  recordedInterestData[i].current_time);
  
      name = ndn_charbuf_create();

  
      sprintf(tmp_name, "%s/%s/%s/%s/%ld/%ld", MON_NAME_PREFIX, myipaddr, 
                                             recordedInterestData[i].ipaddr, 
                                             recordedInterestData[i].current_time, 
                                             recordedInterestData[i].tx, 
                                             recordedInterestData[i].rx);
//printf("sendRecordedInterests(): tmp_name: %s\n", tmp_name);

      res = ndn_name_from_uri(name, tmp_name);
      if (res < 0)
        {
          fprintf(stderr, "%s: bad ndn URI: %s\n", progname, tmp_name);
        }
  
      // Allocate some working space for storing content
      //in_interest.data = ndn;
    
      templ = make_template();
      //res = 0;
      struct ndn_closure* in_data_ptr = malloc(sizeof(*in_data_ptr));
      in_data_ptr->p = &incoming_data;
      in_data_ptr->data = NULL;
      in_data_ptr->refcount = 0;
      res = ndn_express_interest(ndn, name, in_data_ptr, templ);
      //res = ndn_express_interest(ndn, name, NULL, templ);
      if(res < 0) fprintf(stderr, "%s: error expressing interest: %s\n", progname, tmp_name);
      else if (DEBUG)
        printf("Interest sent! %s\n", tmp_name);
    
      ndn_charbuf_destroy(&templ);
      ndn_charbuf_destroy(&name);
    }

  }
}

void
end_element(void *data, const char *el) {
  if (strcmp(el, "face") == 0 || strcmp(el, "ndnd") == 0)
    {
      /*send interest with current_face*/
      if (current_face != NULL)
	{
	  send_interest(current_face);
	  free(current_face);
	  current_face = NULL;
	}
      current_element[0] = '\0';
    }
  else if (strcmp(el, "bytein") == 0 || strcmp(el, "byteout") == 0)
    {
      bandwidth_type[0] = '\0';
    }
  else if (strcmp(el, "faces") == 0)
    {
      parser_on = 0;
      if (current_face != NULL)
	{
	  free(current_face);
	  current_face = NULL;
	}
      current_element[0] = '\0';
    }
  Depth--;
}  /* End of end handler */

void
char_data_handler(void* data, const char* s, int len)
{
  if (!parser_on) return;
  char tmp_str[256];
  int i = 0;
  strncpy(tmp_str, s, len);
  tmp_str[len] = '\0';
  if (strcmp(current_element, "total") == 0 &&
      current_face != NULL &&
      strlen(bandwidth_type) > 0)
    {
      if (strcmp(bandwidth_type, "bytein") == 0)
	strcpy(current_face->rx,tmp_str);
      else
	strcpy(current_face->tx, tmp_str);
    }
  else if (strcmp(current_element, "faceid") == 0)
    {
      if (current_face != NULL)
	{
	  free(current_face);
	  printf(" error in parsing received new face before end of old one");
	}
      current_face = malloc(sizeof(*current_face));/*get_face(atoi(tmp_str));*/
      current_face->id = atoi(tmp_str);
      current_face->ipaddr[0] = '\0';
      current_face->tx[0] = '\0';
      current_face->rx[0] = '\0';
    }
  else if (strcmp(current_element, "now") == 0)
    {
      strcpy(current_time,tmp_str); 
    }
  else if (strcmp(current_element, "ip") == 0)
    {
      if (current_face != NULL &&
	  tmp_str[0] != '[' &&
	  tmp_str[0] != '0')
	{
	  for(i = 0; i < len; ++i)
	    {
	      if (tmp_str[i] == ':')
		{
		  current_face->ipaddr[i] = '\0';
		  break;
		}
	      else
		current_face->ipaddr[i] = tmp_str[i];
	    }
	  //strcpy(current_face->ipaddr, tmp_str);
	}
    }
}

static void
usage(const char *progname)
{
    fprintf(stderr,
            "%s ndn:/a/b\n"
            " Reply with performance data to interest it receives."
            "\n"
            "  -h - print this message and exit"
            "  -i <ipaddr> - host ip address (of this host) to be included in interests"
            "\n", progname);
    exit(1);
}

// This function send http request to the local ndnd.
// char * response - stores http response 
int SendHTTPGetRequest(char * response)
{
	int port = atoi(NDN_DEFAULT_UNICAST_PORT);//9695;
	const char *host = "localhost";
	struct hostent *server;
	struct sockaddr_in serveraddr;
	char request[1000];
	int n = 1, total = 0, found = 0;

	int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (tcpSocket < 0)
	{
		if (DEBUG)
			printf("\nError opening socket\n");
	}
	else
	{
		if (DEBUG)
			printf("\nSuccessfully opened socket\n");
	}
	server = gethostbyname(host);
	if (server == NULL)
	{
		if (DEBUG)
			printf("gethostbyname() failed\n");
		return 0;
	}

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;

	bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);

	serveraddr.sin_port = htons(port);

	if (connect(tcpSocket, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		if (DEBUG)
			printf("Error Connecting\n");
	}
	else
	{
		if (DEBUG)
			printf("Successfully Connected\n");
	}
	bzero(request, 1000);

	sprintf(request, "GET /?f=xml  HTTP/1.1\r\n");

	if (send(tcpSocket, request, strlen(request), 0) < 0)
		printf("Error with send()\n");
	else
	{
		if (DEBUG)
			printf("Successfully sent html fetch request\n");
	}

	bzero(request, 1000);

	// Keep reading up to a '</ndnd>'
    while (!found)
    {
        n = recv(tcpSocket, response + total, 1024, 0);
        if (n == -1)
        {
            // Error, check 'errno' for more details
            break;
        }
        total += n;
        response[total] = '\0';
        found = (strstr(response, "</ndnd>") != 0);
    }
    //printf("finished read %s\n",response);
	close(tcpSocket);
	return 1;
}


int
main(int argc, char **argv)
{
    int res;
    struct hostent *addr_result;
    char* tmp_str;
    struct in_addr naddr;
    int poll_period = 1; 
    char *xml_response;
    unsigned int size = 1024*1024;
    int offset = 0;
    int skiplines = 0;
    int i = 0;
    char myhost_name[128];

    strcpy(progname, argv[0]);
 
    XML_Parser parser = NULL;

    current_element[0] = '\0';
    current_face = NULL;
    Depth = 0;
    parser_on = 0;
    bandwidth_type[0] = '\0';
    myipaddr[0] = '\0';

    // Parse cmd-line arguments
    while ((res = getopt(argc, argv, "hi:")) != -1) {
        switch (res) {
            default:
            case 'i':
                strcpy(myipaddr, optarg);
                break;

            case 'h':
                usage(progname);
                break;
        }
    }
    argc -= optind;
    argv += optind;
    
    if (argv[0] != NULL)
      poll_period = atoi(argv[0]);

    printf("%s polling every %d sec.\n", progname, poll_period);

    if (myipaddr[0] == '\0') {
      res = gethostname(myhost_name, 128);
      if (res != 0)
        {
	  perror("Could not get my host name");
	  exit(1);
        }
  
      addr_result = gethostbyname(myhost_name);
      if (addr_result == NULL)
        {
	  perror("Could not get my host ipaddr: gethostbyname() failed");
	  exit(1);
        }
      i = 0;
      // setting cb to incoming data packets
      //struct mydata mydata = { 0 };
      // struct ndn_closure in_data = {.p=&incoming_data, .data=ndn};//&mydata};
      //in_data_ptr = &in_data;
      while(1)
        {
	  if (addr_result->h_addr_list[i] == NULL) break;
	  bzero((char*)&naddr, sizeof(naddr));
	  bcopy((char*)addr_result->h_addr_list[i], (char*)&naddr.s_addr, addr_result->h_length);
	  tmp_str = inet_ntoa(naddr);
	  if (strncmp(tmp_str, "10.", 3) && strncmp(tmp_str, "127.0.", 6) && tmp_str[0] != '0')
	    {
	      strcpy(myipaddr, tmp_str);
	      break;
	    }
	  ++i;
        }
    }

    if (strlen(myipaddr) > 0)
      printf("My IPaddr is %s\n", myipaddr);
    else
      {
	perror("Couldn't set my ipaddr \n");
	exit(1);
      }


    // Connect to ndnd
    
    ndn = ndn_create();
    if (ndn_connect(ndn, NULL) == -1) {
        perror("Could not connect to ndnd");
        exit(1);
	}
    xml_response = malloc(sizeof(*xml_response)*size);
    res = 0;
    while (res >= 0 )
      {	
initInterests();
	res = ndn_run(ndn, 200);
	parser = XML_ParserCreate(NULL);
	if (! parser) {
	  fprintf(stderr, "Couldn't allocate memory for parser\n");
	  break;
	}
	XML_SetElementHandler(parser, start_element, end_element);
	XML_SetCharacterDataHandler(parser, char_data_handler);
	
	res = SendHTTPGetRequest(xml_response);
	if (res < 1)
	  {
	    perror("XML get failed\n");
	    break;
	  }
	offset = 0;
	skiplines = 0;
	while(skiplines < 5)
	  {
	    if (xml_response[offset] == '\n')
	      ++skiplines;
	    ++offset;
	  }
	res = XML_Parse(parser, xml_response+offset, strlen(xml_response+offset), 1);
	if (res == 0) 
	  {
	    perror("XML parse error\n");
	  }
	else res = 1;
	if (current_face != NULL)
	  {
	    free(current_face);
	    current_face = NULL;
	  }
	XML_ParserFree(parser);
	
        //printf("\n");
        //printInterests();
        sendRecordedInterests();
	sleep(poll_period);
      }
    free(xml_response);
    printf("exit client...\n");

    XML_ParserFree(parser);
    
    ndn_destroy(&ndn);
    
    return EXIT_SUCCESS;
}



