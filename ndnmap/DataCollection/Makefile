# Copyright (c) 2009-2013 Charlie Wiseman, Jyoti Parwatikar, John DeHart 
# and Washington University in St. Louis
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
#    limitations under the License.
#

#CCNDIR = ../../ccnx
NDNDIR = /usr/local/

DEBUG = -g

CXXFLAGS = ${DEBUG} -g -Wall -Wpointer-arith -Wreturn-type -Wstrict-prototypes -D_REENTRANT -fPIC 
INCLUDES = -I. -I${NDNDIR}/include -I/usr/include/expat -I../database/include -I/opt/gurobi300/linux64/include

#LIBS = -lrt -lpthread -L../shared/${OBJDIR} -lONLshared -L../database/lib/${OBJDIR} -lonldb -L/usr/lib64/mysql -lmysqlclient_r -lmysqlpp -L/opt/gurobi300/linux64/lib -lgurobi_c++ -lgurobi30 -lssl
#LIBS = -lrt -lpthread -L../shared/${OBJDIR} -lONLshared -L../database/lib/${OBJDIR} -lonldb /usr/lib64/mysql/libmysqlclient_r.so /usr/lib64/libmysqlpp.so -L/opt/gurobi300/linux64/lib -lgurobi_c++ -lgurobi30 -lssl
#LIBS = -lrt -lpthread  -L../shared/${OBJDIR} -lONLshared -L../database/lib/${OBJDIR} -lonldb -L/usr/lib64/mysql -lmysqlclient_r -lmysqlpp -lssl -lcrypto -lz -lkrb5 -lk5crypto -ldl
LIBS = -L/usr/lib -lexpat -L${NDNDIR}/lib -lndn -lcrypto 

CLIENT_EXEC = ./ndnxmlstat_c
CLIENT_OBJS = ./ndnXMLStatusClient.o 
SRVR_EXEC = ./ndnxmlstat_s
SRVR_OBJS = ./ndnXMLStatusServer.o
OBJS = ${SRVR_OBJS} ${CLIENT_OBJS}

all: ${SRVR_OBJS} ${CLIENT_OBJS} ${CLIENT_EXEC} ${SRVR_EXEC}


$(SRVR_OBJS) : ./%.o : %.c
	cc ${CXXFLAGS} ${INCLUDES} -o $@ -c $<

$(CLIENT_OBJS) : ./%.o : %.c
	cc ${CXXFLAGS} ${INCLUDES} -o $@ -c $<

$(CLIENT_EXEC) : ./% : ${OBJS}
	cc -o $@ ${CLIENT_OBJS} ${LIBS}
	chmod 0755 $@

$(SRVR_EXEC) : ./% : ${OBJS}
	cc -o $@ ${SRVR_OBJS} ${LIBS}
	chmod 0755 $@

clean:
	rm -f ${SRVR_OBJS} ${CLIENT_OBJS} ${SRVR_EXEC} ${CLIENT_EXEC}
