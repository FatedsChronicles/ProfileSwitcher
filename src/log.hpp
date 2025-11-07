#pragma once
#include <obs-module.h>

#ifndef PLOG
#define PLOG(level, fmt, ...) blog(level, "[%s] " fmt, PLUGIN_NAME_STR, ##__VA_ARGS__)
#endif
