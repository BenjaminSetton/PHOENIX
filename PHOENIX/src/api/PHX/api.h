#pragma once

/*
	PHX_API export/import macro.

	When building PHX as a shared library (DLL):
		- Define PHX_DYNAMIC_LIB when including phx.h, and set SharedLib in Premake config

	When building PHX as a static library:
		- Neither macro is defined in code or Premake config, this is the default
*/

#if defined(PHX_WINDOWS)
	#ifdef PHX_DYNAMIC_LIB
		#ifdef PHX_BUILDING_LIB
			#define PHX_API __declspec(dllexport)
		#else
			#define PHX_API __declspec(dllimport)
		#endif
	#else
		#define PHX_API
	#endif
#else
	#define PHX_API
#endif
