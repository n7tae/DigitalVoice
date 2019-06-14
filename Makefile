# Copyright (c) 2019 by Thomas A. Early N7TAE

# choose this if you want debugging help
#CPPFLAGS=-g -ggdb -W -Wall -std=c++11 `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a much smaller executable without debugging help
CPPFLAGS=-W -Wall -std=c++11 `pkg-config --cflags gtkmm-3.0`

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

qnetdv :  $(OBJS)
	g++ $(CPPFLAGS) -o sgs $(OBJS) `pkg-config --libs gtkmm-3.0` -pthread

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY: clean

clean:
	$(RM) $(OBJS) $(DEPS) QnetDV

-include $(DEPS)

# install, uninstall and removehostfiles need root priviledges
