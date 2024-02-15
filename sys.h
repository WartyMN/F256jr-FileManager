//! @file lib_sys.h

/*
 * lib_sys.h
 *
*  Created on: Mar 22, 2022
 *      Author: micahbly
 */

// THIS IS A CUT-DOWN VERSION OF the OS/f lib_sys.h file, just enough to power Lich King
// adapted for Foenix F256 Jr starting November 29, 2022



#ifndef LIB_SYS_H_
#define LIB_SYS_H_


/* about this library: System
 *
 * This provides overall system level functionality
 *
 *** things this library needs to be able to do
 * Manage global system resources such as fonts, screens, mouse pointer, etc. 
 * Provide event handling
 *
 * STRETCH GOALS
 * 
 *
 * SUPER STRETCH GOALS
 * 
 * 
 */


/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes
#include "general.h"

// C includes
#include <stdbool.h>

#include "f256.h"


/*****************************************************************************/
/*                            Macro Definitions                              */
/*****************************************************************************/



/*****************************************************************************/
/*                               Enumerations                                */
/*****************************************************************************/



/*****************************************************************************/
/*                                 Structs                                   */
/*****************************************************************************/

typedef struct System
{
	uint8_t			model_number_;
	uint8_t			text_cols_vis_;		// accounting for borders, the number of visible columns on screen
	uint8_t			text_rows_vis_;		// accounting for borders, the number of visible rows on screen
	uint8_t			text_mem_cols_;		// for the current resolution, the total number of columns per row in VRAM. Use for plotting x,y 
	uint8_t			text_mem_rows_;		// for the current resolution, the total number of rows per row in VRAM. Use for plotting x,y 
// 	char*			text_ram_;
// 	char*			text_attr_ram_;
// 	char*			text_font_ram_;			// 2K of memory holding font definitions.
// 	char*			text_color_fore_ram_;	// 64b of memory holding foreground color LUTs for text mode, in BGRA order
// 	char*			text_color_back_ram_;	// 64b of memory holding background color LUTs for text mode, in BGRA order
} System;





/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/


/*****************************************************************************/
/*                       Public Function Prototypes                          */
/*****************************************************************************/


// **** CONSTRUCTOR AND DESTRUCTOR *****




// **** System Initialization functions *****

//! Initialize the system (primary entry point for all system initialization activity)
//! Starts up the memory manager, creates the global system object, runs autoconfigure to check the system hardware, loads system and application fonts, allocates a bitmap for the screen.
bool Sys_InitSystem(void);



// **** Event-handling functions *****




// **** Screen mode/resolution/size functions *****

//! Find out what kind of machine the software is running on, and determine # of screens available
//! @param	the_system: valid pointer to system object
//! @return	Returns false if the machine is known to be incompatible with this software. 
bool Sys_AutoDetectMachine(void);

//! Find out what kind of machine the software is running on, and configure the passed screen accordingly
//! Configures screen settings, RAM addresses, etc. based on known info about machine types
//! Configures screen width, height, total text rows and cols, and visible text rows and cols by checking hardware
//! @param	the_system: valid pointer to system object
//! @return	Returns false if the machine is known to be incompatible with this software. 
bool Sys_AutoConfigure(void);

//! Detect the current screen mode/resolution, and set # of columns, rows, H pixels, V pixels, accordingly
//! @param	the_screen: valid pointer to the target screen to operate on
//! @return	returns false on any error/invalid input.
bool Sys_DetectScreenSize(void);

//! Change video mode to the one passed.
//! @param	the_screen: valid pointer to the target screen to operate on
//! @param	new_mode: One of the enumerated screen_resolution values. Must correspond to a valid VICKY video mode for the host machine. See VICKY_IIIA_RES_800X600_FLAGS, etc. defined in a2560_platform.h
//! @return	returns false on any error/invalid input.
bool Sys_SetVideoMode(uint8_t new_mode);

//! Switch machine into text mode
//! @param	the_system: valid pointer to system object
//! @param as_overlay: If true, sets text overlay mode (text over graphics). If false, sets full text mode (no graphics);
//! @return	returns false on any error/invalid input.
void Sys_SetModeText(bool as_overlay);

//! Enable or disable the hardware cursor in text mode, for the specified screen
//! @param	the_system: valid pointer to system object
//! @param	the_screen: valid pointer to the target screen to operate on
//! @param enable_it: If true, turns the hardware blinking cursor on. If false, hides the hardware cursor;
//! @return	returns false on any error/invalid input.
void Sys_EnableTextModeCursor(bool enable_it);

//! Set the left/right and top/bottom borders
//! This will reset the visible text columns as a side effect
//! @param	border_width: width in pixels of the border on left and right side of the screen. Total border used with be the double of this.
//! @param	border_height: height in pixels of the border on top and bottom of the screen. Total border used with be the double of this.
//! @return	returns false on any error/invalid input.
void Sys_SetBorderSize(uint8_t border_width, uint8_t border_height);






// **** Other GET functions *****




// **** Other SET functions *****



// **** xxx functions *****




// **** xxx functions *****








// **** TEMP font loading *****



// **** Render functions *****




// **** Tiny VICKY I/O page functions *****

// disable the I/O bank to allow RAM to be mapped into it
// current MMU setting is saved to the 6502 stack
void Sys_DisableIOBank(void);

// change the I/O page
void Sys_SwapIOPage(uint8_t the_page_number);

// restore the previous MMU setting, which was saved by Sys_SwapIOPage()
void Sys_RestoreIOPage(void);




// **** Debug functions *****

// void Sys_Print(System* the_system);

// void Sys_PrintScreen(Screen* the_screen);



#endif /* LIB_SYS_H_ */


