// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

#include "GradientspacePlatform.h"


#ifdef GSIO_EMBEDDED_UE_BUILD

#include "HAL/Platform.h"

#else   // ! GSIO_EMBEDDED_UE_BUILD

#ifdef GRADIENTSPACEIO_EXPORTS
#define GRADIENTSPACEIO_API __declspec(dllexport)
#else
#define GRADIENTSPACEIO_API __declspec(dllimport)
#endif


// disable some warnings (UE disables these warnings too)
#pragma warning(disable: 4251)		// X needs to have dll-interface to be used by clients of class X. 
									// This warning happens when an exported class/struct/etc has template members
									// conceivably it's possible to export the template instantiations...but messy


#endif

