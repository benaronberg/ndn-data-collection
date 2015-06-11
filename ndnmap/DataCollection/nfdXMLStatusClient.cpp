/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014, Washington University in St. Louis,
 *
 */

#include <boost/asio.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/management/nfd-face-status.hpp>
#include <ndn-cxx/encoding/buffer-stream.hpp>


#define NUM_RECORDED_INTERESTS 2048
#define MON_NAME_PREFIX "ndn:/ndn/edu/wustl/ndnstatus"

// global variables to support xml parsing
int DEBUG = 0;

namespace ndn {
class NdnmapClient
{
public:
  
  struct interestData {
    char ipaddr[50];
    unsigned long tx;
    unsigned long rx;
    char currentTime[50];
  };
  
  NdnmapClient(char* programName)
  : m_programName(programName)
  , m_prefixName("ndn:/ndn/edu/wustl/ndnstatus")
  {
    m_myipaddr.clear();
    m_pollPeriod = 1;
    recordedInterestIndex = 0;
    
  }
  
  void
  usage()
  {
    std::cout << "\n Usage:\n " << m_programName << " [poll period] \n"
    ""
    " pull local nfd performace and send it as a prefix to a remote server.\n"
    "\n"
    " \t-h - print this message and exit\n"
    " \t-i <ipaddr> - host ip address (of this host) to be included in interests\n"
    " \t-d - set debug mode: 1 - debug on, 0- debug off (default)\n"
    "\n";
    exit(1);
  }
  
  void
  onData(const Interest& interest, Data& data)
  {
    std::cout << "onData" << std::endl;
  }
  
  void
  onTimeout(const Interest& interest)
  {
    std::cout << "onTimeout" << std::endl;
  }
  
  void
  initInterests()
  {
    recordedInterestIndex = 0;
  }
  void
  fetchSegments(const Data& data, const shared_ptr<OBufferStream>& buffer,
                void (NdnmapClient::*onDone)(const shared_ptr<OBufferStream>&))
  {
    buffer->write(reinterpret_cast<const char*>(data.getContent().value()),
                  data.getContent().value_size());
    
    uint64_t currentSegment = data.getName().get(-1).toSegment();
    
    const name::Component& finalBlockId = data.getMetaInfo().getFinalBlockId();
    if (finalBlockId.empty() ||
        finalBlockId.toSegment() > currentSegment)
    {
      m_face.expressInterest(data.getName().getPrefix(-1).appendSegment(currentSegment+1),
                             bind(&NdnmapClient::fetchSegments, this, _2, buffer, onDone),
                             bind(&NdnmapClient::onTimeout, this, _1));
      
    }
    else
    {
      return (this->*onDone)(buffer);
    }
  }
  void
  fetchFaceStatusInformation()
  {
    shared_ptr<OBufferStream> buffer = make_shared<OBufferStream>();
    
    Interest interest("/localhost/nfd/faces/list");
    interest.setChildSelector(1);
    interest.setMustBeFresh(true);
    
    m_face.expressInterest(interest,
                           bind(&NdnmapClient::fetchSegments, this, _2, buffer,
                                &NdnmapClient::afterFetchedFaceStatusInformation),
                           bind(&NdnmapClient::onTimeout, this, _1));
  }
  
