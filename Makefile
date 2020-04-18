# Copyright (c) 2019 by Thomas A. Early N7TAE
CFGDIR = $(HOME)/etc/
BINDIR = $(HOME)/bin/

# choose this if you want debugging help
CPPFLAGS=-g -ggdb -W -Wall -Wno-deprecated-declarations -std=c++11 -Iircddb -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a much smaller executable without debugging help
#CPPFLAGS=-W -Wall -Wno-deprecated-declarations -std=c++11 -Iircddb -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`


SRCS = $(wildcard *.cpp) $(wildcard ircddb/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

qdv :  $(OBJS)
	g++ $(CPPFLAGS) -o $@ $^ `pkg-config --libs gtkmm-3.0` -lasound -lsqlite3 -pthread

%.o : %.cpp DigitalVoice.glade
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY : clean

clean :
	$(RM) $(OBJS) $(DEPS)

-include $(DEPS)

hostfiles :
	wget -O XLX_Hosts.txt http://xlxapi.rlx.lu/api.php?do=GetXLXDMRMaster
	wget http://www.pistar.uk/downloads/DExtra_Hosts.txt
	wget http://www.pistar.uk/downloads/DPlus_Hosts.txt
	wget http://www.pistar.uk/downloads/DCS_Hosts.txt
	/bin/rm -f gwys.txt
	echo "# Downloaded from www.pistar.uk and xlxapi.rlx.lu `date`" > gwys.txt
	awk '$1 ~ /^XLX/ { gsub("\r", ""); printf "%s %s 30001\n", $1, $2 }' XLX_Hosts.txt >> gwys.txt
	awk '$1 ~ /^XRF/ { printf "%s %s 30001\n", $1, $2 }' DExtra_Hosts.txt >> gwys.txt
	awk '$1 ~ /^DCS/ { printf "%s %s 30051\n", $1, $2 }' DCS_Hosts.txt >> gwys.txt
	awk '$1 ~ /^REF/ { printf "%s %s 20001\n", $1, $2 }' DPlus_Hosts.txt >> gwys.txt
	/bin/rm -f {XLX,DExtra,DPlus,DCS}_Hosts.txt

install : qdv
	mkdir -p $(CFGDIR)
	/bin/cp -rf $(shell pwd)/announce $(CFGDIR)
	/bin/ln -f $(shell pwd)/DigitalVoice.glade $(CFGDIR)
	/bin/ln -f -s $(shell pwd)/gwys.txt $(CFGDIR)
	mkdir -p $(BINDIR)
	/bin/cp -f qdv $(BINDIR)

uninstall :
	/bin/rm -rf $(CFGDIR)
	/bin/rm -f $(BINDIR)qdv

#interactive :
#	GTK_DEBUG=interactive ./qdv
