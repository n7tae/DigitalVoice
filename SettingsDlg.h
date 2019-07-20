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

#include "HostFile.h"
#include "Configure.h"

class CSettingsDlg
{
public:
    CSettingsDlg();
    ~CSettingsDlg();
    bool Init(const Glib::RefPtr<Gtk::Builder>, const Glib::ustring &, Gtk::Window *);
    CFGDATA *Show();	// returns a pointer to the private CFGDATA if okay is pressed, otherwise a nullptr

private:
	// persistance
	void SaveWidgetStates(CFGDATA &d);
	void SetWidgetStates(const CFGDATA &d);
	// data classes
	CFGDATA data;
	// other data
	bool bCallsign, bStation;
	// regular expression for testing callsign
	std::regex CallRegEx;
	// widgets
    Gtk::Dialog *pDlg;
	Gtk::Button *pRescanButton, *pOkayButton;
	Gtk::CheckButton *pUseMyCall, *pDPlusEnableCheck;
	Gtk::Entry *pStationCallsign, *pMyCallsign, *pMyName, *pMessage, *pLocation, *pURL, *pLatitude, *pLongitude, *pLinkAtStart;
	Gtk::RadioButton *p230k, *p460k, *pIPv4Only, *pIPv6Only, *pDualStack, *pNoRouting;
	Gtk::Label *pDevicePath, *pProductID, *pVersion;
	// events
	void on_UseMyCallsignCheckButton_clicked();
	void on_MyCallsignEntry_changed();
	void on_MyNameEntry_changed();
	void on_StationCallsignEntry_changed();
	void On20CharMsgChanged(Gtk::Entry *pEntry);
	void on_MessageEntry_changed();
	void on_LocationEntry_changed();
	void OnLatLongChanged(Gtk::Entry *pEntry);
	void on_LatitudeEntry_changed();
	void on_LongitudeEntry_changed();
	void on_URLEntry_changed();
	void on_RescanButton_clicked();
	void on_QuadNet_Group_clicked();
	void on_LinkAtStartEntry_changed();
	// state changed
	void BaudrateChanged(int newBaudrate);
	void RebuildGateways(bool includelegacy);
};
