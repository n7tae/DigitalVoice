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

#include "HostFile.h"

class CSettingsDlg
{
public:
    CSettingsDlg();
    ~CSettingsDlg();
    bool Init(const Glib::RefPtr<Gtk::Builder>, const Glib::ustring &, Gtk::Window *);
    void Show();
	// parameter values
	int baudrate;
private:
	// data
	CHostFile xrfFile, dcsFile, refFile, customFile;
	// helpers
	// widgets
    Gtk::Dialog *pDlg;
	Gtk::Button *pRescanButton;
	Gtk::CheckButton *pUseMyCall;
	Gtk::CheckButton *pXRFCheck, *pDCSCheck, *pREFCheck, *pCustomCheck, *pDPlusRefCheck, *pDPlusRepCheck;
	Gtk::Label       *pXRFLabel, *pDCSLabel, *pREFLabel, *pCustomLabel, *pDPlusRefLabel, *pDPlusRepLabel;
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
};
