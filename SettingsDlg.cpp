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

#include "AudioManager.h"
#include "WaitCursor.h"
#include "DPlusAuthenticator.h"
#include "SettingsDlg.h"

// globals
extern Glib::RefPtr<Gtk::Application> theApp;
extern CAudioManager AudioManager;
extern CConfigure cfg;
extern bool GetCfgDirectory(std::string &path);
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
	on_RescanButton_clicked();	// reset the ambe device

	if (Gtk::RESPONSE_OK == pDlg->run()) {
		CFGDATA newstate;			// the user clicked okay, time to look at what's changed
		SaveWidgetStates(newstate); // newstate is now the current contents of the Settings Dialog
		cfg.CopyFrom(newstate);		// and it is now in the cfg object
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
	d.sCallsign.assign(pMyCallsign->get_text());
	d.sName.assign(pMyName->get_text());
	d.bUseMyCall = pUseMyCall->get_active();
	d.sStation = pStationCallsign->get_text();
	d.sMessage.assign(pMessage->get_text());
	d.sLocation.assign(pLocation->get_text());
	d.cModule = data.cModule;
	d.dLatitude = std::stod(pLatitude->get_text());
	d.dLongitude = std::stod(pLongitude->get_text());
	d.sURL.assign(pURL->get_text());
	//link
	d.sLinkAtStart.assign(pLinkAtStart->get_text());
	d.bMaintainLink = pMaintainLink->get_active();
	d.bDPlusEnable = pDPlusEnableCheck->get_active();
	// quadnet
	if (pIPv6Only->get_active())
		d.eNetType = EQuadNetType::ipv6only;
	else if (pDualStack->get_active())
		d.eNetType = EQuadNetType::dualstack;
	else if (pNoRouting->get_active())
		d.eNetType = EQuadNetType::norouting;
	else
		d.eNetType = EQuadNetType::ipv4only;
	// device
	d.iBaudRate = (p230k->get_active()) ? 230400 : 460800;
}

void CSettingsDlg::SetWidgetStates(const CFGDATA &d)
{
	// station
	pMyCallsign->set_text(d.sCallsign);
	pMyName->set_text(d.sName);
	pStationCallsign->set_text(d.sStation);
	pUseMyCall->set_active(d.bUseMyCall);
	pMessage->set_text(d.sMessage);
	pLocation->set_text(d.sLocation);
	pLatitude->set_text(std::to_string(d.dLatitude));
	pLongitude->set_text(std::to_string(d.dLongitude));
	pURL->set_text(d.sURL);
	//link
	pLinkAtStart->set_text(d.sLinkAtStart);
	pMaintainLink->set_active(d.bMaintainLink);
	if (d.bDPlusEnable != pDPlusEnableCheck->get_active())
		pDPlusEnableCheck->set_active(d.bDPlusEnable);	// only do this if we need to
	//quadnet
	switch (d.eNetType) {
		case EQuadNetType::ipv6only:
			pIPv6Only->set_active();
			break;
		case EQuadNetType::dualstack:
			pDualStack->set_active();
			break;
		case EQuadNetType::norouting:
			pNoRouting->set_active();
			break;
		default:
			pIPv4Only->set_active();
			break;
	}
	//device
	if (230400 == d.iBaudRate)
		p230k->clicked();
	else
		p460k->clicked();
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
	builder->get_widget("MyCallsignEntry", pMyCallsign);
	builder->get_widget("MyNameEntry", pMyName);
	builder->get_widget("UseMyCallsignCheckButton", pUseMyCall);
	builder->get_widget("StationCallsignEntry", pStationCallsign);
	builder->get_widget("MessageEntry", pMessage);
	builder->get_widget("LocationEntry", pLocation);
	builder->get_widget("LatitudeEntry", pLatitude);
	builder->get_widget("LongitudeEntry", pLongitude);
	builder->get_widget("URLEntry", pURL);
	// device
	builder->get_widget("DeviceLabel", pDevicePath);
	builder->get_widget("ProductIDLabel", pProductID);
	builder->get_widget("VersionLabel", pVersion);
	builder->get_widget("Baud230kRadioButton", p230k);
	builder->get_widget("Baud460kRadioButton", p460k);
	builder->get_widget("RescanButton", pRescanButton);
	// linking
	builder->get_widget("LinkAtStartEntry", pLinkAtStart);
	builder->get_widget("MaintainLinkCheckButton", pMaintainLink);
	builder->get_widget("LegacyCheckButton", pDPlusEnableCheck);
	// QuadNet
	builder->get_widget("IPV4_RadioButton", pIPv4Only);
	builder->get_widget("IPV6_RadioButton", pIPv6Only);
	builder->get_widget("Dual_Stack_RadioButton", pDualStack);
	builder->get_widget("No_Routing_RadioButton", pNoRouting);

	pMyCallsign->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyCallsignEntry_changed));
	pMyName->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyNameEntry_changed));
	pStationCallsign->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_StationCallsignEntry_changed));
	pUseMyCall->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_UseMyCallsignCheckButton_clicked));
	pMessage->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MessageEntry_changed));
	pLocation->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LocationEntry_changed));
	pLatitude->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LatitudeEntry_changed));
	pLongitude->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LongitudeEntry_changed));
	pURL->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_URLEntry_changed));
	pIPv4Only->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pIPv6Only->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pDualStack->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pNoRouting->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_QuadNet_Group_clicked));
	pRescanButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_RescanButton_clicked));
	pLinkAtStart->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_LinkAtStartEntry_changed));

	return false;
}

