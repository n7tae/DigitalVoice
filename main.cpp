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

#include <gtkmm.h>
#include <iostream>
#include <string>

#include "MainWindow.h"
#include "AudioManager.h"
#include "Configure.h"

// Globals
extern CMainWindow MainWindow;
extern Glib::RefPtr<Gtk::Application> theApp;
extern CConfigure cfg;
extern bool GetCfgDirectory(std::string &);

int main (int argc, char **argv)
{
	std::string path;
	if (GetCfgDirectory(path)) {
		std::cerr << "Error: can't find a path to the configuration directory!" << std::endl;
		std::cerr << "Make sure you have installed the application: \"make install\"" << std::endl;
		return 1;
	}

	theApp = Gtk::Application::create(argc, argv, "net.openquad.QnetDV");

	//Load the GtkBuilder file and instantiate its widgets:
	Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
	try
	{
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

	if (MainWindow.Init(builder, "AppWindow"))
		return 1;

	MainWindow.Run();

	return 0;
}
