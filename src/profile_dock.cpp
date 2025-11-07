#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include "log.hpp"
#include "profile_dock.hpp"

#ifndef PLUGIN_VERSION_STR
#define PLUGIN_VERSION_STR "1.0.1-quicksettings"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("profile-switcher", "en-US")

static ProfileDockWidget *g_dockWidget = nullptr;
static const char *kDockId = "profile-switcher-dock";
// Add a version tag to the title so you can SEE it changed
static const char *kDockTitle = "Profile Switcher — QS 1.0.1";

const char *obs_module_description(void)
{
	return "ProfileSwitcher: Dock to switch/create/delete profiles with quick settings";
}

bool obs_module_load(void)
{
	g_dockWidget = new ProfileDockWidget;
	if (!obs_frontend_add_dock_by_id(kDockId, kDockTitle, (void *)g_dockWidget)) {
		PLOG(LOG_ERROR, "Failed to add dock by id '%s'", kDockId);
		delete g_dockWidget;
		g_dockWidget = nullptr;
		return false;
	}

	obs_frontend_add_event_callback([](enum obs_frontend_event ev, void *) {
		switch (ev) {
		case OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED:
		case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		case OBS_FRONTEND_EVENT_PROFILE_RENAMED:
			if (g_dockWidget) g_dockWidget->refreshProfiles();
			break;
		default:
			break;
		}
	}, nullptr);

	PLOG(LOG_INFO, "ProfileSwitcher loaded: %s (requires OBS >= %d)", PLUGIN_VERSION_STR, PLUGIN_MIN_OBS_MAJOR);
	return true;
}

void obs_module_unload(void)
{
	if (g_dockWidget) {
		obs_frontend_remove_dock(kDockId);
		delete g_dockWidget;
		g_dockWidget = nullptr;
	}
	PLOG(LOG_INFO, "ProfileSwitcher unloaded: %s", PLUGIN_VERSION_STR);
}
