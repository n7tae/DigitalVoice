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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>

#include "MainWindow.h"
#include "WaitCursor.h"
#include "DPlusAuthenticator.h"
#include "Utilities.h"
#include "TemplateClasses.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

static Glib::RefPtr<Gtk::Application> theApp;

CMainWindow::CMainWindow() :
	pWin(nullptr),
	pQuitButton(nullptr),
	pSettingsButton(nullptr),
	pGate(nullptr),
	pLink(nullptr)
{
	cfg.CopyTo(cfgdata);
	if (! AudioManager.AMBEDevice.IsOpen()) {
		AudioManager.AMBEDevice.FindandOpen(cfgdata.iBaudRate, DSTAR_TYPE);
	}
}

CMainWindow::~CMainWindow()
{
	if (pWin)
		delete pWin;
	if (nullptr != pLink) {
		pLink->keep_running = false;
		futLink.get();
	}
	if (nullptr != pGate) {
		pGate->keep_running = false;
		futGate.get();
	}
}

void CMainWindow::RunLink()
{
	pLink = new CQnetLink;
	if (! pLink->Init(&cfgdata))
		pLink->Process();
	delete pLink;
	pLink = nullptr;
}

void CMainWindow::RunGate()
{
	pGate = new CQnetGateway;
	if (! pGate->Init(&cfgdata))
		pGate->Process();
	delete pGate;
	pGate = nullptr;
}

void CMainWindow::SetState(const CFGDATA &data)
{
	if (data.bRouteEnable) {
		if (nullptr == pGate && cfg.IsOkay())
			futGate = std::async(std::launch::async, &CMainWindow::RunGate, this);
		pRouteComboBox->set_sensitive(true);
		pRouteActionButton->set_sensitive(true);
	} else {
		if (nullptr != pGate) {
			pGate->keep_running = false;
			futGate.get();
		}
		pRouteEntry->set_text("CQCQCQ");
		pRouteEntry->set_sensitive(false);
		pRouteComboBox->set_sensitive(false);
		pRouteActionButton->set_sensitive(false);
	}
	if (data.bLinkEnable) {
		if (nullptr == pLink)
			futLink = std::async(std::launch::async, &CMainWindow::RunLink, this);
		std::list<CLink> linklist;
		if (qnDB.FindLS(cfgdata.cModule, linklist)) {
			std::cerr << "MainWindow::SetSet FindLS failed!" << std::endl;
		} else {
			if (linklist.size()) {
				auto link = linklist.front();
				pLinkEntry->set_text(link.callsign.c_str());
				pLinkEntry->set_sensitive(false);
				pLinkButton->set_sensitive(false);
				pUnlinkButton->set_sensitive(true);
			} else {
				pLinkEntry->set_sensitive(true);
				pUnlinkButton->set_sensitive(false);
				const std::string entry(pLinkEntry->get_text().c_str());
				if (8==entry.size() && qnDB.FindGW(entry.substr(0, 6).c_str()) && isalpha(entry.at(7)))
					pLinkButton->set_sensitive(true);
				else
					pLinkButton->set_sensitive(false);

			}
		}
	} else {
		if (nullptr != pLink) {
			pLink->keep_running = false;
			futLink.get();
		}
		pLinkButton->set_sensitive(false);
		pLinkEntry->set_sensitive(false);
		pUnlinkButton->set_sensitive(false);
	}
}

bool CMainWindow::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name)
{
	std::string dbname(CFG_DIR);
	dbname.append("qn.db");
	if (qnDB.Open(dbname.c_str()) || qnDB.Init())
		return true;
	RebuildGateways(cfgdata.bDPlusEnable);

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

	if (AudioManager.Init(this)) {
		LogInput.Close();
		Gate2AM.Close();
		Link2AM.Close();
		return true;
	}

 	builder->get_widget(name, pWin);
	if (nullptr == pWin) {
		LogInput.Close();
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

	if (SettingsDlg.Init(builder, "SettingsDialog", pWin, this))
		return true;

	builder->get_widget("QuitButton", pQuitButton);
	builder->get_widget("SettingsButton", pSettingsButton);
	builder->get_widget("LinkButton", pLinkButton);
	builder->get_widget("UnlinkButton", pUnlinkButton);
	builder->get_widget("RouteActionButton", pRouteActionButton);
	builder->get_widget("RouteComboBox", pRouteComboBox);
	builder->get_widget("RouteEntry", pRouteEntry);
	builder->get_widget("LinkEntry", pLinkEntry);
	builder->get_widget("EchoTestButton", pEchoTestButton);
	builder->get_widget("PTTButton", pPTTButton);
	builder->get_widget("QuickKeyButton", pQuickKeyButton);
	builder->get_widget("ScrolledWindow", pScrolledWindow);
	builder->get_widget("LogTextView", pLogTextView);
	pLogTextBuffer = pLogTextView->get_buffer();

	// events
	pSettingsButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_SettingsButton_clicked));
	pQuitButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuitButton_clicked));
	pRouteActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteActionButton_clicked));
	pRouteComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteComboBox_changed));
	pRouteEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteEntry_changed));
	pEchoTestButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_EchoTestButton_toggled));
	pPTTButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_PTTButton_toggled));
	pQuickKeyButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuickKeyButton_clicked));
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
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &CMainWindow::TimeoutProcess), 1000);

	return false;
}

