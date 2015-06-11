#! /usr/bin/python

import os
import sys
#import argparse
import socket
import xml.parsers.expat
import time

def open_socket(host, port, socktp):
    sock = None
    addrinfo  = None
    for res in socket.getaddrinfo(host, port, socket.AF_UNSPEC, socktp, 0, \
                                  (socket.AI_ADDRCONFIG or socket.AI_NUMERICSERV)):
        addrinfo = res
        break
        #addrinfo[0], addrinfo[1], addrinfo[2], addrinfo[3], addrinfo[4] = res
        # returns tuple aifamily, socktype, proto, canonname, sa
    #print 'length addrinfo ' + repr(len(addrinfo)) + ' addrinfo:' + str(addrinfo)
    try:
        sock = socket.socket(addrinfo[0], addrinfo[1], addrinfo[2])
    except socket.error, msg:
        print 'failed to create socket'
        sock = None
    if sock != None:
        try:
            myaddrinfo = None
            #print 'getting myaddrinfo'
            #myaddrinfo = socket.getaddrinfo(None, 0,  addrinfo[0], socktp, 0, \
                    #                            (socket.AI_PASSIVE or socket.AI_NUMERICSERV))
            for res in socket.getaddrinfo(None, 0,  addrinfo[0], addrinfo[1], 0, \
                                                (socket.AI_PASSIVE or socket.AI_NUMERICSERV)):
                myaddrinfo = res
                break
            #print 'length myaddrinfo ' + repr(len(myaddrinfo)) + ' addrinfo:' + str(myaddrinfo)
            sock.bind(myaddrinfo[4])
            #print 'successful bind'
            sock.connect(addrinfo[4])
        except socket.error, msg:
            print 'failed to bind and connect:'
            sock.close()
            sock = None

    if sock == None:
        print 'could not open socket'
    return sock

class BWData:    
    def __init__(self):
        self.tx = float(0)
        self.rx = float(0)
        self.txdata = {'sz': 0, 'osz': 0}
        self.rxdata = {'sz': 0, 'osz': 0}
        self.lastBWCalc = float(0)
    def calculateBandwidth(self, time):
        """calculate the rx and tx bandwidth for a given time"""
        if self.lastBWCalc > 0:
           delta_time = time - self.lastBWCalc
           if delta_time <= 0:
               delta_time = 1
           self.tx = (self.txdata['sz'] - self.txdata['osz'])/delta_time
           self.txdata['osz'] = self.txdata['sz']
           self.rx = (self.rxdata['sz'] - self.rxdata['osz'])/delta_time
           self.rxdata['osz'] = self.rxdata['sz']
        else:
           #self.tx = self.txdata['sz'] - self.txdata['osz']
           self.txdata['osz'] = self.txdata['sz']
           #self.rx = self.rxdata['sz'] - self.rxdata['osz']
           self.rxdata['osz'] = self.rxdata['sz']
        self.lastBWCalc = time  
     
    def updateRX(self, sz):
        self.rxdata['sz'] = sz
    def updateTX(self, sz):
        self.txdata['sz'] = sz

    def getrxdata(self):
        rtn = repr(8*self.rxdata['sz'])
        return rtn

    def gettxdata(self):
        rtn = repr(8*self.txdata['sz'])
        return rtn

    def str(self):
        rtn = 'tx:' + repr(self.tx) + ' txcounter:' + repr(self.txdata['sz']) + \
              'rx:' + repr(self.rx) + ' rxcounter:' + repr(self.rxdata['sz'])
        return rtn

class FaceBandwidth:

    def __init__(self, fid):
        self.id = fid
        self.ip = None
        self.bandwidth = BWData()
        #self.time = None

    def calculateBandwidth(self, time):
        self.bandwidth.calculateBandwidth(time)

    def updateBytesIn(self, bw):
        """processes event and updates bytein counts as necessary"""
        self.bandwidth.updateRX(bw)

    def updateBytesOut(self, bw):
        """processes event and updates byteout counts as necessary"""
        self.bandwidth.updateTX(bw)

    def setIP(self, ip):
        self.ip = ip

    def rxdata(self):
        rtn = self.bandwidth.getrxdata()
        return rtn

    def txdata(self):
        rtn = self.bandwidth.gettxdata()
        return rtn

    def str(self):
        rx = float(self.bandwidth.rx * float(8)/float(1000))
        tx = float(self.bandwidth.tx * float(8)/float(1000))
        rtn = 'face ' + self.bandwidth.str() + repr(self.id) + ' ' + self.ip + ' ' + repr(rx) + \
              ' ' + repr(tx)
        return rtn


