
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
#include <netdb.h>

#include <string.h>
#include <ndn/ndn.h>
#include <ndn/charbuf.h>
#include <ndn/coding.h>
#include <ndn/uri.h>


/* interest name format for status ndn:/ndn/edu/wustl/ndnstatus/<server ip>/<face ip>/<timestamp>/<tx bytes>/<rx bytes>/ */
#define DEBUG 0
#define MON_NAME_PREFIX "ndn:/ndn/edu/wustl/ndnstatus"

char map_server_addr[128];

struct face_status
{
  char sipaddr[50];
  char fipaddr[50];
  char timestamp[50];
  char tx[50];
  char rx[50];
  unsigned long rxbits;
  unsigned long txbits;
  int id;
};

struct link_entry
{ 
  char sipaddr[50];
  char fipaddr[50];
  int id;
};

struct link_entry** link_entries;
int num_link_entries;

static void
usage(const char *progname)
{
    fprintf(stderr,
            "%s [-h] -f link_file -n number_of_linkids [-s map_addr]\n"
            " Receives status info in form of interests from gateways."
            "\n"
            "  -h - print this message and exit"
            "\n"
	    "  -f file_name - link_file is name of file containing ip pairs associated with linkid"
	    "\n"
	    "  -n int -  number_of_linkids supplied by the linkfile"
	    "\n"
	    "  -s ipaddr - addr added to curl command for ndn map"
	    "\n", progname);
    exit(1);
}


int
get_link_id(char* sipaddr, char* fipaddr)
{
  int i = 0;
  for (i = 0; i < num_link_entries; ++i)
    {
      if (!strcmp(link_entries[i]->sipaddr, sipaddr) && !strcmp(link_entries[i]->fipaddr, fipaddr))
	return (link_entries[i]->id);
    }
  return -1;
}

// Callback for incoming interest. 
enum ndn_upcall_res
incoming_interest(
                  struct ndn_closure *selfp,
                  enum ndn_upcall_kind kind,
                  struct ndn_upcall_info *info)
{
  int res;
  const unsigned char *p = NULL;
  size_t size = 0;
  int endc = info->interest_comps->n - 2;
  struct ndn *ndn = selfp->data;
  char tmp_str[50];
  char cmd_str[256];
  // int i;
  
  // Switch different kinds of incoming packets
  switch (kind)
    {
    case NDN_UPCALL_INTEREST:
      if (DEBUG)
	printf("Received interest packet with number of components %d\n", info->interest_comps->n);
      /* for (i = 0; i < info->interest_comps->n; ++i)
	{
	  res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, i, &p, &size);
	  tmp_str[0] = '\0';
	  if (size < 50)
	    {
	      strncpy(tmp_str, (const char*)p, size);
	      tmp_str[size] = '\0';
	    }
	  if (DEBUG)
	    printf(" component %d = %s :", i, tmp_str);
	}
	if (DEBUG) printf("\n");*/


      if (endc < 7)
	{
	  if (DEBUG)
	    printf("Non monitoring interest received endc = %d\n", endc);
	  break;
	}
      else
	{
// They all start /ndn/edu/wustl/ndnstatus
// This define tells how many come before the ndnstatus
#define NDNSTATUS_PREFIX_COMPONENTS 3
	  res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, NDNSTATUS_PREFIX_COMPONENTS, &p, &size);
	  tmp_str[0] = '\0';
	  if (size < 50)
	    {
	      strncpy(tmp_str, (const char*)p, size);
	      tmp_str[size] = '\0';
	    }
	  if (strlen(tmp_str) == 0 || strcmp(tmp_str, "ndnstatus"))
	    {
	      if (DEBUG)
		printf("Non monitoring interest received %s\n", tmp_str);
	      break;
	    }
	}
      
      struct face_status fstatus;
      