void CMainWindow::Run()
{
	theApp->run(*pWin);
}

void CMainWindow::on_QuitButton_clicked()
{
	if (nullptr != pGate) {
		pGate->keep_running = false;
		futGate.get();
		pGate = nullptr;
	}

	if (nullptr != pLink) {
		pLink->keep_running = false;
		futLink.get();
		pLink = nullptr;
	}

	if (pWin)
		pWin->hide();
}

void CMainWindow::on_SettingsButton_clicked()
{
	auto newdata = SettingsDlg.Show();
	if (newdata) {	// the user clicked okay so we need to see if anything changed. We'll shut things down and let SetState start things up again
		if (newdata->sStation.compare(cfgdata.sCallsign) || newdata->cModule!=cfgdata.cModule) {	// the station callsign has changed
			if (nullptr != pGate) {
				pGate->keep_running = false;
				futGate.get();
				pGate = nullptr;
			}
			if (nullptr != pLink) {
				pLink->keep_running = false;
				futLink.get();
				pLink = nullptr;
			}
		}
		else if (newdata->eNetType != cfgdata.eNetType) {
			if (nullptr != pGate) {
				pGate->keep_running = false;
				futGate.get();
				pGate = nullptr;
			}
		}
		if (! newdata->bLinkEnable && nullptr!=pLink) {
			pLink->keep_running = false;
			futLink.get();
			pLink = nullptr;
		}
		if (! newdata->bRouteEnable && nullptr!=pGate) {
			pGate->keep_running = false;
			futGate.get();
			pGate = nullptr;
		}
		SetState(*newdata);
		cfg.CopyTo(cfgdata);
	}
}

