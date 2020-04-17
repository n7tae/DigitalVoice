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

#include "AudioManager.h"
#include "Configure.h"
#include "HostFile.h"
#include "MainWindow.h"
#include "SettingsDlg.h"

CAudioManager AudioManager;
CConfigure cfg;
CHostFile gwys;
CMainWindow MainWindow;
Glib::RefPtr<Gtk::Application> theApp;
CSettingsDlg SettingsDlg;


bool GetCfgDirectory(std::string &dir)
{
	const char *homedir = getenv("HOME");

	if (NULL == homedir)
		homedir = getpwuid(getuid())->pw_dir;

	if (NULL == homedir) {
		std::cerr << "ERROR: could not find a home directory!" << std::endl;
		return true;
	}
	dir.assign(homedir);
	dir.append("/etc/");
	return false;
}
