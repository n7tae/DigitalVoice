/*
 *   Copyright (c) 2019 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <iostream>
#include <sstream>
#include <alsa/asoundlib.h>

#include "AudioManager.h"
#include "WaitCursor.h"
#include "DPlusAuthenticator.h"
#include "SettingsDlg.h"

// globals
//extern Glib::RefPtr<Gtk::Application> theApp;
extern CAudioManager AudioManager;
extern CConfigure cfg;
//extern bool GetCfgDirectory(std::string &path);
extern CHostFile gwys;

CSettingsDlg::CSettingsDlg() :
	pDlg(nullptr)
{
	CallRegEx = std::regex("^(([1-9][A-Z])|([A-Z][0-9])|([A-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][ ]*[ A-RT-Z]$", std::regex::extended);
	std::string path;
}

CSettingsDlg::~CSettingsDlg()
{
	if (pDlg)
		delete pDlg;
}

CFGDATA *CSettingsDlg::Show()
{
	cfg.CopyTo(data);	// get the saved config data (MainWindow has alread read it)
	SetWidgetStates(data);
	on_AMBERescanButton_clicked();		// reset the ambe device
	on_AudioRescanButton_clicked();	// re-read the audio PCM devices

	if (Gtk::RESPONSE_OK == pDlg->run()) {
		CFGDATA newstate;			// the user clicked okay, time to look at what's changed
		SaveWidgetStates(newstate); // newstate is now the current contents of the Settings Dialog
		cfg.CopyFrom(newstate);		// and it is now in the global cfg object
		cfg.WriteData();			// and it's saved in ~/.config/qdv/qdv.cfg

		// reconfigure current environment if anything changed
		if (data.bDPlusEnable != newstate.bDPlusEnable)
			RebuildGateways(newstate.bDPlusEnable);
		if (data.iBaudRate != newstate.iBaudRate)
			BaudrateChanged(newstate.iBaudRate);
		cfg.CopyTo(data);	// we need to return to the MainWindow a pointer to the new state
		pDlg->hide();
		return &data;		// changes to the irc client will be done in the MainWindow
	}
	pDlg->hide();
	return nullptr;
}

void CSettingsDlg::SaveWidgetStates(CFGDATA &d)
{
	// station
	d.sCallsign.assign(pMyCallsignEntry->get_text());
	d.sName.assign(pMyNameEntry->get_text());
	d.bUseMyCall = pUseMyCallCheckButton->get_active();
	d.sStation = pStationCallsignEntry->get_text();
	d.sMessage.assign(pMessageEntry->get_text());
	d.cModule = data.cModule;
	d.sURL.assign(pURLEntry->get_text());
	// location
	d.sLocation[0].assign(pLocationEntry[0]->get_text());
	d.sLocation[1].assign(pLocationEntry[1]->get_text());
	d.dLatitude = std::stod(pLatitudeEntry->get_text());
	d.dLongitude = std::stod(pLongitudeEntry->get_text());
	d.bAPRSEnable = pAPRSEnableCheckButton->get_active();
	d.sAPRSServer.assign(pAPRSServerEntry->get_text());
	d.usAPRSPort = std::stoul(pAPRSPortEntry->get_text());
	d.iAPRSInterval = std::stoi(pAPRSIntervalEntry->get_text());
	d.bGPSDEnable = pGPSDEnableCheckButton->get_active();
	d.sGPSDServer.assign(pGPSDServerEntry->get_text());
	d.usGPSDPort = std::stoul(pGPSDPortEntry->get_text());
	//link
	d.sLinkAtStart.assign(pLinkAtStartEntry->get_text());
	d.bDPlusEnable = pDPlusEnableCheckButton->get_active();
	// quadnet
	if (pIPv6OnlyRadioButton->get_active())
		d.eNetType = EQuadNetType::ipv6only;
	else if (pDualStackRadioButton->get_active())
		d.eNetType = EQuadNetType::dualstack;
	else if (pNoRoutingRadioButton->get_active())
		d.eNetType = EQuadNetType::norouting;
	else
		d.eNetType = EQuadNetType::ipv4only;
	// device
	d.iBaudRate = (p230kRadioButton->get_active()) ? 230400 : 460800;
	// audio
	Gtk::ListStore::iterator it = pAudioInputComboBox->get_active();
	Gtk::ListStore::Row row = *it;
	Glib::ustring s = row[audio_columns.name];
	d.sAudioIn.assign(s.c_str());
	it = pAudioOutputComboBox->get_active();
	row = *it;
	s = row[audio_columns.name];
	d.sAudioOut.assign(s.c_str());
}

void CSettingsDlg::SetWidgetStates(const CFGDATA &d)
{
	// station
	pMyCallsignEntry->set_text(d.sCallsign);
	pMyNameEntry->set_text(d.sName);
	pStationCallsignEntry->set_text(d.sStation);
	pUseMyCallCheckButton->set_active(d.bUseMyCall);
	pMessageEntry->set_text(d.sMessage);
	pURLEntry->set_text(d.sURL);
	// location
	pLocationEntry[0]->set_text(d.sLocation[0]);
	pLocationEntry[1]->set_text(d.sLocation[1]);
	pLatitudeEntry->set_text(std::to_string(d.dLatitude));
	pLongitudeEntry->set_text(std::to_string(d.dLongitude));
	pAPRSEnableCheckButton->set_active(d.bAPRSEnable);
	pAPRSIntervalEntry->set_text(std::to_string(d.iAPRSInterval));
	pAPRSServerEntry->set_text(d.sAPRSServer);
	pAPRSPortEntry->set_text(std::to_string(d.usAPRSPort));
	pGPSDEnableCheckButton->set_active(d.bGPSDEnable);
	pGPSDServerEntry->set_text(d.sGPSDServer);
	pGPSDPortEntry->set_text(std::to_string(d.usGPSDPort));
	//link
	pLinkAtStartEntry->set_text(d.sLinkAtStart);
	if (d.bDPlusEnable != pDPlusEnableCheckButton->get_active())
		pDPlusEnableCheckButton->set_active(d.bDPlusEnable);	// only do this if we need to
	//quadnet
	switch (d.eNetType) {
		case EQuadNetType::ipv6only:
			pIPv6OnlyRadioButton->set_active();
			break;
		case EQuadNetType::dualstack:
			pDualStackRadioButton->set_active();
			break;
		case EQuadNetType::norouting:
			pNoRoutingRadioButton->set_active();
			break;
		default:
			pIPv4OnlyRadioButton->set_active();
			break;
	}
	//device
	if (230400 == d.iBaudRate)
		p230kRadioButton->clicked();
	else
		p460kRadioButton->clicked();
}

bool CSettingsDlg::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name, Gtk::Window *pWin)
{
	bCallsign = bStation = false;
	builder->get_widget(name, pDlg);
	if (nullptr == pDlg) {
		std::cerr << "Failed to initialize SettingsDialog!" << std::endl;
		return true;
	}
	pDlg->set_transient_for(*pWin);
	pDlg->add_button("Cancel", Gtk::RESPONSE_CANCEL);
	pOkayButton = pDlg->add_button("Okay", Gtk::RESPONSE_OK);
	pOkayButton->set_sensitive(false);

	// station
	builder->get_widget("MyCallsignEntry", pMyCallsignEntry);
	builder->get_widget("MyNameEntry", pMyNameEntry);
	builder->get_widget("UseMyCallsignCheckButton", pUseMyCallCheckButton);
	builder->get_widget("StationCallsignEntry", pStationCallsignEntry);
	builder->get_widget("MessageEntry", pMessageEntry);
	builder->get_widget("URLEntry", pURLEntry);
	// location
	builder->get_widget("Location1Entry", pLocationEntry[0]);
	builder->get_widget("Location2Entry", pLocationEntry[1]);
	builder->get_widget("LatitudeEntry", pLatitudeEntry);
	builder->get_widget("LongitudeEntry", pLongitudeEntry);
	builder->get_widget("APRSEnableCheckButton", pAPRSEnableCheckButton);
	builder->get_widget("APRSIntervalEntry", pAPRSIntervalEntry);
	builder->get_widget("APRSServerEntry", pAPRSServerEntry);
	builder->get_widget("APRSPortEntry", pAPRSPortEntry);
	builder->get_widget("GPSDEnableCheckButton", pGPSDEnableCheckButton);
	builder->get_widget("APRSServerEntry", pAPRSServerEntry);
	builder->get_widget("APRSPortEntry", pAPRSPortEntry);
	// AMBE device
	builder->get_widget("DeviceLabel", pDevicePathLabel);
	builder->get_widget("ProductIDLabel", pProductIDLabel);
	builder->get_widget("VersionLabel", pVersionLabel);
	builder->get_widget("Baud230kRadioButton", p230kRadioButton);
	builder->get_widget("Baud460kRadioButton", p460kRadioButton);
	builder->get_widget("AMBERescanButton", pAMBERescanButton);
	// linking
	builder->get_widget("LinkAtStartEntry", pLinkAtStartEntry);
	builder->get_widget("LegacyCheckButton", pDPlusEnableCheckButton);
	// QuadNet
	builder->get_widget("IPV4_RadioButton", pIPv4OnlyRadioButton);
	builder->get_widget("IPV6_RadioButton", pIPv6OnlyRadioButton);
	builder->get_widget("Dual_Stack_RadioButton", pDualStackRadioButton);
	builder->get_widget("No_Routing_RadioButton", pNoRoutingRadioButton);
	// Audio
	builder->get_widget("AudioInputComboBox", pAudioInputComboBox);
	refAudioInListModel = Gtk::ListStore::create(audio_columns);
	pAudioInputComboBox->set_model(refAudioInListModel);
	pAudioInputComboBox->pack_start(audio_columns.short_name);

	builder->get_widget("AudioOutputComboBox", pAudioOutputComboBox);
	refAudioOutListModel = Gtk::ListStore::create(audio_columns);
	pAudioOutputComboBox->set_model(refAudioOutListModel);
	pAudioOutputComboBox->pack_start(audio_columns.short_name);

	builder->get_widget("InputDescLabel", pInputDescLabel);
	builder->get_widget("OutputDescLabel", pOutputDescLabel);
	builder->get_widget("AudioRescanButton", pAudioRescanButton);
	// APRS
	builder->get_widget("APRSEnableCheckButton", pAPRSEnableCheckButton);
	builder->get_widget("APRSIntervalEntry", pAPRSIntervalEntry);
	builder->get_widget("APRSServerEntry", pAPRSServerEntry);
	builder->get_widget("APRSPortEntry", pAPRSPortEntry);
	// GPSD
	builder->get_widget("GPSDEnableCheckButton", pGPSDEnableCheckButton);
	builder->get_widget("GPSDServerEntry", pGPSDServerEntry);
	builder->get_widget("GPSDPortEntry", pGPSDPortEntry);

	pMyCallsignEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyCallsignEntry_changed));
	pMyNameEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyNameEntry_changed));
	pStationCallsignEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_StationCallsignEntry_changed));
	pUseMyCallCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_UseMyCallsignCheckButton_toggled));
	pMessageEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MessageEntry_changed));
	pLocationEntry[0]->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_Location1Entry_changed));
	pLocationEntry[1]->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_Location2Entry_changed));
	pLatitudeEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LatitudeEntry_changed));
	pLongitudeEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LongitudeEntry_changed));
	pURLEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_URLEntry_changed));
	pIPv4OnlyRadioButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pIPv6OnlyRadioButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pDualStackRadioButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pNoRoutingRadioButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pAMBERescanButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AMBERescanButton_clicked));
	pAudioRescanButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AudioRescanButton_clicked));
	pLinkAtStartEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LinkAtStartEntry_changed));
	pAudioInputComboBox->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AudioInputComboBox_changed));
	pAudioOutputComboBox->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AudioOutputComboBox_changed));
	pAPRSEnableCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_APRSEnableCheckButton_toggled));
	pGPSDEnableCheckButton->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_GPSDEnableCheckButton_toggled));

	return false;
}

void CSettingsDlg::RebuildGateways(bool includelegacy)
{
	gwys.Init();

	if (includelegacy) {
		CWaitCursor wait;
		const std::string call(pMyCallsignEntry->get_text().c_str());
		CDPlusAuthenticator auth(call, "auth.dstargateway.org");
		if (auth.Process(gwys.hostmap, true, false)) {
			std::cerr << "Error processing D-Plus authorization" << std::endl;
		}
	}
}

void CSettingsDlg::on_AMBERescanButton_clicked()
{
	CWaitCursor wait;
	if (AudioManager.AMBEDevice.IsOpen())
		AudioManager.AMBEDevice.CloseDevice();

	AudioManager.AMBEDevice.FindandOpen(data.iBaudRate, DSTAR_TYPE);
	if (AudioManager.AMBEDevice.IsOpen()) {
		const Glib::ustring path(AudioManager.AMBEDevice.GetDevicePath());
		pDevicePathLabel->set_text(path);
		const Glib::ustring prodid(AudioManager.AMBEDevice.GetProductID());
		pProductIDLabel->set_text(prodid);
		const Glib::ustring version(AudioManager.AMBEDevice.GetVersion());
		pVersionLabel->set_text(version);
	} else {
		pDevicePathLabel->set_text("Not found!");
		pProductIDLabel->set_text("Is the device plugged in?");
		pVersionLabel->set_text("Are you in the 'dialout' group?");
	}
}

void CSettingsDlg::on_APRSEnableCheckButton_toggled()
{
	bool checked = pAPRSEnableCheckButton->get_active();
	pAPRSIntervalEntry->set_sensitive(checked);
	pAPRSServerEntry->set_sensitive(checked);
	pAPRSPortEntry->set_sensitive(checked);
	if (! checked && pGPSDEnableCheckButton->get_active())
		pGPSDEnableCheckButton->set_active(false);
	pGPSDEnableCheckButton->set_sensitive(checked);
}

void CSettingsDlg::on_GPSDEnableCheckButton_toggled()
{
	bool checked = pGPSDEnableCheckButton->get_active();
	pGPSDServerEntry->set_sensitive(checked);
	pGPSDPortEntry->set_sensitive(checked);
}

void CSettingsDlg::on_UseMyCallsignCheckButton_toggled()
{
	if (pUseMyCallCheckButton->get_active()) {
		pStationCallsignEntry->hide();
		pStationCallsignEntry->set_text(pMyCallsignEntry->get_text());
	} else {
		pStationCallsignEntry->show();
	}
}

void CSettingsDlg::on_MyCallsignEntry_changed()
 {
	 int pos = pMyCallsignEntry->get_position();
	 Glib::ustring s = pMyCallsignEntry->get_text().uppercase();
	 pMyCallsignEntry->set_text(s);
	 pMyCallsignEntry->set_position(pos);
	 bCallsign = std::regex_match(s.c_str(), CallRegEx);
	 pMyCallsignEntry->set_icon_from_icon_name(bCallsign ? "gtk-ok" : "gtk-cancel");
	 pOkayButton->set_sensitive(bCallsign && bStation);
	 if (pUseMyCallCheckButton->get_active())
	 	pStationCallsignEntry->set_text(s);
 }

void CSettingsDlg::on_MyNameEntry_changed()
 {
	 int pos = pMyNameEntry->get_position();
	 Glib::ustring s = pMyNameEntry->get_text();
	 pMyNameEntry->set_text(s.uppercase());
	 pMyNameEntry->set_position(pos);
 }

 void CSettingsDlg::on_LinkAtStartEntry_changed()
 {
	 int pos = pLinkAtStartEntry->get_position();
	 Glib::ustring s = pLinkAtStartEntry->get_text().uppercase();
	 const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	 Glib::ustring n;
	 for (auto it=s.begin(); it!=s.end(); it++) {
		 if (Glib::ustring::npos != good.find(*it)) {
			 n.append(1, *it);
		 }
	 }
	 pLinkAtStartEntry->set_text(n);
	 pLinkAtStartEntry->set_position(pos);
 }

void CSettingsDlg::on_StationCallsignEntry_changed()
 {
	 int pos = pStationCallsignEntry->get_position();
	 Glib::ustring s = pStationCallsignEntry->get_text().uppercase();
	 pStationCallsignEntry->set_text(s);
	 pStationCallsignEntry->set_position(pos);
	 bStation = std::regex_match(s.c_str(), CallRegEx);
	 pStationCallsignEntry->set_icon_from_icon_name(bStation ? "gtk-ok" : "gtk-cancel");
	 pDPlusEnableCheckButton->set_sensitive(bCallsign);
	 pOkayButton->set_sensitive(bCallsign && bStation);
 }

void CSettingsDlg::OnIntegerChanged(Gtk::Entry *pEntry)
{
	int pos = pEntry->get_position();
	const Glib::ustring good("0123456789");
	Glib::ustring s = pEntry->get_text();
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pEntry->set_text(n);
	pEntry->set_position(pos);
}

void CSettingsDlg::on_APRSIntervalEntry_changed()
{
	OnIntegerChanged(pAPRSIntervalEntry);
}

void CSettingsDlg::on_APRSPortEntry_changed()
{
	OnIntegerChanged(pAPRSPortEntry);
}

void CSettingsDlg::on_GPSDPortEntry_changed()
{
	OnIntegerChanged(pGPSDPortEntry);
}

void CSettingsDlg::On20CharMsgChanged(Gtk::Entry *pEntry)
{
	int pos = pEntry->get_position();
	const Glib::ustring good(" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-/().,:_");
	Glib::ustring s = pEntry->get_text();
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pEntry->set_text(n);
	pEntry->set_position(pos);
}

void CSettingsDlg::on_URLEntry_changed()
{
	OnServerChanged(pURLEntry);
}

void CSettingsDlg::on_APRSServerEntry_changed()
{
	OnServerChanged(pAPRSServerEntry);
}

void CSettingsDlg::on_GPSDServerEntry_changed()
{
	OnServerChanged(pGPSDServerEntry);
}

void CSettingsDlg::OnServerChanged(Gtk::Entry *pEntry)
{
	int pos = pEntry->get_position();
	const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-/.:_%");
	Glib::ustring s = pEntry->get_text();
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pEntry->set_text(n);
	pEntry->set_position(pos);
}

void CSettingsDlg::on_MessageEntry_changed()
{
	On20CharMsgChanged(pMessageEntry);
}

void CSettingsDlg::on_Location1Entry_changed()
{
	On20CharMsgChanged(pLocationEntry[0]);
}

void CSettingsDlg::on_Location2Entry_changed()
{
	On20CharMsgChanged(pLocationEntry[1]);
}

void CSettingsDlg::OnLatLongChanged(Gtk::Entry *pEntry)
{
	int pos = pEntry->get_position();
	const Glib::ustring good("+-.0123456789");
	Glib::ustring s = pEntry->get_text();
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pEntry->set_text(n);
	pEntry->set_position(pos);
}

void CSettingsDlg::on_LatitudeEntry_changed()
{
	OnLatLongChanged(pLatitudeEntry);
}

void CSettingsDlg::on_LongitudeEntry_changed()
{
	OnLatLongChanged(pLongitudeEntry);
}

void CSettingsDlg::BaudrateChanged(int baudrate)
{
	if (AudioManager.AMBEDevice.IsOpen()) {
		if (AudioManager.AMBEDevice.SetBaudRate(baudrate)) {
			AudioManager.AMBEDevice.CloseDevice();
			pDevicePathLabel->set_text("Error");
			pProductIDLabel->set_text("Setting the baudrate failed");
			pVersionLabel->set_text("Please Rescan.");
		}
	}
}

void CSettingsDlg::on_QuadNet_Group_clicked()
{
	if (pIPv6OnlyRadioButton->get_active())
		data.eNetType = EQuadNetType::ipv6only;
	else if (pDualStackRadioButton->get_active())
		data.eNetType = EQuadNetType::dualstack;
	else if (pNoRoutingRadioButton->get_active())
		data.eNetType = EQuadNetType::norouting;
	else
		data.eNetType = EQuadNetType::ipv4only;
}

void CSettingsDlg::on_AudioInputComboBox_changed()
{
	Gtk::ListStore::iterator iter = pAudioInputComboBox->get_active();
	if (iter) {
		Gtk::ListStore::Row row = *iter;
		if (row) {
			Glib::ustring name = row[audio_columns.name];
			Glib::ustring desc = row[audio_columns.desc];

			data.sAudioIn.assign(name.c_str());
			pInputDescLabel->set_text(desc);
		}
	}
}

void CSettingsDlg::on_AudioOutputComboBox_changed()
{
	Gtk::ListStore::iterator iter = pAudioOutputComboBox->get_active();
	if (iter) {
		Gtk::ListStore::Row row = *iter;
		if (row) {
			Glib::ustring name = row[audio_columns.name];
			Glib::ustring desc = row[audio_columns.desc];

			data.sAudioOut.assign(name.c_str());
			pOutputDescLabel->set_text(desc);
		}
	}
}

void CSettingsDlg::on_AudioRescanButton_clicked()
{
	if (0 == data.sAudioIn.size())
		data.sAudioIn.assign("default");
	if (0 == data.sAudioOut.size())
		data.sAudioOut.assign("default");
	void **hints;
	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return;
	void **n = hints;
	refAudioInListModel->clear();
	refAudioOutListModel->clear();
	while (*n != NULL) {
		char *name = snd_device_name_get_hint(*n, "NAME");
		if (NULL == name)
			continue;
		char *desc = snd_device_name_get_hint(*n, "DESC");
		if (NULL == desc) {
			free(name);
			continue;
		}

		if ((0==strcmp(name, "default") || strstr(name, "plughw")) && NULL==strstr(desc, "without any conversions")) {

			char *io = snd_device_name_get_hint(*n, "IOID");
			bool is_input = true, is_output = true;

			if (io) {	// io == NULL means it's for both input and output
				if (0 == strcasecmp(io, "Input")) {
					is_output = false;
				} else if (0 == strcasecmp(io, "Output")) {
					is_input = false;
				} else {
					std::cerr << "ERROR: unexpected IOID=" << io << std::endl;
				}
				free(io);
			}

			Glib::ustring short_name(name);
			auto pos = short_name.find("plughw:CARD=");
			if (short_name.npos != pos) {
				short_name = short_name.replace(pos, 12, "");
				pos = short_name.find(",DEV=0");
				if (short_name.npos != pos)
					short_name = short_name.replace(pos, 6, "");
				if (0 == short_name.size())
					short_name.assign(name);
			}

			if (is_input) {
				snd_pcm_t *handle;
				if (snd_pcm_open(&handle, name, SND_PCM_STREAM_CAPTURE, 0) == 0) {
					Gtk::ListStore::Row row = *(refAudioInListModel->append());
					row[audio_columns.short_name] = short_name;
					row[audio_columns.name] = name;
					row[audio_columns.desc] = desc;
					if (0==data.sAudioIn.compare(name))
						pAudioInputComboBox->set_active(row);
					snd_pcm_close(handle);
				}
			}

			if (is_output) {
				snd_pcm_t *handle;
				if (snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0) == 0) {
					Gtk::ListStore::Row row = *(refAudioOutListModel->append());
					row[audio_columns.short_name] = short_name;
					row[audio_columns.name] = name;
					row[audio_columns.desc] = desc;
					if (0==data.sAudioOut.compare(name))
						pAudioOutputComboBox->set_active(row);
					snd_pcm_close(handle);
				}
			}

		}

	    if (name) {
	      	free(name);
		}
		if (desc) {
			free(desc);
		}
		n++;
	}
	snd_device_name_free_hint(hints);
}
