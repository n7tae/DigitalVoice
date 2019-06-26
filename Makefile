# Copyright (c) 2019 by Thomas A. Early N7TAE

# choose this if you want debugging help
CPPFLAGS=-g -ggdb -W -Wall -std=c++11 `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a much smaller executable without debugging help
#CPPFLAGS=-W -Wall -std=c++11 `pkg-config --cflags gtkmm-3.0`

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

qndv :  $(OBJS)
	g++ $(CPPFLAGS) -o $@ $(OBJS) `pkg-config --libs gtkmm-3.0` -lasound -pthread

%.o : %.cpp DigitalVoice.glade
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY : clean

clean :
	$(RM) $(OBJS) $(DEPS) qndv

-include $(DEPS)

hostfiles :
	/usr/bin/wget http://www.pistar.uk/downloads/DExtra_Hosts.txt
	/usr/bin/wget http://www.pistar.uk/downloads/DPlus_Hosts.txt
	/usr/bin/wget http://www.pistar.uk/downloads/DCS_Hosts.txt

interactive :
	GTK_DEBUG=interactive ./qndv