  void
  recordInterest(std::string& ipaddr, uint64_t& rx, uint64_t& tx, std::string& time)
  {
    int i;
    
    for (i=0; i < recordedInterestIndex; i++)
    {
      if (!strcmp(recordedInterestData[i].ipaddr, ipaddr.c_str()))
      {
        // Found it!
        //printf("recordInterest: Found an already recorded ipaddr: %s\n", ipaddr);
        recordedInterestData[i].tx += tx;
        recordedInterestData[i].rx += rx;
        strcpy(recordedInterestData[i].currentTime, time.c_str());
        return;
      }
    }
    
    if (i < NUM_RECORDED_INTERESTS)
    {
      recordedInterestData[i].tx = tx;
      recordedInterestData[i].rx = rx;
      strcpy(recordedInterestData[i].currentTime, time.c_str());
      strcpy(recordedInterestData[i].ipaddr, ipaddr.c_str());
      recordedInterestIndex++;
    }
    else
    {
      printf("recordInterest: TOO MANY FACES!!!\n");
    }
    
  }
  void
  printInterests()
  {
    int i;
    for (i=0; i < recordedInterestIndex; i++)
    {
      printf("interest[%i]: %s %ld %ld %s\n", i, recordedInterestData[i].ipaddr ,recordedInterestData[i].tx ,recordedInterestData[i].rx , recordedInterestData[i].currentTime);
    }
  }

  
  void
  afterFetchedFaceStatusInformation(const shared_ptr<OBufferStream>& buffer)
  {
    ConstBufferPtr buf = buffer->buf();
    
    Block block;
    size_t offset = 0;
    
    std::string currentTime;
    std::tm ctime;
    std::stringstream realEpochTime;
    ndn::time::system_clock::TimePoint realCurrentTime = ndn::time::system_clock::now();
    std::string currentTimeStr = ndn::time::toString(realCurrentTime, "%Y-%m-%dT%H:%M:%S%F");
    
    strptime(currentTimeStr.c_str(), "%FT%T%Z", &ctime);
    std::string stime(currentTimeStr);
    std::time_t realEpochSeconds = std::mktime(&ctime);
    std::size_t pos = stime.find(".");
    std::string realEpochMilli = stime.substr(pos+1);
    realEpochTime << realEpochSeconds << "." << realEpochMilli;
    
    if (DEBUG)
    {
      std::cout << "'real' currentTime" << currentTimeStr << std::endl;;
      std::cout << "real epochTime: " << realEpochTime.str().c_str() << std::endl;
    }
    currentTime =  realEpochTime.str();

    
    while (offset < buf->size())
    {
      bool ok = Block::fromBuffer(buf, offset, block);
      if (!ok)
      {
        std::cerr << "ERROR: cannot decode FaceStatus TLV" << std::endl;
        break;
      }
      
      offset += block.size();
      
      nfd::FaceStatus faceStatus(block);
      
      // take only udp4 and tcp4 faces at the moment
      std::string remoteUri = faceStatus.getRemoteUri();
      
      if(remoteUri.compare(0,4,"tcp4") != 0 &&
         remoteUri.compare(0,4,"udp4") != 0)
        continue;
      
      // take the ip from uri (remove tcp4:// and everything after ':'
      std::size_t strPos = remoteUri.find_last_of(":");
      std::string remoteIp = remoteUri.substr(7,strPos - 7);
      
      uint64_t tx = faceStatus.getNOutBytes();
      uint64_t rx = faceStatus.getNInBytes();
      uint64_t faceId = faceStatus.getFaceId();
      
      if (DEBUG)
        std::cout << "about to store " << faceId << ": " << rx << ", " << tx << ", " << remoteIp << std::endl;
      
      recordInterest(remoteIp, rx, tx, currentTime);
    }

  }
  