def startElement(name, attrs):
    global parserOn
    #print ' startElement:' + name + ' ' + repr(parserOn)
    if name == 'ccnd':
        parserOn = True
    if not parserOn:
        return
    global current_element
    current_element = name
    if name == 'bytein' or name == 'byteout':
        global bandwidth_type
        bandwidth_type = name

def endElement(name):
    global current_face
    global current_element
    global ALL_Faces_ids
    #print ' endElement:' + name
    if name == 'face' or name == 'ccnd':
        if current_face:
            current_face.calculateBandwidth(current_time)
        current_face = None
        current_element = None
    if name == 'bytein' or name == 'byteout':
        global bandwidth_type
        bandwidth_type = None 
    if name == 'faces':
        global parserOn
        parserOn = False
        current_face = None
        current_element = None

def char_data(data):
    if not parserOn:
        return
    #print ' char_data:' + current_element + ' ' + data
    global current_face
    if current_face and bandwidth_type and current_element == 'total':
        if bandwidth_type == 'bytein':
            current_face.updateBytesIn(int(data))
        else:
            current_face.updateBytesOut(int(data))
    elif current_element == 'faceid':
        global ccndstatus
        #global current_face
        face = ccndstatus.getFace(int(data))
        if (current_face):
           print ' error in parsing received new face before end of old one'
        current_face = face
    elif current_element == 'now':
        global current_time
        current_time = float(data)
    elif current_element == 'ip' and current_face:
        #use regular expression to find x.y.z.a:port where !(x = y = z = a = 0)
        c = data[0]
        if c != '[' and c !='0':
            current_face.setIP(data)

class CCNdStatus:   
    def __init__(self, host, port):
        self.face_list = []
        self.host = host
        self.port = port
    def getFace(self, face):
        """gets bandwidth entry associated with a face"""
        for entry in self.face_list:
            if entry.id == face:
                return entry
        rtn = FaceBandwidth(face)
        self.face_list.append(rtn)
        return rtn
    def getStats(self, idList):        
        sock = open_socket(self.host, self.port, socket.SOCK_STREAM)
        if sock:
            res = sock.send("GET /?f=xml \r\n")
            sock.settimeout(1)
            #parser = xml.parsers.expat.ParserCreate()
            #parser.StartElementHandler = startElement
            #parser.EndElementHandler = endElement
            #parser.CharacterDataHandler = char_data
            xmldata = ''
            while res > 0:
                recv_str = sock.recv(4096)
                res = len(recv_str)
                if res > 0:
                    #print recv_str + '\n\n\n'
                    xmldata = xmldata + recv_str
                    
                    #if recv_str.startswith('HTTP'):
                     #   tmp = recv_str.split('\n')
                      #  print 'num lines:' + repr(len(tmp))
                    #else:
                       # parser.Parse(recv_str)
                    #self.parse(recv_str)
                else:
                    res = 0
            if len(xmldata) > 0:
                self.parse(xmldata)
            sock.close()
        self.printout(idList)

    def parse(self, data):
        # print data + '\n\n\n'
        tmp = data.split('\n')
        #print 'parse' #num lines:' + repr(len(tmp))
        xmldata = ''
        for d in tmp[5:]:
            xmldata = xmldata + d
        #print xmldata + '\n\n\n'
        tmp = xmldata.split('\n')
        #print 'num lines:' + repr(len(tmp))
        parser = xml.parsers.expat.ParserCreate()
        parser.StartElementHandler = startElement
        parser.EndElementHandler = endElement
        parser.CharacterDataHandler = char_data
        parser.Parse(xmldata)

    def printout(self, idList):
        #print 'ccnd (' + self.host + ':' + repr(self.port) + ') ' + repr(current_time)
        #print 'idList:' + repr(idList)
        for entry in self.face_list:
            if entry.ip:
                #print '    ' + entry.str()
                try:
                  if idList.count(ALL_Faces_ids[entry.ip]) != 0:
                    # ALL_Faces_id 15 is for the WU to KANS link
                    curlCmd = '/usr/bin/curl -s -L http://128.252.153.27/bw/' + repr(ALL_Faces_ids[entry.ip]) + '/' + repr(current_time) + '/' + entry.txdata() + '/' +entry.rxdata()
                    #print 'curlCmd: ' + curlCmd
                    os.system(curlCmd)
                    #if ALL_Faces_ids[entry.ip] == 15:
                    #  print 'WU KANS Link'
                    #  os.system(curlCmd)
                    #os.system("CMD")

                    #print 'entry.bandwidth.str(): ' + entry.bandwidth.str() 
                    #print 'entry.txdata(): ' + entry.txdata()
                    #print 'entry.bandwidth.txdata(): ' + entry.bandwidth.txdata()
                except :
                  pass
            