      res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, endc, &p, &size);
      if (res < 0)
	printf("error getting rx component %d from name\n", endc);
      else
	{
	  strncpy(fstatus.rx, (const char*)p, size);
	  fstatus.rx[size] = '\0';
	  fstatus.rxbits = (atol(fstatus.rx)) * 8;
	  if (DEBUG)
	    printf(" c%d:%s,", endc, fstatus.rx);
	}
      --endc;
	
      res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, endc, &p, &size);	  
      if (res < 0)
	printf("error getting tx component %d from name\n", endc);
      else
	{
	  strncpy(fstatus.tx, (const char*)p, size);
	  fstatus.tx[size] = '\0';
	  fstatus.txbits = (atol(fstatus.tx)) * 8;
	  if (DEBUG)
	    printf(" c%d:%s,", endc, fstatus.tx);
	}
      --endc;
	
      res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, endc, &p, &size);	  
      if (res < 0)
	printf("error getting timestamp component %d from name\n", endc);
      else
	{
	  strncpy(fstatus.timestamp, (const char*)p, size);
	  fstatus.timestamp[size] = '\0';
	  if (DEBUG)
	    printf(" c%d:%s,", endc, fstatus.timestamp);
	}
      --endc;

      res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, endc, &p, &size);	  
      if (res < 0)
	printf("error getting fipaddr component %d from name\n", endc);
      else
	{
	  strncpy(fstatus.fipaddr, (const char*)p, size);
	  fstatus.fipaddr[size] = '\0';
	  if (DEBUG)
	    printf(" c%d:%s,", endc, fstatus.fipaddr);
	}
      --endc;

      res = ndn_name_comp_get(info->interest_ndnb, info->interest_comps, endc, &p, &size);	  
      if (res < 0)
	printf("error getting sipaddr component %d from name\n", endc);
      else
	{
	  strncpy(fstatus.sipaddr, (const char*)p, size);
	  fstatus.sipaddr[size] = '\0';
	  if (DEBUG)
	    printf(" c%d:%s\n", endc, fstatus.sipaddr);
	}
      --endc;
      
      /* 
      // Extract URI from interest and set it in name buffer
      struct ndn_charbuf *name = NULL;
      name = ndn_charbuf_create();
      unsigned name_start = info->pi->offset[NDN_PI_B_Name];
      unsigned name_size = info->pi->offset[NDN_PI_E_Name] - name_start;
      
      ndn_charbuf_append(name, info->interest_ndnb + name_start, name_size);
      */
      
      //if (!strncmp((char*)name->buf, MON_NAME_PREFIX, strlen(MON_NAME_PREFIX)))
      //{
      //sscanf((const char*)name->buf, "ndn:/ndn/edu/wustl/ndnstatus/%s/%s/%s/%s/%s", fstatus.sipaddr, fstatus.fipaddr, fstatus.timestamp, fstatus.tx, fstatus.rx);
	  
      fstatus.id = get_link_id(fstatus.sipaddr, fstatus.fipaddr);
      if (fstatus.id >= 0)
	{
	  //sprintf(cmd_str, "curl -s -L http://%s/bw/%d/%s/%u/%u \n", map_server_addr, fstatus.id, fstatus.timestamp, fstatus.txbits, fstatus.rxbits);
	  sprintf(cmd_str, "http://%s/bw/%d/%s/%u/%u", map_server_addr, fstatus.id, fstatus.timestamp, fstatus.txbits, fstatus.rxbits); 
	  if (DEBUG)
	    printf("cmd to pass to curl: %s\n", cmd_str);
          //system(cmd_str);
          int status;
          // check for zombies
          int deadChildPid = waitpid(-1, &status, WNOHANG);
          int pid;
          if ((pid = fork()) < 0)
            printf("for failed for curl %s\n", cmd_str);
          else
            {
               if (pid == 0)
                  execl("/usr/bin/curl","curl", "-s", "-L", cmd_str, NULL);
            }
          // check for zombies again
          deadChildPid = waitpid(-1, &status, WNOHANG);
        }

      return(NDN_UPCALL_RESULT_INTEREST_CONSUMED);
      //}
      //if (DEBUG)
      //printf("Non monitoring interest received\n");
      //break;
    default:
      if (DEBUG)
	printf("Received packet different than interest packet, kind = %d\n",kind);
      break;
    }
  return(NDN_UPCALL_RESULT_OK);
}
static int
hexit(int c)
{
    if ('0' <= c && c <= '9')
        return(c - '0');
    if ('A' <= c && c <= 'F')
        return(c - 'A' + 10);
    if ('a' <= c && c <= 'f')
        return(c - 'a' + 10);
    return(-1);
}

