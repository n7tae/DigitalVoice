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
#include "SettingsDlg.h"
#include "MainWindow.h"

// globals
CSettingsDlg SettingsDlg;
extern Glib::RefPtr<Gtk::Application> theApp;

CMainWindow::CMainWindow() :
	pWin(nullptr),
	pQuitButton(nullptr),
	pSettingsButton(nullptr)
{
}

CMainWindow::~CMainWindow()
{
	if (pWin)
		delete pWin;
}

bool CMainWindow::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name)
{
	builder->get_widget(name, pWin);
	if (nullptr == pWin) {
		std::cerr << "Failed to Initialize MainWindow!" << std::endl;
		return true;
	}

	if (SettingsDlg.Init(builder, "SettingsDialog", pWin))
		return true;

	builder->get_widget("QuitButton", pQuitButton);
	pQuitButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuitButton_clicked));

	builder->get_widget("SettingsButton", pSettingsButton);
	pSettingsButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_SettingsButton_clicked));

	builder->get_widget("QuickKeyButton", pQuickKeyButton);
	builder->get_widget("LinkButton", pLinkButton);
	builder->get_widget("UnlinkButton", pUnlinkButton);
	builder->get_widget("RouteActionButton", pRouteActionButton);
	builder->get_widget("RouteComboBox", pRouteComboBox);
	builder->get_widget("RouteEntry", pRouteEntry);
	builder->get_widget("RouteRadioButton", pRouteRadioButton);
	builder->get_widget("LinkRadioButton", pLinkRadioButton);
	builder->get_widget("LinkEntry", pLinkEntry);
	builder->get_widget("EchoTestToggleButton", pEchoTestToggleButton);
	builder->get_widget("PTTToggleButton", pPTTToggleButton);
	//builder->get_widget("LogTextBuffer", pLogTextBuffer);
	builder->get_widget("LogTextView", pLogTextView);
	// events
	pRouteActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteActionButton_clicked));
	pRouteComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteComboBox_changed));
	pRouteEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteEntry_changed));
	ReadRoutes();

	return false;
}

void CMainWindow::Run()
{
	theApp->run(*pWin);
}

void CMainWindow::on_QuitButton_clicked()
{
	if (pWin)
		pWin->hide();
}

void CMainWindow::on_SettingsButton_clicked()
{
	SettingsDlg.Show();
}

void CMainWindow::ReadRoutes()
{
	const char *homedir = getenv("HOME");
	if (NULL == homedir)
		homedir = getpwuid(getuid())->pw_dir;

	if (homedir) {
		std::string filename(homedir);
		filename.append("/.config/qndv/routes.cfg");
		std::ifstream file(filename.c_str(), std::ifstream::in);
		if (file.is_open()) {
			char line[128];
			while (!file.eof()) {
				file.getline(line, 128);
				if ('#' != *line) {
					routeset.insert(line);
				}
			}
			file.close();
			for (auto it=routeset.begin(); it!=routeset.end(); it++)
				pRouteComboBox->append(*it);
			return;
		}
	}
	routeset.insert("DSTAR3");
	routeset.insert("DSTAR3 T");
	routeset.insert("DSTAR2");
	routeset.insert("DSTAR2 T");
	routeset.insert("DSTAR4");
	routeset.insert("DSTAR4 T");
	routeset.insert("DSTAR1");
	routeset.insert("DSTAR1 T");
	routeset.insert("QNET20 C");
	routeset.insert("QNET20 Z");
	for (auto it=routeset.begin(); it!=routeset.end(); it++)
		pRouteComboBox->append(*it);
	pRouteComboBox->set_active(0);
}

void CMainWindow::on_RouteEntry_changed()
{
	int pos = pRouteEntry->get_position();
	Glib::ustring s = pRouteEntry->get_text().uppercase();
	pRouteEntry->set_text(s);
	pRouteEntry->set_position(pos);
	pRouteActionButton->set_sensitive(s.size() ? true : false);
	pRouteActionButton->set_label((routeset.end() == routeset.find(s)) ? "Add to list" : "Delete from list");
}

void CMainWindow::on_RouteComboBox_changed()
{
	pRouteEntry->set_text(pRouteComboBox->get_active_text());
}

void CMainWindow::on_RouteActionButton_clicked()
{
	if (pRouteActionButton->get_label().compare(0,3, "Add")) {
		// deleting an entry
		auto todelete = pRouteEntry->get_text();
		int index = pRouteComboBox->get_active_row_number();
		pRouteComboBox->remove_text(index);
		routeset.erase(todelete);
		if (index >= int(routeset.size()))
			index--;
		pRouteComboBox->set_active(index);
	} else {
		// adding an entry
		auto toadd = pRouteEntry->get_text();
		routeset.insert(toadd);
		pRouteComboBox->remove_all();
		for (auto it=routeset.begin(); it!=routeset.end(); it++)
			pRouteComboBox->append(*it);
		pRouteComboBox->set_active_text(toadd);
	}
}
