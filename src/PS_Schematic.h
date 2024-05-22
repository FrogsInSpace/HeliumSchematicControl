/*
Schematic Maxscript UI control
Copyright (C) 2005 Kees Rijnen (kees@lumonix.net)
www.lumonix.net

This is a new UI widget that can be added to maxscript rollouts and controlled via maxscript.
It represents a generic schematic UI tool for 3dsmax that can be used for various scripted tools.

Special Thanks to:
- Larry Minton
- Simon Feltman (for sharing source like imgTag)
- All companies that have bought the source code over the years.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _PSSCHEMATIC_		// makes it so this header doesn't get run twice by accident
#define _PSSCHEMATIC_

/* ===========================================================================
	Includes and defines
   =========================================================================== */

#define NODEMOVEDRAWTH 5;

#include <max.h>	// needed for gport below:
#include <gport.h>	// need for rendering renderBalls (pStamp)

#include <maxscript/maxscript.h>
#include <maxscript\maxwrapper\mxsobjects.h>
#include <maxscript/compiler/parser.h>
#include <maxscript/foundation/3dmath.h>	// for Point2Value
#include <maxscript/foundation/numbers.h>	// for intern
#include <maxscript/foundation/colors.h>
#include <maxscript/maxwrapper/bitmaps.h>	// for background image
#include <maxscript/UI/rollouts.h>

   // below is needed because we are dealing with maxscript:
#ifdef ScripterExport
#undef ScripterExport
#endif
#define ScripterExport __declspec( dllexport )

// this defines def_name, so we need it here:
#include <maxscript\macros\define_instantiation_functions.h>

#include "PS_Events.h"			// events that get send to maxscript rollout
#include "PS_Nodes.h"			// schematic node definitions

// window class:
#define SCHEMATIC_WINDOWCLASS _T("SCHEMATIC_WINDOWCLASS")

// below will be defined later, we tell the plugin that it will be by using extern
// this function is used in libInit to register our rollout control:
extern void SchematicInit();

/* ===========================================================================
	Class Definitions:
   =========================================================================== */

   /* ----------------------- Schematic Control UI -------------------------- */

visible_class(SchematicControl)	// Macro

#include "PS_SchematicControl.h"

visible_class_instance(SchematicControl, "SchematicControl")	// Macro for RolloutControls

#endif