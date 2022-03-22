# Quadnet Digital Voice
*Quadnet Digital Voice* , qdv, is a fully functional, graphical repeater. It uses a *Northwest Digital Radio* **ThumbDV** and operates as a complete D-Star repeater, only there is no RF component. It can Link to reflectors or other repeaters and it can also do *routing*! It works best with a USB-based headset with microphone. qdv uses the default pulseaudio/ALSA input and output device, so for most versions of linux, all you need to do is plug your headset in and you should be ready to go.

## Building on a Raspberry Pi

Raspbian is not ready for *qdv* out of the box. While Raspbian has ALSA, it also needs pulseaudio:
```
sudo apt install -y pulseaudio pavucontrol paprefs
```

It's proabably a good idea to reboot after installing pulseaudio. After it's installed, take a look at the output of `aplay -L`. Near the beginning is should say:
```
default
    Playback/recording through the PulseAudio sound server
```

You'll also need to go to the ALSA audio configuration. For Debian Buster, this is found in main menu under **Preferences-->Audio Device Settings**. Select your headset in the drop-down list of devices, and then configure it and set it to be the default device. Set your speaker and microphone gain somewhere near the top. Once you build and configure *qdv*, you can use the Echo feature to help you set these gains adjust the speaker volume of you headset for a comfortable level and adjust the mic gain for the loudest playback without clipping. For my setup, the playback (speaker) was near 100% and the mic gain was at about 50%.


## Building and installing
There are several library requirements before you start:
```
sudo apt install -y git build-essential libgtkmm-3.0-dev libasound2-dev libsqlite3-dev
```

Then you can download and build qdv:
```
git clone https://github.com/n7tae/DigitalVoice.git
cd DigitalVoice
make
```
If it builds okay, then you can install it:
```
make install
```
All the configuration files are located in ~/etc and the qdv executable is in ~/bin. Please note that symbolic links are installed, so you can do a `make clean` to get rid of the intermediate object files, but don't delete the build directory unless you want to remove *qdv* from your system.

If you are planning on doing linking, you need host files. So do a:
```
make hostfile
```
If you want to override or provide another linking host definition provided by the Makefile, create a file called My_Hosts.txt. It will be append to the end of the gwys.txt file. The format for this file is each line contains the 6-character hostname, the IP address and the port number, separated by white space:
```
# comments begin with a hash mark
Q0XYZ 44.45.46.47 20001
```
If you have legacy D-Plus enabled, it is possible that an REF??? definition in you My_Hosts.txt file will be overwritten by the authorization process.

## Dashboard
DigitalVoice includes a web-based dashboard that uses the php mini-server. To start the mini-server:
```
sudo make installdash
```
This will automatically install packages needed for the mini-server. The mini-server can run in the background even if you are not using the DigitalVoice program. If it is not serving a browser client, it doesn't use much resources.

Once *qdv* is running, you can point your broswer to http://localhost, or http://*hostname*.local where *hostname* is, of course, the hostname where your running *qdv*.

If you want to turn off the mini-server:
```
sudo make uninstalldash
```

## Configuring qdv
Be sure to plug in you ThumbDV and your headset before starting qdv. Once your ready, open a shell and type `bin/qdv`. Most log messages will be displayed within the log window of qdv, but a few messages, especially if there are problems, may also be printed in the shell you launch qdv from.

Once it launches, click the **Settings** button and make sure the baud rate is set properly for you ThumbDV. Newer devices use 460800 and older devices use 230400. Set your callsign and name and your station callsign. You can set the station callsign to be the same as your callsign by clicking the check box. You should also set your 20 character message. Other fields are optional. Click the Okay button and your settings will be saved in your ~/.config/qdv directory.

Set you routing protocols. You internet provider must already supply IPv6 if you want to use IPv6 or Dual Stack, otherwise select IPv4.

For Linking, you can select a node (reflector or repeater) to automatically link when the linking thread starts. This node must be listed in one of the host files. The link button will only be activated after you have entered a valid target in the link entry window. This validation **does not** check to see if the module you are requesting is actually configured and operational on the target. If you want to link to something not listed in the host files, add it to your gwys.txt file:
```
# Here is an example
# this is not really needed becuase it probably already in the gwys.txt file
XRF757 107.191.121.105 30001
```
Be sure to specify the port number for the link: DExtra is 30001, DCS is 30051 and DPlus is 20001.

If you want to use the closed-source, legacy DPlus reflectors (REF) click the checkbox and your callsign will be validated. You need to be registered if you want to link to these systems.

**Please Note:** Unfortunately, qdv cannot tell if you callsign was successfully validiated or not. Also, data returned from the authorization host is highly variable depending on which authorization server the main round-robin server sends you to, so you may need to make appropriate entries in you MyHosts.txt file to insure you have a valid IP address for the reflector/repeater to which you wish to link. You can check the DPlus_Hosts.txt file to see if you favorite legacy systems are already listed.

## Operating
*qdv* can operate in linking or routing mode. Use the check boxes on the Settings dialog to enable or disable these modes. When you disable a mode, the thread operating the that mode will be shut down. If you are enabling *routing* mode, wait until you hear the "Welcome to Quadnet" message before you transmit anything. If *linking* mode is enabled *and* you have configured a start-up reflector, you should hear an announcement when you are linked.

There are three "transmitting" buttons on *qdv*:

1) **Echo Test** is a toggle switch. Click it to start recording an echo test and your voice will be transcoded to AMBE data and stored in memory. Note that the button will turn red when you are recording. Click the button again and *qdv* will decode the saved AMBE data and play it back to you. This is a great way to check the operation of you setup as well as the volume levels and make sure your ThumbDV is working properly. Note that *qdv* has no volume or gain controls for either TX or RX. You'll use your ALSA interface for this.

2) **PTT** the large PTT button is also toggle switch. Click it and it will turn red also and *qdv* will encode your audio and send it on a route or to a linked node. Click the button again to return *qdv* to receiving. Of course the YourCall sign will be a routing target if you are routing and "CQCQCQ  " if you are doing linking. Note that there are no linking or status "       I" commands as you don't need them linking and unlinking are done with buttons and the status command is unnecessary with the graphical interface.

3) **Quick Key** is a single press button that will send a 200 millisecond transmission. This is useful for initially subscribing or unsubscribing to/from a *routing group* and it is also useful when trying to get the attention of a Net Control Operator when you are participating in a Net.

de N7TAE (at) tearly (dot) net
