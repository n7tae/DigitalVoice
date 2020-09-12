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

#include <regex>
#include <future>
#include <gtkmm.h>

#include "Configure.h"
#include "QnetGateway.h"
#include "QnetLink.h"
#include "QnetDB.h"
#include "SettingsDlg.h"
#include "AboutDlg.h"
#include "AudioManager.h"
#include "aprs.h"

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
	// regular expression for testing stuff
	std::regex CallRegEx, IPRegEx, M17CallRegEx;

private:
	// classes
	CSettingsDlg SettingsDlg;
	CAboutDlg AboutDlg;
	CQnetDB qnDB;

	// widgets
	Gtk::Window *pWin;
	Gtk::Button *pQuitButton, *pSettingsButton, *pLinkButton, *pUnlinkButton, *pRouteActionButton, *pQuickKeyButton, *pM17DestActionButton;
	Gtk::ComboBoxText *pRouteComboBox, *pM17DestCallsignComboBox;
	Gtk::Entry *pLinkEntry, *pRouteEntry, *pM17DestCallsignEntry, *pM17DestIPEntry;
	Gtk::ToggleButton *pEchoTestButton, *pPTTButton;
	Gtk::MenuItem *pAboutMenuItem;
	Glib::RefPtr<Gtk::TextBuffer> pLogTextBuffer;
	Gtk::ScrolledWindow *pScrolledWindow;
	Gtk::TextView *pLogTextView;
	Gtk::StackSwitcher *pMainStackSwitcher;
	Gtk::Stack *pMainStack;
	Gtk::Box *pM17Stack, *pDStarStack;

	// state data
	std::set<Glib::ustring> routeset;
	std::map<std::string, std::string> destmap;
	CFGDATA cfgdata;

	// helpers
	void FixM17DestActionButton();
	void SetDestActionButton(const bool sensitive, const char *label);
	void ReadDestinations();
	void WriteDestinations();
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
	CUnixDgramReader Gate2AM, Link2AM, M17_2AM, LogInput;
	CAPRS aprs;

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
	void on_AboutMenuItem_activate();
	void on_M17DestCallsignEntry_changed();
	void on_M17DestIPEntry_changed();
	void on_M17DestCallsignComboBox_changed();
	void on_M17DestActionButton_clicked();
	bool RelayLink2AM(Glib::IOCondition condition);
	bool RelayGate2AM(Glib::IOCondition condition);
	bool RelayM17_2AM(Glib::IOCondition condition);
	bool GetLogInput(Glib::IOCondition condition);
	bool TimeoutProcess();

	bool destCS, destIP;
};
