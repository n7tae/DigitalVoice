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

#include "SettingsDlg.h"

Gtk::Window *pWindow = nullptr;

SettingsDlg settings;

static void on_QuitButton_clicked()
{
  if(pWindow)
    pWindow->hide(); //hide() will cause main::run() to end.
}

static void on_SettingsButton_clicked()
{
    settings.show();
}

int main (int argc, char **argv)
{
  auto app = Gtk::Application::create(argc, argv, "org.gtkmm.example");

  //Load the GtkBuilder file and instantiate its widgets:
  auto refBuilder = Gtk::Builder::create();
  try
  {
    refBuilder->add_from_file("QnetDV.glade");
  }
  catch(const Glib::FileError& ex)
  {
    std::cerr << "FileError: " << ex.what() << std::endl;
    return 1;
  }
  catch(const Glib::MarkupError& ex)
  {
    std::cerr << "MarkupError: " << ex.what() << std::endl;
    return 1;
  }
  catch(const Gtk::BuilderError& ex)
  {
    std::cerr << "BuilderError: " << ex.what() << std::endl;
    return 1;
  }

  //Get the GtkBuilder-instantiated Window:
  refBuilder->get_widget("AppWindow", pWindow);
  refBuilder->get_widget("SettingsDialog", settings.pWindow);

  if(pWindow)
  {
    //Get the GtkBuilder-instantiated Button, and connect a signal handler:
    Gtk::Button *pQuitButton = nullptr;
    refBuilder->get_widget("QuitButton", pQuitButton);
    if(pQuitButton)
    {
      pQuitButton->signal_clicked().connect( sigc::ptr_fun(on_QuitButton_clicked));
    }

    Gtk::Button *pSettingsButton = nullptr;
    refBuilder->get_widget("SettingsButton", pSettingsButton);
    if (pSettingsButton) {
        pSettingsButton->signal_clicked().connect(sigc::ptr_fun(on_SettingsButton_clicked));
    }

    app->run(*pWindow);
  }

  delete pWindow;

  return 0;
}