#aparser = argparse.ArgumentParser(description="query remote ccnd to get status in xml")
#aparser.add_argument("-p", help="port of remote ccnd defaults to 9695")
#aparser.add_argument("host", help="host of remote ccnd")
#aparser.parse_args()

def getUCLA():
    global ccndstatus
    ids = [9,10,11,18,24,34]
    host='spurs.cs.ucla.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getUCLA() got an exception during getStats()'

def getCSU():
    global ccndstatus
    ids = [14,23,26,27,29]
    host='mccoy.netsec.colostate.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getCSU() got an exception during getStats()'

def getWashU():
    global ccndstatus
    ids = [15,30,31]
    host='momcat.arl.wustl.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getWashU() got an exception during getStats()'

def getRemap():
    global ccndstatus
    ids = [19,20]
    host='borges.metwi.ucla.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getRemap() got an exception during getStats()'

def getUIUC():
    global ccndstatus
    ids = [16,22,32]
    host='ndn.cs.illinois.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getUIUC() got an exception during getStats()'

def getUCSD():
    global ccndstatus
    ids = [21,25]
    host='ccnhub.caida.org'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getUCSD() got an exception during getStats()'

def getSALT():
    global ccndstatus
    ids = [1]
    host='sppsalt1.arl.wustl.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getSALT() got an exception during getStats()'

def getKANS():
    global ccndstatus
    ids = [2,6,7]
    host='sppkans1.arl.wustl.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getKANS() got an exception during getStats()'

def getWASH():
    global ccndstatus
    ids = [8,17]
    host='sppwash1.arl.wustl.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getWASH() got an exception during getStats()'

#def getATLA():
#    global ccndstatus
#    ids = [5,13]
#    host='sppatla1.arl.wustl.edu'
#    port=9695
#    ccndstatus = CCNdStatus(host, port)
#    try:
#      ccndstatus.getStats(ids)
#    except :
#      print 'getATLA() got an exception during getStats()'

def getUM():
    global ccndstatus
    ids = [13,28,33]
    host='netlogic.cs.memphis.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getUM() got an exception during getStats()'

def getHOUS():
    global ccndstatus
    ids = [3,4,5,12]
    host='spphous1.arl.wustl.edu'
    port=9695
    ccndstatus = CCNdStatus(host, port)
    try:
      ccndstatus.getStats(ids)
    except :
      print 'getHOUS() got an exception during getStats()'


current_time = None
current_element = None
current_face = None
bandwidth_type = None
parserOn = False
ccndstatus = None
host = None

port = 9695
ids=None
WU_Faces_ids= { '10.0.9.6:9695'  : 15, 
                '10.0.4.37:9695' : 27, 
                '10.0.8.37:9695' : 30, 
                '10.0.2.37:9695' : 31 }

