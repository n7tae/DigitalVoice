/*
 *   Copyright (c) 2019-2020 by Thomas A. Early N7TAE
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

#include <future>
#include <gtkmm.h>

#include "Configure.h"
#include "QnetGateway.h"
#include "QnetLink.h"
#include "QnetDB.h"
#include "SettingsDlg.h"
#include "AudioManager.h"

class CMainWindow
{
public:
	CMainWindow();
	~CMainWindow();

	CConfigure cfg;
	CAudioManager AudioManager;

	bool Init(const Glib::RefPtr<Gtk::Builder>, const Glib::ustring &);
	void Run();
	void Receive(bool is_rx);
	void RebuildGateways(bool includelegacy);
private:
	// classes
	CSettingsDlg SettingsDlg;
	CQnetDB qnDB;

	// widgets
	Gtk::Window *pWin;
	Gtk::Button *pQuitButton, *pSettingsButton, *pLinkButton, *pUnlinkButton, *pRouteActionButton, *pQuickKeyButton;
	Gtk::ComboBoxText *pRouteComboBox;
	Gtk::Entry *pLinkEntry, *pRouteEntry;
	Gtk::ToggleButton *pEchoTestButton, *pPTTButton;
	Glib::RefPtr<Gtk::TextBuffer> pLogTextBuffer;
	Gtk::ScrolledWindow *pScrolledWindow;
	Gtk::TextView *pLogTextView;

	// state data
	std::set<Glib::ustring> routeset;
	CFGDATA cfgdata;

	// helpers
	void ReadRoutes();
	void WriteRoutes();
	CQnetGateway *pGate;
	CQnetLink *pLink;
	std::future<void> futLink, futGate;
	void SetState(const CFGDATA &data);
	void RunLink();
	void RunGate();
	void StopLink();
	void StopGate();
	CUnixDgramReader Gate2AM, Link2AM, LogInput;

	// events
	void on_QuitButton_clicked();
	void on_SettingsButton_clicked();
	void on_RouteActionButton_clicked();
	void on_RouteComboBox_changed();
	void on_RouteEntry_changed();
	void on_EchoTestButton_toggled();
	void on_PTTButton_toggled();
	void on_QuickKeyButton_clicked();
	void on_LinkButton_clicked();
	void on_UnlinkButton_clicked();
	void on_LinkEntry_changed();
	bool RelayLink2AM(Glib::IOCondition condition);
	bool RelayGate2AM(Glib::IOCondition condition);
	bool GetLogInput(Glib::IOCondition condition);
	bool TimeoutProcess();
};