void CMainWindow::WriteRoutes()
{
	std::string path(CFG_DIR);
	path.append("routes.cfg");
	std::ofstream file(path.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (! file.is_open())
		return;
	for (auto it=routeset.begin(); it!=routeset.end(); it++) {
		file << *it << std::endl;
	}
	file.close();
}

void CMainWindow::ReadRoutes()
{
	std::string path(CFG_DIR);

	path.append("routes.cfg");
	std::ifstream file(path.c_str(), std::ifstream::in);
	if (file.is_open()) {
		char line[128];
		while (file.getline(line, 128)) {
			if ('#' != *line) {
				routeset.insert(line);
			}
		}
		file.close();
		for (auto it=routeset.begin(); it!=routeset.end(); it++) {
			pRouteComboBox->append(*it);
		}
		pRouteComboBox->set_active(0);
		return;
	}

	routeset.insert("CQCQCQ");
	routeset.insert("DSTAR1");
	routeset.insert("DSTAR1 T");
	routeset.insert("DSTAR2");
	routeset.insert("DSTAR2 T");
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
	WriteRoutes();
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
	const std::string urcall(pRouteEntry->get_text().c_str());
	bool is_cqcqcq = (0 == urcall.compare(0, 6, "CQCQCQ"));

	if ((! is_cqcqcq && cfgdata.bRouteEnable) || (is_cqcqcq && cfgdata.bLinkEnable)) {
		if (pPTTButton->get_active()) {
			if (is_cqcqcq)
				AudioManager.RecordMicThread(E_PTT_Type::link, "CQCQCQ");
			else
				AudioManager.RecordMicThread(E_PTT_Type::gateway, pRouteEntry->get_text().c_str());
		} else
			AudioManager.KeyOff();
	}
}

void CMainWindow::on_QuickKeyButton_clicked()
{
	AudioManager.QuickKey(pRouteEntry->get_text().c_str());
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
	static auto it = pLogTextBuffer->begin();
	if (condition & Glib::IO_IN) {
		char line[256];
		LogInput.Read(line, 256);
		it = pLogTextBuffer->insert(it, line);
		pLogTextView->scroll_to(it, 0.0, 0.0, 1.0);
	} else {
		std::cerr << "GetLogInput is not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::TimeoutProcess()
{
	std::list<CLink> linkstatus;
	if (qnDB.FindLS(cfgdata.cModule, linkstatus))	// get the link status list of our module (there should only be one, or none if it's not linked)
		return true;

	std::string call;
	if (linkstatus.size()) {	// extract it from the returned list, no list means our module is not linked!
		CLink ls(linkstatus.front());
		call.assign(ls.callsign);
	}

	if (call.empty()) {
		pLinkEntry->set_sensitive(true);
		pUnlinkButton->set_sensitive(false);
		on_LinkEntry_changed();
	} else {
		pLinkEntry->set_sensitive(false);
		pLinkButton->set_sensitive(false);
		pUnlinkButton->set_sensitive(true);
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
	str.resize(6, ' ');
	if (8==n.size() && isalpha(n.at(7)) && qnDB.FindGW(str.c_str())) {
		pLinkEntry->set_icon_from_icon_name("gtk-ok");
		pLinkButton->set_sensitive(true);
	} else {
	 	pLinkEntry->set_icon_from_icon_name("gtk-cancel");
		pLinkButton->set_sensitive(false);
	}
}

void CMainWindow::on_LinkButton_clicked()
{
	if (pLink) {
		std::cout << "Pushed the Link button for " << pLinkEntry->get_text().c_str() << '.' << std::endl;
		std::string cmd("LINK");
		cmd.append(pLinkEntry->get_text().c_str());
		AudioManager.Link(cmd);
	}
}

void CMainWindow::on_UnlinkButton_clicked()
{
	if (pLink) {
		std::string cmd("LINK");
		AudioManager.Link(cmd);
	}
}

void CMainWindow::RebuildGateways(bool includelegacy)
{
	CWaitCursor WaitCursor;
	qnDB.ClearGW();
	CHostQueue qhost;

	std::string filename(CFG_DIR);	// now open the gateways text file
	filename.append("gwys.txt");
	int count = 0;
	std::ifstream hostfile(filename);
	if (hostfile.is_open()) {
		std::string line;
		while (std::getline(hostfile, line)) {
			trim(line);
			if (! line.empty() && ('#' != line.at(0))) {
				std::istringstream iss(line);
				std::string name, addr;
				unsigned short port;
				iss >> name >> addr >> port;
				CHost host(name, addr, port);
				qhost.Push(host);
				count++;
			}
		}
		hostfile.close();
		qnDB.UpdateGW(qhost);
	}

	if (includelegacy && ! cfgdata.sStation.empty()) {
		const std::string website("auth.dstargateway.org");
		CDPlusAuthenticator auth(cfgdata.sStation, website);
		int dplus = auth.Process(qnDB, true, false);
		if (0 == dplus) {
			fprintf(stdout, "DPlus Authorization failed.\n");
			printf("# of Gateways: %s=%d\n", filename.c_str(), count);
		} else {
			fprintf(stderr, "DPlus Authorization completed!\n");
			printf("# of Gateways %s=%d %s=%d Total=%d\n", filename.c_str(), count, website.c_str(), dplus, qnDB.Count("GATEWAYS"));
		}
	} else {
		printf("#Gateways: %s=%d\n", filename.c_str(), count);
	}
}

int main (int argc, char **argv)
{
	theApp = Gtk::Application::create(argc, argv, "net.openquad.QnetDV");

	//Load the GtkBuilder file and instantiate its widgets:
	Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
	try
	{
		std::string path(CFG_DIR);
		builder->add_from_file(path + "DigitalVoice.glade");
	}
	catch (const Glib::FileError& ex)
	{
		std::cerr << "FileError: " << ex.what() << std::endl;
		return 1;
	}
	catch (const Glib::MarkupError& ex)
	{
		std::cerr << "MarkupError: " << ex.what() << std::endl;
		return 1;
	}
	catch (const Gtk::BuilderError& ex)
	{
		std::cerr << "BuilderError: " << ex.what() << std::endl;
		return 1;
	}

	CMainWindow MainWindow;
	if (MainWindow.Init(builder, "AppWindow"))
		return 1;

	MainWindow.Run();

	return 0;
}