ALL_Faces_ids= { 
                '10.0.9.33:9695' :  1,
                '10.0.9.34:9695' :  1,
                '10.0.9.13:9695' :  2,
                '10.0.9.14:9695' :  2,
                '10.0.9.65:9695' :  3,
                '10.0.9.66:9695' :  3,
                '10.0.9.61:9695' :  4,
                '10.0.9.62:9695' :  4,
                '10.0.9.69:9695' :  5,
                '10.0.9.70:9695' :  5,
                '10.0.9.21:9695' :  6,
                '10.0.9.22:9695' :  6,
                '10.0.9.25:9695' :  7,
                '10.0.9.26:9695' :  7,
                '10.0.9.53:9695' :  8,
                '10.0.9.54:9695' :  8,
                '10.0.3.21:9695' :  9,
                '10.0.3.22:9695' :  9,
                '10.0.3.25:9695' : 10,
                '10.0.3.26:9695' : 10,
                '10.0.3.29:9695' : 11,
                '10.0.3.30:9695' : 11,
                '10.0.9.41:9695' : 12,
                '10.0.9.42:9695' : 12,
                '10.0.9.49:9695' : 13,
                '10.0.9.50:9695' : 13,
                '10.0.9.29:9695' : 14,
                '10.0.9.30:9695' : 14,
                '10.0.9.5:9695'  : 15, 
                '10.0.9.6:9695'  : 15, 
                '10.0.9.9:9695'  : 16,
                '10.0.9.10:9695'  : 16,
                '10.0.9.57:9695'  : 17,
                '10.0.9.58:9695'  : 17,
                '10.0.3.1:9695'  : 18,
                '10.0.3.2:9695'  : 18,
                '10.0.5.1:9695' : 19,
                '10.0.5.2:9695' : 19,
                '10.0.6.1:9695' : 20,
                '10.0.6.2:9695' : 20,
                '10.0.6.29:9695' : 21,
                '10.0.6.30:9695' : 21,
                '10.0.5.33:9695' : 22,
                '10.0.5.34:9695' : 22,
                '10.0.4.21:9695' : 23,
                '10.0.4.22:9695' : 23,
                '10.0.1.13:9695' : 24,
                '10.0.1.14:9695' : 24,
                '10.0.1.29:9695' : 25, 
                '10.0.1.30:9695' : 25, 
                '10.0.4.5:9695' : 26, 
                '10.0.4.6:9695' : 26, 
                '10.0.4.37:9695' : 27, 
                '10.0.4.38:9695' : 27, 
                '10.0.1.9:9695' : 28, 
                '10.0.1.10:9695' : 28, 
                '10.0.4.9:9695' : 29, 
                '10.0.4.10:9695' : 29, 
                '10.0.8.37:9695' : 30, 
                '10.0.8.38:9695' : 30, 
                '10.0.2.37:9695' : 31,
                '10.0.2.38:9695' : 31,
                '10.0.8.41:9695' : 32,
                '10.0.8.42:9695' : 32,
                '10.0.2.41:9695' : 33,
                '10.0.2.42:9695' : 33,
                '10.0.4.1:9695'  : 34,
                '10.0.4.2:9695'  : 34 
               }

UCLA_Faces_ids= { '10.0.1.13:9695' : 24,
                  '10.0.3.30:9695' : 11,
                  '10.0.3.26:9695' : 10,
                  '10.0.3.2:9695'  : 18,
                  '10.0.3.22:9695' :  9,
                  '10.0.9.33:9695' :  1,
                  '10.0.4.1:9695'  : 34 }
           
while True:
  getWashU()
  time.sleep(1)

#for i in range(len(sys.argv)):
#    if sys.argv[i] == '-p':
#        port = int(sys.argv[i+1])
#        i = i + 1
#    else:
#        host = sys.argv[i]
#
#if host:
#    #global ccndstatus
#    ccndstatus = CCNdStatus(host, port)
#    while True:
#        ccndstatus.getStats()
#        print ''
#        time.sleep(1)

#    sock = open_socket(host, port, socket.SOCK_STREAM)
#    if sock:
#        res = sock.send("GET /?f=xml \r\n")
#        sock.settimeout(1)
#        while res > 0:
#            recv_str = sock.recv(4096)
#            res = len(recv_str)
#            if res > 0:
#                print recv_str
#            else:
#                res = 0
#        sock.close()
   
