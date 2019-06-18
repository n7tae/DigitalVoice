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

#include "DV3000U.h"
#include "DPlusAuthenticator.h"
#include "SettingsDlg.h"

// globals
extern Glib::RefPtr<Gtk::Application> theApp;
extern CDV3000U AMBEDevice;

CSettingsDlg::CSettingsDlg() :
	pDlg(nullptr)
{
	CallRegEx = std::regex("^(([1-9][A-Z])|([A-Z][0-9])|([A-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][ ]*[ A-RT-Z]$", std::regex::extended);
}

CSettingsDlg::~CSettingsDlg()
{
	if (pDlg)
		delete pDlg;
}

void CSettingsDlg::Show()
{
	switch(pDlg->run()) {
		case Gtk::RESPONSE_CANCEL:
			std::cout << "Clicked Cancel!" << std::endl;
			break;
		case Gtk::RESPONSE_OK:
			std::cout << "Clicked OK!" << std::endl;
			break;
		default:
			std::cout << "Unexpected return from dlg->run()!" << std::endl;
			break;
	}
	pDlg->hide();
}

#define GET_WIDGET(a,b) builder->get_widget(a, b); if (nullptr == b) { std::cerr << "Failed to initialize " << a << std::endl; return true; }

bool CSettingsDlg::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name, Gtk::Window *pWin)
{
	builder->get_widget(name, pDlg);
	if (nullptr == pDlg) {
		std::cerr << "Failed to initialize SettingsDialog!" << std::endl;
		return true;
	}
	pDlg->set_transient_for(*pWin);
	pDlg->add_button("Cancel", Gtk::RESPONSE_CANCEL);
	pOkayButton = pDlg->add_button("Okay", Gtk::RESPONSE_OK);
	pOkayButton->set_sensitive(false);

	GET_WIDGET("UseMyCallsignCheckButton", pUseMyCall);
	pUseMyCall->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_UseMyCallsignCheckButton_clicked));

	GET_WIDGET("StationCallsignEntry", pStationCallsign);

	GET_WIDGET("ValidCallCheckButton", pValidCall);
	GET_WIDGET("MyCallsignEntry", pMyCallsign);
	pMyCallsign->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyCallsignEntry_changed));

	GET_WIDGET("MyNameEntry", pMyName);
	pMyName->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyNameEntry_changed));

	GET_WIDGET("StationCallsignEntry", pStationCallsign);
	pStationCallsign->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_StationCallsignEntry_changed));

	GET_WIDGET("MessageEntry", pMessage);
	pMessage->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MessageEntry_changed));

	GET_WIDGET("DeviceLabel", pDevicePath);
	GET_WIDGET("ProductIDLabel", pProductID);
	GET_WIDGET("VersionLabel", pVersion);

	baudrate = 0;
	GET_WIDGET("Baud230kRadioButton", p230k);
	p230k->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_BaudrateRadioButton_toggled));
	if (p230k->get_active())
		baudrate = 230400;

	GET_WIDGET("Baud460kRadioButton", p460k);
	p460k->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_BaudrateRadioButton_toggled));
	if (p460k->get_active())
		baudrate = 460800;

	if (0 == baudrate) {
		std::cerr << "Baudrate not set" << std::endl;
		return true;
	}

	GET_WIDGET("RescanButton", pRescanButton);
	pRescanButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_RescanButton_clicked));

	xrfFile.Open("DExtra_Hosts.txt", 30001);
	GET_WIDGET("XRFCheckButton", pXRFCheck);
	GET_WIDGET("XRFCountLabel", pXRFLabel);
	pXRFLabel->set_text(std::to_string(xrfFile.hostmap.size()));

	dcsFile.Open("DCS_Hosts.txt", 30051);
	GET_WIDGET("DCSCheckButton", pDCSCheck);
	GET_WIDGET("DCSCountLabel", pDCSLabel);
	pDCSLabel->set_text(std::to_string(dcsFile.hostmap.size()));

	refFile.Open("DPlus_Hosts.txt", 20001);
	GET_WIDGET("REFRefCheckButton", pREFRepCheck);
	GET_WIDGET("REFRefCountLabel", pREFRepLabel);
	GET_WIDGET("REFRepCheckButton", pREFRefCheck);
	GET_WIDGET("REFRepCountLabel", pREFRefLabel);
	int ref = 0, rep = 0;
	for (auto it=refFile.hostmap.begin(); it!=refFile.hostmap.end(); it++) {
		if (it->first.compare(0, 3, "REF"))
			rep++;
		else
			ref++;
	}
	pREFRefLabel->set_text(std::to_string(ref));
	pREFRepLabel->set_text(std::to_string(rep));

	customFile.Open("Custom_Hosts.txt", 20001);
	GET_WIDGET("CustomCheckButton", pCustomCheck);
	GET_WIDGET("CustomCountLabel", pCustomLabel);
	pCustomLabel->set_text(std::to_string(customFile.hostmap.size()));

	GET_WIDGET("DPlusEnableCheckButton", pDPlusEnableCheck);
	pDPlusEnableCheck->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_DPlusEnableCheck_toggled));
	GET_WIDGET("DPlusRefCheckButton", pDPlusRefCheck);
	GET_WIDGET("DPlusRepCheckButton", pDPlusRepCheck);
	GET_WIDGET("DPlusRefCountLabel", pDPlusRefLabel);
	GET_WIDGET("DPlusRepCountLabel", pDPlusRepLabel);

	// initialize complex components
	on_DPlusEnableCheck_toggled();
	on_RescanButton_clicked();

	return false;
}