static int
ndn_name_last_component_offset(const unsigned char *ndnb, size_t size)
{
    struct ndn_buf_decoder decoder;
    struct ndn_buf_decoder *d = ndn_buf_decoder_start(&decoder, ndnb, size);
    int res = -1;
    if (ndn_buf_match_dtag(d, NDN_DTAG_Name)) {
        ndn_buf_advance(d);
        res = d->decoder.token_index; /* in case of 0 components */
        while (ndn_buf_match_dtag(d, NDN_DTAG_Component)) {
            res = d->decoder.token_index;
            ndn_buf_advance(d);
            if (ndn_buf_match_blob(d, NULL, NULL))
                ndn_buf_advance(d);
            ndn_buf_check_close(d);
        }
        ndn_buf_check_close(d);
    }
    return ((d->decoder.state >= 0) ? res : -1);
}



  static int
ndn_append_uri_component(struct ndn_charbuf *c, const char *s, size_t limit, size_t *cont)
{
    size_t start = c->length;
    size_t i;
    int err = 0;
    int hex = 0;
    int d1, d2;
    unsigned char ch;
    for (i = 0; i < limit; i++) {
        ch = s[i];
        switch (ch) {
            case 0:
            case '/':
            case '?':
            case '#':
                limit = i;
                break;
            case '=':
                if (hex || i + 3 > limit) {
                    return(-3);
                }
                hex = 1;
                break;
            case '%':
                if (hex || i + 3 > limit || (d1 = hexit(s[i+1])) < 0 ||
                    (d2 = hexit(s[i+2])) < 0   ) {
                    return(-3);
                }
                ch = d1 * 16 + d2;
                i += 2;
                ndn_charbuf_append(c, &ch, 1);
                break;
            case ':': case '[': case ']': case '@':
            case '!': case '$': case '&': case '\'': case '(': case ')':
            case '*': case '+': case ',': case ';':
                err++;
                /* FALLTHROUGH */
            default:
                if (ch <= ' ' || ch > '~')
                    err++;
                if (hex) {
                    if ((d1 = hexit(s[i])) < 0 || (d2 = hexit(s[i+1])) < 0) {
                        return(-3);
                    }
                    ch = d1 * 16 + d2;
                    i++;
                }
                ndn_charbuf_append(c, &ch, 1);
                break;
        }
    }
    for (i = start; i < c->length && c->buf[i] == '.'; i++)
        continue;
    if (i == c->length) {
        /* all dots */
        i -= start;
        if (i <= 1) {
            c->length = start;
            err = -2;
        }
        else if (i == 2) {
            c->length = start;
            err = -1;
        }
        else
            c->length -= 3;
    }
    if (cont != NULL)
        *cont = limit;
    return(err);
}
 
int
JDD_ndn_name_from_uri(struct ndn_charbuf *c, const char *uri)
{
    int res = 0;
    struct ndn_charbuf *compbuf = NULL;
    const char *stop = uri + strlen(uri);
    const char *s = uri;
    size_t cont = 0;

    compbuf = ndn_charbuf_create();
    if (compbuf == NULL) 
       {
         fprintf(stderr, "JDD_ndn_name_from_uri: returning because compbuf == NULL\n");
         return(-1);
       }
    if (s[0] != '/') {
        res = ndn_append_uri_component(compbuf, s, stop - s, &cont);
        if (res < -2)
            goto Done;
        ndn_charbuf_reserve(compbuf, 1)[0] = 0;
        if (s[cont-1] == ':') {
            if ((0 == strcasecmp((const char *)(compbuf->buf), "ndn:") ||
                 0 == strcasecmp((const char *)(compbuf->buf), "ndn:"))) {
                s += cont;
                cont = 0;
            } else {
                fprintf(stderr, "JDD_ndn_name_from_uri: returning because strcasecmp's failed\n");
                fprintf(stderr, "JDD_ndn_name_from_uri: compbuf->buf = %s\n", compbuf->buf);
                return (-1);
            }
        }
    }
    if (s[0] == '/') {
        ndn_name_init(c);
        if (s[1] == '/') {
            /* Skip over hostname part - not used in ndnx scheme */
            s += 2;
            compbuf->length = 0;
            res = ndn_append_uri_component(compbuf, s, stop - s, &cont);
            if (res < 0 && res != -2)
                goto Done;
            s += cont; cont = 0;
        }
    }
    while (s[0] != 0 && s[0] != '?' && s[0] != '#') {
        if (s[0] == '/')
            s++;
        compbuf->length = 0;
        res = ndn_append_uri_component(compbuf, s, stop - s, &cont);
        s += cont; cont = 0;
        if (res < -2)
            goto Done;
        if (res == -2) {
            res = 0; /* process . or equiv in URI */
            continue;
        }
        if (res == -1) {
            /* process .. in URI - discard last name component */
            res = ndn_name_last_component_offset(c->buf, c->length);
            if (res < 0)
                goto Done;
            c->length = res;
            ndn_charbuf_append_closer(c);
            continue;
        }
        res = ndn_name_append(c, compbuf->buf, compbuf->length);
        if (res < 0)
            goto Done;
    }
Done:
    ndn_charbuf_destroy(&compbuf);
    if (res < 0)
        return(-1);
    if (c->length < 2 || c->buf[c->length-1] != NDN_CLOSE)
        return(-1);
    return(s - uri);
}
 