  void
  sendRecordedInterests()
  {
    char tmp_name[512];
    int i;
    
    for (i=0; i < recordedInterestIndex; i++)
    {
      if (DEBUG)
        printf("sendRecordedInterests(): interest[%i]: %s %ld %ld %s\n", i, recordedInterestData[i].ipaddr ,
               recordedInterestData[i].tx ,
               recordedInterestData[i].rx ,
               recordedInterestData[i].currentTime);
      
      sprintf(tmp_name, "%s/%s/%s/%s/%ld/%ld", MON_NAME_PREFIX, m_myipaddr.c_str(),
              recordedInterestData[i].ipaddr,
              recordedInterestData[i].currentTime,
              recordedInterestData[i].tx,
              recordedInterestData[i].rx);
      
      if (DEBUG)
        printf("sendRecordedInterests(): tmp_name: %s\n", tmp_name);
      
      Interest interest(tmp_name);
      interest.setInterestLifetime(ndn::time::seconds(0));
      interest.setMustBeFresh(true);
      m_face.expressInterest(interest,
                             bind(&NdnmapClient::onData, this, _1, _2),
                             bind(&NdnmapClient::onTimeout, this, _1));
      m_face.processEvents();
      
    }
  }
  void
  run()
  {
    try
    {
      std::cout << m_programName <<  "polling every " << m_pollPeriod << " seconds" << std::endl;
      std::cout << "My IP is: " << m_myipaddr << std::endl;
      
      int res = 0;
      
      
      // debug vars
      time_t beforeStatusRequest;
      time_t afterStatusRequest;
      time_t beforeSleep;
      time_t afterSleep;
     
      while (res >= 0 )
      {
        initInterests();
        
        if (DEBUG)
        {
          beforeStatusRequest = std::time(NULL);
        }

        fetchFaceStatusInformation();
        m_face.processEvents();
        
        if (DEBUG)
        {
          afterStatusRequest = std::time(NULL);
        }
  
        if (DEBUG)
        {
          printf("ABOUT TO PRINT FACES LIST\n");
          printInterests();
        }
        sendRecordedInterests();
        
        
        if (DEBUG)
        {
          beforeSleep = std::time(NULL);
        }
        
        sleep(m_pollPeriod);
        
        if (DEBUG)
        {
          afterSleep = std::time(NULL);
        }
        
        if (DEBUG)
        {
          printf("before status request system time is %s",ctime(&beforeStatusRequest));
          printf("after status request system time is %s", ctime(&afterStatusRequest));
          printf("before sleep system time is %s",ctime(&beforeSleep));
          printf("after sleep system time is %s",ctime(&afterSleep));
        }
      }
  
      printf("exit client...\n");
      
    }
    catch (std::exception& e)
    {
      std::cerr << "ERROR: " << e.what() << "\n" << std::endl;
      exit(1);
    }
  }
  
  std::string&
  getProgramName()
  {
    return m_programName;
  }
  void
  setPollPeriod(int period)
  {
    m_pollPeriod = period;
  }
  
  int
  getPollPeriod()
  {
    return m_pollPeriod;
  }
  
  void
  setMyIP(char* myipaddr)
  {
    if(DEBUG)
      std::cout << "About to set IP to" << myipaddr << std::endl;
    
    m_myipaddr = myipaddr;
  }
public:
  std::string m_myipaddr;
  
private:
  
  std::string m_programName;
  std::string m_prefixName;
  int m_pollPeriod;
  Face m_face;
  
  
  struct interestData recordedInterestData[NUM_RECORDED_INTERESTS];
  int recordedInterestIndex;
};
} // namespace ndn

ndn::NdnmapClient ndnmapClient("ndnxmlstat_c");

int
main(int argc, char* argv[])
{
  int option;
  
  while ((option = getopt(argc, argv, "hi:d:")) != -1) {
    switch (option)
    {
      case 'i':
        ndnmapClient.setMyIP(optarg);
        break;
        
      case 'h':
        ndnmapClient.usage();
        break;
        
      case 'd':
        DEBUG = atoi(optarg);
        break;
        
      default:
        ndnmapClient.usage();
        break;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argv[0] != NULL)
    ndnmapClient.setPollPeriod(atoi(argv[0]));
  
  if(ndnmapClient.m_myipaddr.empty())
  {
    char myHostName[128];
    int res = gethostname(myHostName, 128);
    if (res != 0)
    {
      perror("Could not get my host name");
      exit(1);
    }
    struct hostent *addrResult;
    addrResult = gethostbyname(myHostName);
    if (addrResult == NULL)
    {
      perror("Could not get my host ipaddr: gethostbyname() failed");
      exit(1);
    }
    int i = 0;
    struct in_addr naddr;
    char* tmpStr;
    while(1)
    {
      if(addrResult->h_addr_list[i] == NULL) break;
      bzero((char*)&naddr, sizeof(naddr));
      bcopy((char*)addrResult->h_addr_list[i], (char*)&naddr.s_addr, addrResult->h_length);
      tmpStr = inet_ntoa(naddr);
      
      if (strncmp(tmpStr, "10.", 3) && strncmp(tmpStr, "127.0.", 6) && tmpStr[0] != '0')
      {
        if(DEBUG)
          std::cout << "About to set IP to" << tmpStr << std::endl;
        
        ndnmapClient.m_myipaddr = tmpStr;
        break;
      }
      ++i;
    }
  }
  if(ndnmapClient.m_myipaddr.empty())
  {
    std::cout << "Failed to set local IP adress. Exit." << std::endl;
    exit(0);
  }
  ndnmapClient.run();
  
  return 0;
}
