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
	/usr/bin/curl -o XLX_Hosts.txt https://ar-dns.net/xlx
	/usr/bin/wget http://www.pistar.uk/downloads/DExtra_Hosts.txt
	/usr/bin/wget http://www.pistar.uk/downloads/DPlus_Hosts.txt
	/usr/bin/wget http://www.pistar.uk/downloads/DCS_Hosts.txt

install : qdv
	mkdir -p $(CFG_DIR)
	ln -s $(shell pwd)/XLX_Hosts.txt $(CFG_DIR)
	ln -s $(shell pwd)/DExtra_Hosts.txt $(CFG_DIR)
	ln -s $(shell pwd)/DCS_Hosts.txt $(CFG_DIR)
	ln -s $(shell pwd)/DPlus_Hosts.txt $(CFG_DIR)
	ln -s $(shell pwd)/announce $(CFG_DIR)
	ln -s $(shell pwd)/DigitalVoice.glade $(CFG_DIR)
	mkdir -p $(BIN_DIR)
	ln -s $(shell pwd)/qdv $(BIN_DIR)

#interactive :
#	GTK_DEBUG=interactive ./qdv
