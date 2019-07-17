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
#include <iomanip>
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
extern bool GetCfgDirectory(std::string &path);

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
	logdata.clear();
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
		if (pLink) {
			pLink->keep_running = false;
			futLink.get();
		}
		if (nullptr == pGate)
			futGate = std::async(std::launch::async, &CMainWindow::RunGate, this);
	}
}

bool CMainWindow::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name)
{
	if (Gate2AM.Open("gate2am"))
		return true;

	if (Link2AM.Open("link2am")) {
		Gate2AM.Close();
		return true;
	}

	if (LogInput.Open("log_input")) {
		Gate2AM.Close();
		Link2AM.Close();
		return true;
	}

 	builder->get_widget(name, pWin);
	if (nullptr == pWin) {
		Link2AM.Close();
		Gate2AM.Close();
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
	builder->get_widget("ScrolledWindow", pScrolledWindow);
	builder->get_widget("LogTextView", pLogTextView);
	pLogTextBuffer = pLogTextView->get_buffer();

	// events
	pRouteActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteActionButton_clicked));
	pRouteComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteComboBox_changed));
	pRouteEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteEntry_changed));
	pEchoTestButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_EchoTestButton_toggled));
	pPTTButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_PTTButton_toggled));
	pLinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_LinkButton_clicked));
	pUnlinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_UnlinkButton_clicked));
	pLinkEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_LinkEntry_changed));
	ReadRoutes();
	SetState(cfgdata);

	// i/o events
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::RelayGate2AM), Gate2AM.GetFD(), Glib::IO_IN);
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::RelayLink2AM), Link2AM.GetFD(), Glib::IO_IN);
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::GetLogInput), LogInput.GetFD(), Glib::IO_IN);
	// idle processing
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &CMainWindow::TimeoutProcess), 50);

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
	std::string path;

	if ( !GetCfgDirectory(path)) {
		path.append("routes.cfg");
		std::ifstream file(path.c_str(), std::ifstream::in);
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
	const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pRouteEntry->set_text(n);
	pRouteEntry->set_position(pos);
	pRouteActionButton->set_sensitive(n.size() ? true : false);
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

void CMainWindow::Receive(bool is_rx)
{
	pPTTButton->set_sensitive(!is_rx);
	pEchoTestButton->set_sensitive(!is_rx);
	pQuickKeyButton->set_sensitive(!is_rx);
}

void CMainWindow::on_PTTButton_toggled()
{
	bool is_link = pLinkRadioButton->get_active();
	if (pPTTButton->get_active()) {
		if (is_link)
			AudioManager.RecordMicThread(E_PTT_Type::link, "CQCQCQ  ");
		else
			AudioManager.RecordMicThread(E_PTT_Type::gateway, pRouteEntry->get_text().c_str());
	} else
		AudioManager.KeyOff();
}

bool CMainWindow::RelayLink2AM(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		CDSVT dsvt;
		Link2AM.Read(dsvt.title, 56);
		if (0 == memcmp(dsvt.title, "DSVT", 4))
			AudioManager.Link2AudioMgr(dsvt);
		else if (0 == memcmp(dsvt.title, "PLAY", 4))
			AudioManager.PlayFile((char *)&dsvt.config);
	} else {
		std::cerr << "RelayLink2AM not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::RelayGate2AM(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		CDSVT dsvt;
		Gate2AM.Read(dsvt.title, 56);
		if (0 == memcmp(dsvt.title, "DSVT", 4))
			AudioManager.Gateway2AudioMgr(dsvt);
		else if (0 == memcmp(dsvt.title, "PLAY", 4))
			AudioManager.PlayFile((char *)&dsvt.config);
	} else {
		std::cerr << "RelayGate2AM not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::GetLogInput(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		char line[256];
		LogInput.Read(line, 128);
		logdata.push_back(line);
		while (logdata.size()>100)
			logdata.pop_front();
		std::string current;
		for (auto it=logdata.begin(); it!=logdata.end(); it++)
			current.append(*it);
		pLogTextBuffer->set_text(current.c_str());
	} else {
		std::cerr << "GetLogInput is not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::TimeoutProcess()
{
	// check the status file for changes
	static double lasttime = 0.0;
	std::string path;
	if (! GetCfgDirectory(path)) {
		path.append("status");
		// get the last modified time
		struct stat sbuf;
		if (! stat(path.c_str(), &sbuf)) {
			double mtime = sbuf.st_mtim.tv_sec + (sbuf.st_mtim.tv_nsec / 1.0e9);
			if (mtime > lasttime) {
				// time to update!
				lasttime = mtime;
				std::ifstream status(path.c_str(), std::ifstream::in);
				if (status.is_open()) {
					std::string cs, mod;
					std::getline(status, cs, ',');
					std::getline(status, cs, ',');
					std::getline(status, mod, ',');
					status.close();
					if (8==cs.size() && 1==mod.size()) {
						cs.resize(7);
						cs.append(mod);
						pLinkEntry->set_text(cs.c_str());
						pLinkEntry->set_sensitive(false);
						pLinkButton->set_sensitive(false);
						pUnlinkButton->set_sensitive(true);
					} else {
						pLinkEntry->set_text("");
						pLinkEntry->set_sensitive(true);
						pLinkButton->set_sensitive(false);
						pUnlinkButton->set_sensitive(false);
					}
				}
			}
		}
	}
	return true;
}

void CMainWindow::on_LinkEntry_changed()
{
	int pos = pLinkEntry->get_position();
	Glib::ustring s = pLinkEntry->get_text().uppercase();
	const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pLinkEntry->set_text(n);
	pLinkEntry->set_position(pos);
	std::string str(n.c_str());
	str.resize(7, ' ');
	str.append(1, ' ');
	if (8==n.size() && isalpha(n.at(7)) && gwys.hostmap.end() != gwys.hostmap.find(str)) {
		if (pLinkEntry->get_sensitive())
			pLinkButton->set_sensitive(true);
	} else
		pLinkButton->set_sensitive(false);
}

void CMainWindow::on_LinkButton_clicked()
{
	if (pLink) {
		std::string cs(pLinkEntry->get_text().c_str());
		char mod = cs.at(7);
		cs.at(7) = ' ';
		pLink->Link(cs.c_str(), mod);
	}
}

void CMainWindow::on_UnlinkButton_clicked()
{
	if (pLink)
		pLink->Unlink();
}