void CSettingsDlg::on_DPlusEnableCheck_toggled()
{
	int ref = 0, rep = 0;
	dplusFile.ClearMap();
	if (pDPlusEnableCheck->get_active()) {
		//GdkCursor *cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "wait");
		//gdk_window_set_cursor(gdk_get_default_root_window(), cursor);
		pDPlusRefCheck->show();
		pDPlusRefLabel->show();
		pDPlusRepCheck->show();
		pDPlusRepLabel->show();
		const std::string call(pMyCallsign->get_text().c_str());
		CDPlusAuthenticator auth(call, "auth.dstargateway.org");
		if (auth.Process(dplusFile.hostmap, true, true)) {
			std::cerr << "Error processing D-Plus authorization" << std::endl;
		} else {
			for (auto it=dplusFile.hostmap.begin(); it!=dplusFile.hostmap.end(); it++) {
				if (it->first.compare(0, 3, "REF"))
					rep++;
				else
					ref++;
			}
		}
		pDPlusRefLabel->set_text(std::to_string(ref));
		pDPlusRepLabel->set_text(std::to_string(rep));
		//gdk_window_set_cursor(gdk_get_default_root_window(), NULL);
	} else {
		pDPlusRefCheck->hide();
		pDPlusRefLabel->hide();
		pDPlusRepCheck->hide();
		pDPlusRepLabel->hide();
	}
}

void CSettingsDlg::on_RescanButton_clicked()
{
	if (AMBEDevice.IsOpen())
		AMBEDevice.CloseDevice();

	AMBEDevice.FindandOpen(baudrate, DSTAR_TYPE);
	if (AMBEDevice.IsOpen()) {
		const Glib::ustring path(AMBEDevice.GetDevicePath());
		pDevicePath->set_text(path);
		const Glib::ustring prodid(AMBEDevice.GetProductID());
		pProductID->set_text(prodid);
		const Glib::ustring version(AMBEDevice.GetVersion());
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
	 bool ok = std::regex_match(s.c_str(), CallRegEx);
	 pValidCall->set_active(ok);
	 pDPlusEnableCheck->set_sensitive(ok);
	 pOkayButton->set_sensitive(ok);
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

void CSettingsDlg::on_StationCallsignEntry_changed()
 {
	 int pos = pStationCallsign->get_position();
	 Glib::ustring s = pStationCallsign->get_text();
	 pStationCallsign->set_text(s.uppercase());
	 pStationCallsign->set_position(pos);
 }

void CSettingsDlg::on_MessageEntry_changed()
{
	int pos = pMessage->get_position();
	const Glib::ustring good(" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-/().,:_");
	Glib::ustring s = pMessage->get_text();
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pMessage->set_text(n);
	pMessage->set_position(pos);
}

void CSettingsDlg::on_BaudrateRadioButton_toggled()
{
	if (AMBEDevice.IsOpen()) {
		int baudrate = 0;
		if (p230k->get_active())
			baudrate = 230400;
		else if (p460k->get_active())
			baudrate = 460800;

		if (baudrate) {
			if (AMBEDevice.SetBaudRate(baudrate)) {
				AMBEDevice.CloseDevice();
				pDevicePath->set_text("Error");
				pProductID->set_text("Setting the baudrate failed");
				pVersion->set_text("Pleae Rescan.");
			}
		}
	}
}
