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

#pragma once

#include <gtkmm.h>
#include <regex>

#include "Defines.h"
#include "HostFile.h"

class CSettingsDlg
{
public:
    CSettingsDlg();
    ~CSettingsDlg();
    bool Init(const Glib::RefPtr<Gtk::Builder>, const Glib::ustring &, Gtk::Window *);
    void Show();
	static SSETTINGSDATA *ReadCfgFile();

private:
	// data classes
	CHostFile xrfFile, dcsFile, refFile, dplusFile, customFile;
	// other data
	int baudrate;
	bool bCallsign, bStation;
	// regular expression for testing callsign
	std::regex CallRegEx;
	// persistance
	void WriteCfgFile();
	// widgets
    Gtk::Dialog *pDlg;
	Gtk::Button *pRescanButton, *pOkayButton;
	Gtk::CheckButton *pUseMyCall;
	Gtk::CheckButton *pXRFCheck, *pDCSCheck, *pREFRefCheck, *pREFRepCheck, *pCustomCheck, *pDPlusRefCheck, *pDPlusRepCheck, *pDPlusEnableCheck;
	Gtk::Label       *pXRFLabel, *pDCSLabel, *pREFRefLabel, *pREFRepLabel, *pCustomLabel, *pDPlusRefLabel, *pDPlusRepLabel;
	Gtk::Entry *pStationCallsign, *pMyCallsign, *pMyName, *pMessage;
	Gtk::RadioButton *p230k, *p460k;
	Gtk::Label *pDevicePath, *pProductID, *pVersion;
	// events
	void on_UseMyCallsignCheckButton_clicked();
	void on_MyCallsignEntry_changed();
	void on_MyNameEntry_changed();
	void on_StationCallsignEntry_changed();
	void on_MessageEntry_changed();
	void on_RescanButton_clicked();
	void on_BaudrateRadioButton_toggled();
	void on_DPlusEnableCheck_toggled();
};
