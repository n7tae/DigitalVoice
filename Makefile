# Copyright (c) 2019 by Thomas A. Early N7TAE

# choose this if you want debugging help
CPPFLAGS=-g -ggdb -W -Wall -std=c++11 -Iircddb `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a much smaller executable without debugging help
#CPPFLAGS=-W -Wall -std=c++11 -Iircddb `pkg-config --cflags gtkmm-3.0`
IRC=ircddb

CFG_DIR = $(HOME)/.config/qdv
BIN_DIR = $(HOME)/bin

DSTROBJS = $(IRC)/dstar_dv.o $(IRC)/golay23.o
IRCOBJS = $(IRC)/IRCDDB.o $(IRC)/IRCClient.o $(IRC)/IRCReceiver.o $(IRC)/IRCMessageQueue.o $(IRC)/IRCProtocol.o $(IRC)/IRCMessage.o $(IRC)/IRCDDBApp.o $(IRC)/IRCutils.o $(DSTROBJS)
SRCS = $(wildcard *.cpp) $(wildcard $(IRC)/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

qdv :  $(OBJS) $(IRCOBJS)
	g++ $(CPPFLAGS) -o $@ $^ `pkg-config --libs gtkmm-3.0` -lasound -pthread

%.o : %.cpp DigitalVoice.glade
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY : clean

clean :
	$(RM) $(OBJS) $(DEPS)

-include $(DEPS)

hostfiles :
	/bin/rm -f {DExtra,DCS,Dplus,XLX}_Hosts.txt
	/usr/bin/curl -o XLX_Hosts.txt https://ar-dns.net/xlx && /bin/mv -f XLX_Hosts.txt $(CFG_DIR)
	/usr/bin/wget http://www.pistar.uk/downloads/DExtra_Hosts.txt && /bin/mv -f DExtra_Hosts.txt $(CFG_DIR)
	/usr/bin/wget http://www.pistar.uk/downloads/DCS_Hosts.txt && /bin/mv -f DCS_Hosts.txt $(CFG_DIR)
	/usr/bin/wget http://www.pistar.uk/downloads/DPlus_Hosts.txt && /bin/mv -f DPlus_Hosts.txt $(CFG_DIR)

install : qdv
	mkdir -p $(CFG_DIR)
	/bin/cp -f $(shell pwd)/XLX_Hosts.txt $(CFG_DIR)
	/bin/cp -f $(shell pwd)/DExtra_Hosts.txt $(CFG_DIR)
	/bin/cp -f $(shell pwd)/DCS_Hosts.txt $(CFG_DIR)
	/bin/cp -f $(shell pwd)/DPlus_Hosts.txt $(CFG_DIR)
	/bin/cp -rf $(shell pwd)/announce $(CFG_DIR)
	/bin/cp -f $(shell pwd)/DigitalVoice.glade $(CFG_DIR)
	mkdir -p $(BIN_DIR)
	/bin/cp -f $(shell pwd)/qdv $(BIN_DIR)

uninstall :
	/bin/rm -rf $(CFG_DIR)
	/bin/rm -f $(BIN_DIR)/qdv

#interactive :
#	GTK_DEBUG=interactive ./qdv
