/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014, Washington University in St. Louis,
 *
 */

#include <boost/asio.hpp>
#include <ndn-cxx/face.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>


#define MON_NAME_PREFIX "ndn:/ndn/edu/wustl/ndnstatus"

int DEBUG = 0;
class ndnmapServer
{
public:
  struct faceStatus
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
  
  struct linkEntry
  {
    char sipaddr[50];
    char fipaddr[50];
    int id;
  };
  typedef struct linkEntry linkEntry;
  linkEntry** linkEntries;
  int numLinkEntries;
  
  ndnmapServer(char* programName)
  : m_programName(programName)
  {
      m_mapServerAddr = "128.252.153.27";
  }
  
  void
  usage()
  {
    std::cout << "\n Usage:\n " << m_programName <<
    ""
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
    "\n"
    "  -d - set debug mode: 1 - debug on, 0- debug off (default)\n" << std::endl;
    exit(1);
  }
  int
  get_link_id(char* sipaddr, char* fipaddr)
  {
    int i = 0;
    for (i = 0; i < numLinkEntries; ++i)
    {
      if (!strcmp(linkEntries[i]->sipaddr, sipaddr) && !strcmp(linkEntries[i]->fipaddr, fipaddr))
        return (linkEntries[i]->id);
    }
    return -1;
  }
  
