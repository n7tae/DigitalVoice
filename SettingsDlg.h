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

class CWaitCursor
{
public:
	CWaitCursor() :
	screen(gdk_screen_get_default()),
	win(gdk_screen_get_root_window(screen)),
	display(gdk_screen_get_display(screen))
	{
		gtkSetCursor(GDK_WATCH);
	}

	~CWaitCursor()
	{
		gtkSetCursor(GDK_LEFT_PTR);
	}

private:
	// params
	GdkScreen *screen;
	GdkWindow *win;
	GdkDisplay *display;
	// methods
	void gtkSetCursor(GdkCursorType cursorType)
	{
		GdkCursor *cursor = gdk_cursor_new_for_display(display, cursorType);
		gdk_window_set_cursor(win, cursor);
		while (gtk_events_pending())
			gtk_main_iteration();
	}
};

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
	CHostFile xrfFile, dcsFile, refFile, dplusFile, customFile;
	// helpers
	std::regex CallRegEx;
	// widgets
    Gtk::Dialog *pDlg;
	Gtk::Button *pRescanButton, *pOkayButton;
	Gtk::CheckButton *pUseMyCall, *pValidCall;
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
