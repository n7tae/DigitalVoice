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
#include <thread>
#include <chrono>

#include "SettingsDlg.h"
#include "MainWindow.h"
#include "AudioManager.h"
#include "HostFile.h"
#include "WaitCursor.h"

// globals
extern CSettingsDlg SettingsDlg;
extern CHostFile gwys;
extern Glib::RefPtr<Gtk::Application> theApp;
extern CAudioManager AudioManager;
extern CConfigure cfg;


CMainWindow::CMainWindow() :
	pWin(nullptr),
	pQuitButton(nullptr),
	pSettingsButton(nullptr),
	pGate(nullptr),
	pLink(nullptr)
{
	cfg.CopyTo(cfgdata);
	gwys.Init();
	if (! AudioManager.AMBEDevice.IsOpen()) {
		AudioManager.AMBEDevice.FindandOpen(cfgdata.iBaudRate, DSTAR_TYPE);
	}
}

CMainWindow::~CMainWindow()
{
	if (pWin)
		delete pWin;
	if (pLink) {
		pLink->keep_running = false;
		futLink.get();
	}
	if (pGate) {
		pGate->keep_running = false;
		futGate.get();
	}
}

void CMainWindow::RunLink()
{
	pLink = new CQnetLink;
	if (! pLink->Init())
		pLink->Process();
	pLink->Shutdown();
	delete pLink;
	pLink = nullptr;
}

void CMainWindow::RunGate()
{
	pGate = new CQnetGateway;
	if (! pGate->Init())
		pGate->Process();
	delete pGate;
	pGate = nullptr;
}

void CMainWindow::SetState(const CFGDATA &data)
{
	if (data.eNetType == EQuadNetType::norouting) {
		pLinkRadioButton->set_active(true);
		pRouteRadioButton->set_sensitive(false);
		if (pGate) {
			pGate->keep_running = false;
			futGate.get();
		}
		if (nullptr == pLink) {
			futLink = std::async(std::launch::async, &CMainWindow::RunLink, this);
		}
	} else {
		pRouteRadioButton->set_sensitive(true);
		if (nullptr == pLink)
			futLink = std::async(std::launch::async, &CMainWindow::RunLink, this);
		if (nullptr == pGate)
			futGate = std::async(std::launch::async, &CMainWindow::RunGate, this);
	}
}

bool CMainWindow::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name)
{
 	builder->get_widget(name, pWin);
	if (nullptr == pWin) {
		std::cerr << "Failed to Initialize MainWindow!" << std::endl;
		return true;
	}

	//setup our css context and provider
	Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
	Glib::RefPtr<Gtk::StyleContext> style = Gtk::StyleContext::create();

	//load our red clicked style (applies to Gtk::ToggleButton)
	if (css->load_from_data("button:checked { background: red; }")) {
		style->add_provider_for_screen(pWin->get_screen(), css, GTK_STYLE_PROVIDER_PRIORITY_USER);
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
	builder->get_widget("EchoTestButton", pEchoTestButton);
	builder->get_widget("PTTButton", pPTTButton);
	//builder->get_widget("LogTextBuffer", pLogTextBuffer);
	builder->get_widget("LogTextView", pLogTextView);
	// events
	pRouteActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteActionButton_clicked));
	pRouteComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteComboBox_changed));
	pRouteEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteEntry_changed));
	pEchoTestButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_EchoTestButton_toggled));
	ReadRoutes();
	SetState(cfgdata);

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

void CMainWindow::on_EchoTestButton_toggled()
{
	if (pEchoTestButton->get_active()) {
		// record the mic to a queue
		AudioManager.RecordMicThread(E_PTT_Type::echo, "CQCQCQ  ");
		//std::cout << "AM.RecordMicThread() returned\n";
	} else {
		// play back the queue
		AudioManager.PlayAMBEDataThread();
		//std::cout << "AM.PlayAMBEDataThread() returned\n";
	}
}