  void
  onInterest(const ndn::Name& name, const ndn::Interest& interest)
  {
    char cmd_str[256];
    ndn::Name iName(interest.getName());
    std::string uri = iName.toUri();
    
    faceStatus fstatus;
    
    if (DEBUG)
    {
      std::cout << "recieved interest: " << uri << std::endl;
    }
    int nameLen = uri.size();

    // example of uri:  /ndn/edu/wustl/ndnstatus/192.168.1.2/192.168.1.3/2014-08-21T09%3A27%3A53.770000/0/0
    
    // getting rx
    int end = uri.size();
    int iComponent = uri.find_last_of("/", end);
    strncpy(fstatus.rx, (const char*)&uri[iComponent+1], end - iComponent-1);
    fstatus.rx[end - iComponent - 1] = '\0';
    fstatus.rxbits = (atol(fstatus.rx)) * 8;
	  if (DEBUG)
      printf("rx: %s\n", fstatus.rx);
    
    end = iComponent - 1;
    
    // getting tx
    iComponent = uri.find_last_of("/", end);
    strncpy(fstatus.tx, (const char*)&uri[iComponent+1], end - iComponent);
    fstatus.tx[end - iComponent] = '\0';
    fstatus.txbits = (atol(fstatus.tx)) * 8;
	  if (DEBUG)
      printf("tx: %s\n", fstatus.tx);
    
    end = iComponent - 1;
    
    // getting timestamp
    iComponent = uri.find_last_of("/", end);
    strncpy(fstatus.timestamp, (const char*)&uri[iComponent+1], end - iComponent);
    fstatus.timestamp[end - iComponent] = '\0';
	  if (DEBUG)
      printf("timestamp: %s\n", fstatus.timestamp);
    
    end = iComponent - 1;
    
    // getting fipaddr
    iComponent = uri.find_last_of("/", end);
    strncpy(fstatus.fipaddr, (const char*)&uri[iComponent+1], end - iComponent);
    fstatus.fipaddr[end - iComponent] = '\0';
	  if (DEBUG)
      printf("fipaddr: %s\n", fstatus.fipaddr);

    end = iComponent - 1;
    // getting sipaddr
    iComponent = uri.find_last_of("/", end);
    strncpy(fstatus.sipaddr, (const char*)&uri[iComponent+1], end - iComponent);
    fstatus.sipaddr[end - iComponent] = '\0';
	  if (DEBUG)
      printf("sipaddr: %s\n", fstatus.sipaddr);

    
    //printf("iComponent: %d, nameLen = %d\n", iComponent,nameLen);
    
    fstatus.id = get_link_id(fstatus.sipaddr, fstatus.fipaddr);
    if (fstatus.id >= 0)
    {
      sprintf(cmd_str, "http://%s/bw/%d/%s/%u/%u", m_mapServerAddr.c_str(), fstatus.id, fstatus.timestamp, fstatus.txbits, fstatus.rxbits);
      
      if (DEBUG)
        printf("cmd to pass to curl: %s\n", cmd_str);
      
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
  
  }
  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix (" <<
    reason << ")" << std::endl;
    m_face.shutdown();
  }
  void
  registerInterest(char* name)
  {
    if (DEBUG)
      std::cout << "register for prefix " << name << std::endl;
    
    // Set up a handler for incoming interests
    m_face.setInterestFilter(name,
                             ndn::bind(&ndnmapServer::onInterest, this, _1, _2),
                             ndn::RegisterPrefixSuccessCallback(),
                             ndn::bind(&ndnmapServer::onRegisterFailed, this, _1, _2));
  }
  void
  listen()
  {
    m_face.processEvents();
  }
  void
  setMapServerAddr(std::string & addr)
  {
    m_mapServerAddr = addr;
  }
private:
  ndn::Face m_face;
  std::string m_programName;
  std::string m_faceMgmtUri;
  std::string m_mapServerAddr;

  
};

int
main(int argc, char* argv[])
{
  ndnmapServer ndnmapServer(argv[0]);
  int option;
  FILE *file = NULL;
  int num_lines = 0;
  char line[256];
  int i = 0;
  

  // Parse cmd-line arguments
  while ((option = getopt(argc, argv, "hn:f:s:d:")) != -1)
  {
    switch (option)
    {
      case 'f':
        file = fopen(optarg, "r");
        if (file == NULL)
        {
          printf("cannot open file %s ..\n", optarg);
          ndnmapServer.usage();
        }
        break;
      case 'n':
        num_lines = atoi(optarg);
        break;
      case 's':
        ndnmapServer.setMapServerAddr((std::string&)(optarg));
        break;
      case 'd':
        DEBUG = atoi(optarg);
        break;
        
      default:
      case 'h':
        ndnmapServer.usage();
        break;
    }
  }
  if (num_lines < 1)
    ndnmapServer.usage();
  
  ndnmapServer.numLinkEntries = 0;
  
  char tmpName[256];
  
  ndnmapServer.linkEntries = (ndnmapServer::linkEntry**)malloc(sizeof(ndnmapServer::linkEntry*)*num_lines);
  
  //open file and read ip pair, linkid
  for(i = 0; i < num_lines; ++i)
  {
    if (fgets(line, sizeof line, file) == NULL)
      fprintf(stderr, "%s: num_lines was incorrect too large\n", argv[0]);
    else
	  {
	    if (DEBUG)
	      printf("  %s\n", line);
	    
      ndnmapServer.linkEntries[i] = (ndnmapServer::linkEntry*)malloc(sizeof(ndnmapServer::linkEntry));
	    sscanf(line, "%d %s %s", &ndnmapServer.linkEntries[i]->id, ndnmapServer.linkEntries[i]->sipaddr, ndnmapServer.linkEntries[i]->fipaddr);
      
	    sprintf(tmpName, "%s/%s/%s", MON_NAME_PREFIX, ndnmapServer.linkEntries[i]->sipaddr, ndnmapServer.linkEntries[i]->fipaddr);
      std::cout << "tmpName: " << tmpName << std::endl;
      ndnmapServer.registerInterest(tmpName);
      
      ++ndnmapServer.numLinkEntries;
	  }
  }
  
  // register a general prefix
  // TBD - understand why we need this one...
  ndnmapServer.registerInterest(MON_NAME_PREFIX);
  
  ndnmapServer.listen();
  
  printf("exit client...\n");

  for (i = 0; i < ndnmapServer.numLinkEntries; ++i)
  {
    free(ndnmapServer.linkEntries[i]);
  }
  free(ndnmapServer.linkEntries);
  
  return 0;
}