void CSettingsDlg::RebuildGateways(bool includelegacy)
{
	gwys.Init();

	if (includelegacy) {
		CWaitCursor wait;
		const std::string call(pMyCallsign->get_text().c_str());
		CDPlusAuthenticator auth(call, "auth.dstargateway.org");
		if (auth.Process(gwys.hostmap, true, false)) {
			std::cerr << "Error processing D-Plus authorization" << std::endl;
		}
	}
}

void CSettingsDlg::on_RescanButton_clicked()
{
	CWaitCursor wait;
	if (AudioManager.AMBEDevice.IsOpen())
		AudioManager.AMBEDevice.CloseDevice();

	AudioManager.AMBEDevice.FindandOpen(data.iBaudRate, DSTAR_TYPE);
	if (AudioManager.AMBEDevice.IsOpen()) {
		const Glib::ustring path(AudioManager.AMBEDevice.GetDevicePath());
		pDevicePath->set_text(path);
		const Glib::ustring prodid(AudioManager.AMBEDevice.GetProductID());
		pProductID->set_text(prodid);
		const Glib::ustring version(AudioManager.AMBEDevice.GetVersion());
		pVersion->set_text(version);
	} else {
		pDevicePath->set_text("Not found!");
		pProductID->set_text("Is the device plugged in?");
		pVersion->set_text("Are you in the 'dialout' group?");
	}
}

void CSettingsDlg::on_UseMyCallsignCheckButton_clicked()
{
	if (pUseMyCall->get_active()) {
		pStationCallsign->hide();
		pStationCallsign->set_text(pMyCallsign->get_text());
	} else {
		pStationCallsign->show();
	}
}

void CSettingsDlg::on_MyCallsignEntry_changed()
 {
	 int pos = pMyCallsign->get_position();
	 Glib::ustring s = pMyCallsign->get_text().uppercase();
	 pMyCallsign->set_text(s);
	 pMyCallsign->set_position(pos);
	 bCallsign = std::regex_match(s.c_str(), CallRegEx);
	 pMyCallsign->set_icon_from_icon_name(bCallsign ? "gtk-ok" : "gtk-cancel");
	 pDPlusEnableCheck->set_sensitive(bCallsign);
	 pOkayButton->set_sensitive(bCallsign && bStation);
	 if (pUseMyCall->get_active())
	 	pStationCallsign->set_text(s);
 }

void CSettingsDlg::on_MyNameEntry_changed()
 {
	 int pos = pMyName->get_position();
	 Glib::ustring s = pMyName->get_text();
	 pMyName->set_text(s.uppercase());
	 pMyName->set_position(pos);
 }

 void CSettingsDlg::on_LinkAtStartEntry_changed()
 {
	 int pos = pLinkAtStart->get_position();
	 Glib::ustring s = pLinkAtStart->get_text().uppercase();
	 const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	 Glib::ustring n;
	 for (auto it=s.begin(); it!=s.end(); it++) {
		 if (Glib::ustring::npos != good.find(*it)) {
			 n.append(1, *it);
		 }
	 }
	 pLinkAtStart->set_text(n);
	 pLinkAtStart->set_position(pos);
 }

void CSettingsDlg::on_StationCallsignEntry_changed()
 {
	 int pos = pStationCallsign->get_position();
	 Glib::ustring s = pStationCallsign->get_text();
	 pStationCallsign->set_text(s.uppercase());
	 pStationCallsign->set_position(pos);
	 bStation = std::regex_match(s.c_str(), CallRegEx);
	 pStationCallsign->set_icon_from_icon_name(bStation ? "gtk-ok" : "gtk-cancel");
	 pOkayButton->set_sensitive(bCallsign && bStation);
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
	int pos = pURL->get_position();
	const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-/.:_%");
	Glib::ustring s = pURL->get_text();
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pURL->set_text(n);
	pURL->set_position(pos);
}

void CSettingsDlg::on_MessageEntry_changed()
{
	On20CharMsgChanged(pMessage);
}

void CSettingsDlg::on_LocationEntry_changed()
{
	On20CharMsgChanged(pLocation);
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
	OnLatLongChanged(pLatitude);
}

void CSettingsDlg::on_LongitudeEntry_changed()
{
	OnLatLongChanged(pLongitude);
}

void CSettingsDlg::BaudrateChanged(int baudrate)
{
	if (AudioManager.AMBEDevice.IsOpen()) {
		if (AudioManager.AMBEDevice.SetBaudRate(baudrate)) {
			AudioManager.AMBEDevice.CloseDevice();
			pDevicePath->set_text("Error");
			pProductID->set_text("Setting the baudrate failed");
			pVersion->set_text("Please Rescan.");
		}
	}
}

void CSettingsDlg::on_QuadNet_Group_clicked()
{
	if (pIPv6Only->get_active())
		data.eNetType = EQuadNetType::ipv6only;
	else if (pDualStack->get_active())
		data.eNetType = EQuadNetType::dualstack;
	else if (pNoRouting->get_active())
		data.eNetType = EQuadNetType::norouting;
	else
		data.eNetType = EQuadNetType::ipv4only;
}
