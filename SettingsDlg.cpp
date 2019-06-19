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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <iostream>
#include <fstream>

#include "Defines.h"
#include "DV3000U.h"
#include "WaitCursor.h"
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
	if (Gtk::RESPONSE_OK == pDlg->run())
		WriteCfgFile();
	pDlg->hide();
}

SSETTINGSDATA *CSettingsDlg::ReadCfgFile()
{
	const char *homedir = getenv("HOME");

	if (NULL == homedir)
		homedir = getpwuid(getuid())->pw_dir;

	if (NULL == homedir) {
		std::cerr << "ERROR: could not find a home directory!" << std::endl;
		return NULL;
	}

	std::string filename(homedir);
	filename.append("/.config/qndv/qndv.cfg");

	struct stat sb;
	if (stat(filename.c_str(), &sb))
		return NULL;

	if ((sb.st_mode & S_IFMT) != S_IFREG)
		return NULL;	// needs to be a regular file

	if (sb.st_size < 50)
		return NULL; // there has to be something in it

	SSETTINGSDATA *pData = new SSETTINGSDATA;
	if (NULL == pData)
		return pData;	// ouch!

	std::ifstream cfg(filename.c_str(), std::ifstream::in);
	if (! cfg.is_open()) {	// this should not happen!
		std::cerr << "Error opening " << filename << "!" << std::endl;
		delete pData;
		return NULL;
	}

	while (! cfg.eof()) {
		char line[128];
		cfg.getline(line, 128);
		char *key = strtok(line, " =\t\r\"");
		if (!key) continue;	// skip empty lines
		if (0==strlen(key) || '#'==*key) continue;	// skip comments
		char *val = strtok(NULL, "' #");
		//if (0==strlen(val)) continue;	// skip lines with no values

		if (0 == strcmp(key, "MyCall")) {
			pData->MyCall.assign(val);
		} else if (0 == strcmp(key, "MyName")) {
			pData->MyName.assign(val);
		} else if (0 == strcmp(key, "StationCall")) {
			pData->StationCall.assign(val);
		} else if (0 == strcmp(key, "UseMyCall")) {
			pData->UseMyCall = IS_TRUE(*val);
		} else if (0 == strcmp(key, "Message")) {
			pData->Message.assign(val);
		} else if (0 == strcmp(key, "XRF")) {
			pData->XRF = IS_TRUE(*val);
		} else if (0 == strcmp(key, "DCS")) {
			pData->DCS = IS_TRUE(*val);
		} else if (0 == strcmp(key, "REFref")) {
			pData->REFref = IS_TRUE(*val);
		} else if (0 == strcmp(key, "REFrep")) {
			pData->REFrep = IS_TRUE(*val);
		} else if (0 == strcmp(key, "MyHosts")) {
			pData->MyHost = IS_TRUE(*val);
		} else if (0 == strcmp(key, "DPlusEnable")) {
			pData->DPlusEnable = IS_TRUE(*val);
		} else if (0 == strcmp(key, "DPlusRef")) {
			pData->DPlusRef = IS_TRUE(*val);
		} else if (0 == strcmp(key, "DPlusRep")) {
			pData->DPlusRep = IS_TRUE(*val);
		} else if (0 == strcmp(key, "BaudRate")) {
			pData->BaudRate = (0 == strcmp(val, "460800")) ? 460800 : 230400;
		}
	}
	cfg.close();
	return pData;
}

