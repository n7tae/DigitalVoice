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

#include <iostream>

#include "Defines.h"
#include "SettingsDlg.h"
#include "MainWindow.h"

// globals
CSettingsDlg SettingsDlg;
extern Glib::RefPtr<Gtk::Application> theApp;

CMainWindow::CMainWindow() :
	pWin(nullptr),
	pQuit(nullptr),
	pSettings(nullptr)
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

	GET_WIDGET("QuitButton", pQuit);
	pQuit->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuitButton_clicked));

	GET_WIDGET("SettingsButton", pSettings);
	pSettings->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_SettingsButton_clicked));

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
