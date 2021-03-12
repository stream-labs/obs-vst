/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.
Additional Code Copyright (C) 2016-2017 by c3r1c3 <c3r1c3@nevermindonline.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/
#include "../headers/VSTPlugin.h"
#include "../headers/vst-plugin-callbacks.hpp"

#include <util/platform.h>
#include <windows.h>
#include <string>

AEffect* VSTPlugin::getEffect() {
	return effect;
}

AEffect *VSTPlugin::loadEffect()
{
	AEffect *plugin = nullptr;
	std::string dir    = pluginPath;

	blog(LOG_WARNING, "VST Plug-in: path %s", dir.c_str());

	while (dir.back() != '/')
		dir.pop_back();

	wchar_t *wpath;
	os_utf8_to_wcs_ptr(pluginPath.c_str(), 0, &wpath);
	wchar_t *wdir;
	os_utf8_to_wcs_ptr(dir.c_str(), 0, &wdir);

	SetDllDirectory(wdir);
	dllHandle = LoadLibraryW(wpath);
	SetDllDirectory(NULL);
	bfree(wpath);
	bfree(wdir);
	if (dllHandle == nullptr) {

		DWORD errorCode = GetLastError();

		// Display the error message and exit the process
		if (errorCode == ERROR_BAD_EXE_FORMAT) {
			blog(LOG_WARNING, "VST Plug-in: Could not open library, wrong architecture.");
		} else {
			blog(LOG_WARNING, "VST Plug-in: Failed trying to load VST from '%s', error %d\n",
			     pluginPath.c_str(),
			     GetLastError());
		}
		return nullptr;
	}

	blog(LOG_WARNING, "VST Plug-in: Opening main entry point from plugin path %s", pluginPath.c_str());
	vstPluginMain mainEntryPoint = (vstPluginMain)GetProcAddress(dllHandle, "VSTPluginMain");

	if (mainEntryPoint == nullptr) {
		mainEntryPoint = (vstPluginMain)GetProcAddress(dllHandle, "VstPluginMain()");
	}

	if (mainEntryPoint == nullptr) {
		mainEntryPoint = (vstPluginMain)GetProcAddress(dllHandle, "main");
	}

	if (mainEntryPoint == nullptr) {
		blog(LOG_WARNING, "VST Plug-in: Couldn't get a pointer to plug-in's main()");
		unloadLibrary();
		return nullptr;
	}

	// Instantiate the plug-in
	plugin       = mainEntryPoint(hostCallback_static);
	if (plugin == nullptr) {
		blog(LOG_WARNING, "VST Plug-in: Failed to get filter object from a plugin");
		unloadLibrary();
		return nullptr;
	}

	plugin->user = this;
	return plugin;
}

void VSTPlugin::send_loadEffectFromPath(std::string path) {
	editorWidget->send_loadEffectFromPath(path);
}

void VSTPlugin::send_setChunk(std::string chunk)
{
	editorWidget->send_setChunk(chunk);
}

void VSTPlugin::send_unloadEffect() {

}


void VSTPlugin::unloadLibrary()
{
	if (dllHandle) {
		FreeLibrary(dllHandle);
		dllHandle = nullptr;
	}
}