int
main(int argc, char **argv)
{
    const char *progname = argv[0];
    struct ndn *ndn = NULL;
    int res;
    FILE *file = NULL;
    int num_lines = 0;
    char line[256];
    int i = 0;
    
    // set the callback for incoming interests.
    struct ndn_closure in_interest = {.p=&incoming_interest};

    strcpy(map_server_addr, "128.252.153.27");
    
    // Parse cmd-line arguments
    while ((res = getopt(argc, argv, "hn:f:s:")) != -1) {
        switch (res) {
	case 'f':
	  file = fopen(optarg, "r");
	  if (file == NULL)
	    {
	      printf("cannot open file %s ..\n", optarg);
	      usage(progname);
	    }
	  break;
	case 'n':
	  num_lines = atoi(optarg);
	  break;
	case 's':
	  strcpy(map_server_addr, optarg);
	  break;
	default:
	case 'h':
	  usage(progname);
	  break;
        }
    }
    if (num_lines < 1)
      usage(progname);
    num_link_entries = 0;
    // Connect to ndnd
    ndn = ndn_create();
    if (ndn_connect(ndn, NULL) == -1) {
        perror("Could not connect to ndnd");
        exit(1);
    }
    
    // Create the name for the prefixes we're interested in
    struct ndn_charbuf *name = NULL;
    char tmp_name[256];
     
    link_entries = malloc(sizeof(struct link_entry*)*num_lines);
    // Allocate some working space for storing content
    in_interest.data = ndn;
    name = ndn_charbuf_create();

    //open file and read ip pair, linkid
    for(i = 0; i < num_lines; ++i)
      {
	if (fgets(line, sizeof line, file) == NULL)
	  fprintf(stderr, "%s: num_lines was incorrect too large\n", progname);
	else
	  {
	    ndn_charbuf_reset(name);
	    if (DEBUG)
	      printf("  %s\n", line);
	    link_entries[i] = malloc(sizeof(struct link_entry));
	    sscanf(line, "%d %s %s", &link_entries[i]->id, link_entries[i]->sipaddr, link_entries[i]->fipaddr);
	    sprintf(tmp_name, "%s/%s/%s", MON_NAME_PREFIX,link_entries[i]->sipaddr, link_entries[i]->fipaddr);

	    
	    res = ndn_name_from_uri(name, tmp_name);
	    //res = JDD_ndn_name_from_uri(name, tmp_name);
	    if (res < 0)
	      {
		fprintf(stderr, "%s: bad ndn URI: %s\n", progname, tmp_name);
		exit(1);
	      }
	    // Set up a handler for incoming interests
	    res = ndn_set_interest_filter(ndn, name, &in_interest);
	    if (res < 0)
	      {
		printf("Failed to register interest for name %s (res == %d)\n", tmp_name, res);
		exit(1);
	      }
	    ++num_link_entries;
	  }
      }
    // Run a little while to see if there is anything there
    res = ndn_run(ndn, 200);
    
    // We got something; run until end of data or somebody kills us
    while (res >= 0 )
      {
	res = ndn_run(ndn, 333);
      }
    printf("exit client...\n");
    
    
    for (i = 0; i < num_link_entries; ++i)
      {
	free(link_entries[i]);
      }
    free(link_entries);
    ndn_destroy(&ndn);
    ndn_charbuf_destroy(&name);
    
    return EXIT_SUCCESS;
}