void CSettingsDlg::WriteCfgFile()
{
	const char *homedir = getenv("HOME");

	if (NULL == homedir)
		homedir = getpwuid(getuid())->pw_dir;

	if (NULL == homedir) {
		std::cerr << "ERROR: could not find a home directory!" << std::endl;
		return;
	}

	std::string path(homedir);
	path.append("/.config");
	for (int i=0; i<2; i++) { // walk through two sub-directories
		struct stat sb;
		if (stat(path.c_str(), &sb)) {
			// stat failed, make the directory
			if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
				std::cerr << "ERROR: can't create " << path << '!' << std::endl;
				return;
			}
		} else {	// make sure it's a directory
			if ((sb.st_mode & S_IFMT) != S_IFDIR) {
				std::cerr << "ERROR: " << path << " is not a directory!" << std::endl;
				return;
			}
		}
		if (0 == i)
			path.append("/qndv");
		else
			path.append("/qndv.cfg");
	}

	// directory exists, now make the file
	std::ofstream file(path.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (! file.is_open()) {
		std::cerr << "ERROR: could not open " << path << '!' << std::endl;
		return;
	}
	file << "MyCall=" << pMyCallsign->get_text() << std::endl;
	file << "MyName=" << pMyName->get_text() << std::endl;
	file << "StationCall=" << pStationCallsign->get_text() << std::endl;
	file << "UseMyCall=" << (pUseMyCall->get_active() ? "true" : "false") << std::endl;
	file << "Message=\"" << pMessage->get_text() << '"' << std::endl;
	file << "XRF=" << (pXRFCheck->get_active() ? "true" : "false") << std::endl;
	file << "DCS=" << (pDCSCheck->get_active() ? "true" : "false") << std::endl;
	file << "REFref=" << (pREFRefCheck->get_active() ? "true" : "false") << std::endl;
	file << "REFrep=" << (pREFRepCheck->get_active() ? "true" : "false") << std::endl;
	file << "MyHosts=" << (pCustomCheck->get_active() ? "true" : "false") << std::endl;
	file << "DPlusEnable=" << (pDPlusEnableCheck->get_active() ? "true" : "false") << std::endl;
	file << "DPlusRef=" << (pDPlusRefCheck->get_active() ? "true" : "false") << std::endl;
	file << "DPlusRep=" << (pDPlusRepCheck->get_active() ? "true" : "false") << std::endl;
	file << "BaudRate=" << baudrate << std::endl;
	file.close();
}

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

	SSETTINGSDATA *pData = ReadCfgFile();

	// station
	builder->get_widget("ValidCallCheckButton", pValidCall);
	builder->get_widget("MyCallsignEntry", pMyCallsign);
	builder->get_widget("MyNameEntry", pMyName);
	builder->get_widget("DPlusEnableCheckButton", pDPlusEnableCheck);
	builder->get_widget("UseMyCallsignCheckButton", pUseMyCall);
	builder->get_widget("StationCallsignEntry", pStationCallsign);
	builder->get_widget("MessageEntry", pMessage);
	// device
	builder->get_widget("DeviceLabel", pDevicePath);
	builder->get_widget("ProductIDLabel", pProductID);
	builder->get_widget("VersionLabel", pVersion);
	builder->get_widget("Baud230kRadioButton", p230k);
	builder->get_widget("Baud460kRadioButton", p460k);
	builder->get_widget("RescanButton", pRescanButton);
	// linking
	builder->get_widget("XRFCheckButton", pXRFCheck);
	builder->get_widget("XRFCountLabel", pXRFLabel);
	builder->get_widget("DCSCheckButton", pDCSCheck);
	builder->get_widget("DCSCountLabel", pDCSLabel);
	builder->get_widget("REFRefCheckButton", pREFRepCheck);
	builder->get_widget("REFRefCountLabel", pREFRepLabel);
	builder->get_widget("REFRepCheckButton", pREFRefCheck);
	builder->get_widget("REFRepCountLabel", pREFRefLabel);
	builder->get_widget("CustomCheckButton", pCustomCheck);
	builder->get_widget("CustomCountLabel", pCustomLabel);
	builder->get_widget("DPlusRefCheckButton", pDPlusRefCheck);
	builder->get_widget("DPlusRepCheckButton", pDPlusRepCheck);
	builder->get_widget("DPlusRefCountLabel", pDPlusRefLabel);
	builder->get_widget("DPlusRepCountLabel", pDPlusRepLabel);

	pMyCallsign->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyCallsignEntry_changed));
	pMyName->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MyNameEntry_changed));
	pStationCallsign->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_StationCallsignEntry_changed));
	pUseMyCall->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_UseMyCallsignCheckButton_clicked));
	pMessage->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_MessageEntry_changed));
	if (pData) {
		pMyCallsign->set_text(pData->MyCall.c_str());
		pMyName->set_text(pData->MyName.c_str());
		pStationCallsign->set_text(pData->StationCall.c_str());
		pUseMyCall->set_active(pData->UseMyCall);
		pMessage->set_text(pData->Message.c_str());
	}

	if (pData) {
		if (230400 == pData->BaudRate)
			p230k->clicked();
		else
			p460k->clicked();
	}
	baudrate = 0;
	p230k->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_BaudrateRadioButton_toggled));
	if (p230k->get_active())
		baudrate = 230400;

	p460k->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_BaudrateRadioButton_toggled));
	if (p460k->get_active())
		baudrate = 460800;

	if (0 == baudrate) {
		std::cerr << "Baudrate not set" << std::endl;
		return true;
	}

	pRescanButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_RescanButton_clicked));

	xrfFile.Open("DExtra_Hosts.txt", 30001);
	dcsFile.Open("DCS_Hosts.txt", 30051);
	refFile.Open("DPlus_Hosts.txt", 20001);
	customFile.Open("My_Hosts.txt", 20001);

	pXRFLabel->set_text(std::to_string(xrfFile.hostmap.size()));
	pDCSLabel->set_text(std::to_string(dcsFile.hostmap.size()));
	int ref = 0, rep = 0;
	for (auto it=refFile.hostmap.begin(); it!=refFile.hostmap.end(); it++) {
		if (it->first.compare(0, 3, "REF"))
			rep++;
		else
			ref++;
	}
	pREFRefLabel->set_text(std::to_string(ref));
	pREFRepLabel->set_text(std::to_string(rep));
	pCustomLabel->set_text(std::to_string(customFile.hostmap.size()));
	if (pData) {
		pXRFCheck->set_active(pData->XRF);
		pDCSCheck->set_active(pData->DCS);
		pREFRefCheck->set_active(pData->REFref);
		pREFRepCheck->set_active(pData->REFrep);
		pCustomCheck->set_active(pData->MyHost);
		pDPlusEnableCheck->set_active(pData->DPlusEnable);
		pDPlusRefCheck->set_active(pData->DPlusRef);
		pDPlusRepCheck->set_active(pData->DPlusRep);
	}
	pDPlusEnableCheck->signal_toggled().connect(sigc::mem_fun(*this, &CSettingsDlg::on_DPlusEnableCheck_toggled));

	// initialize complex components
	on_DPlusEnableCheck_toggled();
	on_RescanButton_clicked();

	if (pData)
		delete pData;

	return false;
}

void CSettingsDlg::on_DPlusEnableCheck_toggled()
{
	int ref = 0, rep = 0;
	dplusFile.ClearMap();
	if (pDPlusEnableCheck->get_active()) {
		CWaitCursor wait;
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
	} else {
		pDPlusRefCheck->hide();
		pDPlusRefLabel->hide();
		pDPlusRepCheck->hide();
		pDPlusRepLabel->hide();
	}
}

void CSettingsDlg::on_RescanButton_clicked()
{
	CWaitCursor wait;
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
	baudrate = 0;
	if (p230k->get_active())
		baudrate = 230400;
	else if (p460k->get_active())
		baudrate = 460800;

	if (AMBEDevice.IsOpen() && baudrate) {
		if (AMBEDevice.SetBaudRate(baudrate)) {
			AMBEDevice.CloseDevice();
			pDevicePath->set_text("Error");
			pProductID->set_text("Setting the baudrate failed");
			pVersion->set_text("Pleae Rescan.");
		}
	}
}
