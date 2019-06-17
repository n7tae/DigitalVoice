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
#include "SettingsDlg.h"

// globals
extern Glib::RefPtr<Gtk::Application> theApp;
extern CDV3000U AMBEDevice;

CSettingsDlg::CSettingsDlg() :
	pDlg(nullptr)
{
}

CSettingsDlg::~CSettingsDlg()
{
	if (pDlg)
		delete pDlg;
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
	pDlg->add_button("Okay", Gtk::RESPONSE_OK);

	GET_WIDGET("UseMyCallsignCheckButton", pUseMyCall);
	pUseMyCall->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_UseMyCallsignCheckButton_clicked));

	GET_WIDGET("StationCallsignEntry", pStationCallsign);

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
	GET_WIDGET("REFCheckButton", pREFCheck);
	GET_WIDGET("REFCountLabel", pREFLabel);
	pREFLabel->set_text(std::to_string(refFile.hostmap.size()));

	customFile.Open("Custom_Hosts.txt", 20001);
	GET_WIDGET("CustomCheckButton", pCustomCheck);
	GET_WIDGET("CustomCountLabel", pCustomLabel);
	pCustomLabel->set_text(std::to_string(customFile.hostmap.size()));

	on_RescanButton_clicked();	// load it up on init

	return false;
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
	 Glib::ustring s = pMyCallsign->get_text();
	 pMyCallsign->set_text(s.uppercase());
	 pMyCallsign->set_position(pos);
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
