/*
 * screen.c
 *
 *  Created on: Jan 11, 2023
 *      Author: micahbly
 *
 *  Routines for drawing and updating the UI elements
 *
 */



/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes
#include "screen.h"
#include "app.h"
#include "comm_buffer.h"
#include "general.h"
#include "memory.h"
#include "sys.h"
#include "text.h"
#include "strings.h"

// C includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// F256 includes
#include "f256.h"




/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/

#define UI_BYTE_SIZE_OF_APP_TITLEBAR	240	// 3 x 80 rows for the title at top

/*****************************************************************************/
/*                           File-scope Variables                            */
/*****************************************************************************/

static File_Panel		panel[NUM_PANELS];
static UI_Button		uibutton[NUM_BUTTONS];
 
static uint8_t			app_titlebar[UI_BYTE_SIZE_OF_APP_TITLEBAR] = 
{
	148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,148,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,141,142,143,144,145,146,147,32,102,47,109,97,110,97,103,101,114,32,106,114,32,140,139,138,137,136,135,134,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,153,
};

/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/

extern char*				global_string[NUM_STRINGS];

extern uint8_t				zp_bank_num;
extern uint8_t				io_bank_value_kernel;	// stores value for the physical bank pointing to C000-DFFF whenever we change it, so we can restore it.

#pragma zpsym ("zp_bank_num");


/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/

void Screen_DrawUI(void);

/*****************************************************************************/
/*                       Private Function Definitions                        */
/*****************************************************************************/

void Screen_DrawUI(void)
{
	uint8_t		i;
	uint8_t		x1;
	uint8_t		y1;
	uint8_t		x2;
	uint8_t		y2;
	
	Text_ClearScreen(APP_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
	
	// draw the title bar at top. 3x80
	Text_CopyMemBoxLinearBuffer((uint8_t*)&app_titlebar, 0, 0, 79, 2, SCREEN_COPY_TO_SCREEN, SCREEN_FOR_TEXT_CHAR);
	Text_FillBoxAttrOnly(0, 0, 79, 0, APP_ACCENT_COLOR, APP_BACKGROUND_COLOR);
// 	Text_FillBoxAttrOnly(0, 1, 79, 1, APP_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
	Text_FillBoxAttrOnly(0, 2, 79, 2, APP_ACCENT_COLOR, APP_BACKGROUND_COLOR);
	Text_InvertBox(47, 1, 53, 1);	// right-hand side vertical bars need to be inversed to grow from thin to fat


	// draw panels
	for (i = 0; i < NUM_PANELS; i++)
	{
		x1 = panel[i].x1_;
		y1 = panel[i].y1_;
		x2 = panel[i].x2_;
		y2 = panel[i].y2_;
		Text_DrawBoxCoordsFancy(x1, y1 - 2, x1 + (UI_PANEL_TAB_WIDTH - 1), y1, PANEL_FOREGROUND_COLOR, PANEL_BACKGROUND_COLOR);
		Text_DrawBoxCoordsFancy(x1, y1, x2, y2, PANEL_FOREGROUND_COLOR, PANEL_BACKGROUND_COLOR);
		Text_SetCharAtXY(x1, y1, SC_T_RIGHT);
		Text_SetCharAtXY(x1 + (UI_PANEL_TAB_WIDTH - 1), y1, SC_T_UP);

		// draw file list head rows
		x1 += UI_PANEL_FILENAME_OFFSET;
		++y1;
		Text_DrawStringAtXY(x1, y1, General_GetString(ID_STR_LBL_FILENAME), LIST_HEADER_COLOR, PANEL_BACKGROUND_COLOR);
		x1 += UI_PANEL_FILETYPE_OFFSET;
		Text_DrawStringAtXY(x1, y1, General_GetString(ID_STR_LBL_FILETYPE), LIST_HEADER_COLOR, PANEL_BACKGROUND_COLOR);
		x1 += UI_PANEL_FILESIZE_OFFSET;
		Text_DrawStringAtXY(x1, y1, General_GetString(ID_STR_LBL_FILESIZE), LIST_HEADER_COLOR, PANEL_BACKGROUND_COLOR);
	}
	
	// draw permanently enabled buttons. 
	// see Screen_DrawFileMenuItems() for the file menu items - they need to be redrawn during main loop
	for (i = FIRST_PERMSTATE_BUTTON; i <= LAST_PERMSTATE_BUTTON; i++)
	{
		x1 = uibutton[i].x1_;
		y1 = uibutton[i].y1_;
		Text_DrawHLine(UI_MIDDLE_AREA_START_X, y1, UI_MIDDLE_AREA_WIDTH, CH_SPACE, MENU_FOREGROUND_COLOR, MENU_BACKGROUND_COLOR, CHAR_AND_ATTR);
		Text_DrawStringAtXY(x1, y1, General_GetString(uibutton[i].string_id_), MENU_FOREGROUND_COLOR, MENU_BACKGROUND_COLOR);
	}
		
	// draw file menu
	Text_DrawHLine(UI_MIDDLE_AREA_START_X, UI_MIDDLE_AREA_FILE_MENU_Y, UI_MIDDLE_AREA_WIDTH, CH_UNDERSCORE, MENU_ACCENT_COLOR, APP_BACKGROUND_COLOR, CHAR_AND_ATTR);
	Text_DrawStringAtXY(UI_MIDDLE_AREA_START_X, UI_MIDDLE_AREA_FILE_MENU_Y + 1, General_GetString(ID_STR_MENU_FILE), MENU_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
	Text_DrawHLine(UI_MIDDLE_AREA_START_X, UI_MIDDLE_AREA_FILE_MENU_Y + 2, UI_MIDDLE_AREA_WIDTH, CH_OVERSCORE, MENU_ACCENT_COLOR, APP_BACKGROUND_COLOR, CHAR_AND_ATTR);
		
	// draw device menu
	Text_DrawHLine(UI_MIDDLE_AREA_START_X, UI_MIDDLE_AREA_DEV_MENU_Y, UI_MIDDLE_AREA_WIDTH, CH_UNDERSCORE, MENU_ACCENT_COLOR, APP_BACKGROUND_COLOR, CHAR_AND_ATTR);
	Text_DrawStringAtXY(UI_MIDDLE_AREA_START_X, UI_MIDDLE_AREA_DEV_MENU_Y + 1, General_GetString(ID_STR_MENU_DEVICE), MENU_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
	Text_DrawHLine(UI_MIDDLE_AREA_START_X, UI_MIDDLE_AREA_DEV_MENU_Y + 2, UI_MIDDLE_AREA_WIDTH, CH_OVERSCORE, MENU_ACCENT_COLOR, APP_BACKGROUND_COLOR, CHAR_AND_ATTR);

	// also draw the comms area
	Buffer_DrawCommunicationArea();
	Buffer_RefreshDisplay();
}

/*****************************************************************************/
/*                        Public Function Definitions                        */
/*****************************************************************************/

// redraw file menu buttons in activated/inactivated state as appropriate
// device buttons are always activated, so are only drawn once
void Screen_DrawFileMenuItems(bool as_active)
{
	uint8_t		i;
	uint8_t		x1;
	uint8_t		y1;
	uint8_t		text_color;

	text_color = (as_active == true ? MENU_FOREGROUND_COLOR : MENU_INACTIVE_COLOR);
	
	// draw buttons
	for (i = FIRST_ACTIVATING_BUTTON; i <= LAST_ACTIVATING_BUTTON; i++)
	{
		//text_color = (uibutton[i].active_ == true ? MENU_FOREGROUND_COLOR : MENU_INACTIVE_COLOR);
		x1 = uibutton[i].x1_;
		y1 = uibutton[i].y1_;
		Text_DrawHLine(UI_MIDDLE_AREA_START_X, y1, UI_MIDDLE_AREA_WIDTH, CH_SPACE, text_color, MENU_BACKGROUND_COLOR, CHAR_AND_ATTR);
		Text_DrawStringAtXY(x1, y1, General_GetString(uibutton[i].string_id_), text_color, MENU_BACKGROUND_COLOR);
	}
}


// swap the copy >>> button for copy <<< and vice versa
void Screen_SwapCopyDirectionIndicator(void)
{
	uint8_t		x1;
	uint8_t		y1;

	if (uibutton[BUTTON_ID_COPY].string_id_ == ID_STR_FILE_COPY_RIGHT)
	{
		uibutton[BUTTON_ID_COPY].string_id_ = ID_STR_FILE_COPY_LEFT;
	}
	else
	{
		uibutton[BUTTON_ID_COPY].string_id_ = ID_STR_FILE_COPY_RIGHT;
	}

	x1 = uibutton[BUTTON_ID_COPY].x1_;
	y1 = uibutton[BUTTON_ID_COPY].y1_;
	Text_DrawStringAtXY(x1, y1, General_GetString(uibutton[BUTTON_ID_COPY].string_id_), MENU_FOREGROUND_COLOR, MENU_BACKGROUND_COLOR);
}


// populate button objects, etc. no drawing.
void Screen_InitializeUI(void)
{
	panel[PANEL_ID_LEFT].id_ = PANEL_ID_LEFT;
	panel[PANEL_ID_LEFT].device_id_ = DEVICE_ID_UNSET;
	panel[PANEL_ID_LEFT].x1_ = UI_LEFT_PANEL_BODY_X1;
	panel[PANEL_ID_LEFT].y1_ = UI_LEFT_PANEL_BODY_Y1;
	panel[PANEL_ID_LEFT].width_ = UI_LEFT_PANEL_BODY_WIDTH;
	panel[PANEL_ID_LEFT].x2_ = UI_LEFT_PANEL_BODY_X2;
	panel[PANEL_ID_LEFT].y2_ = UI_LEFT_PANEL_BODY_Y2;

	panel[PANEL_ID_RIGHT].id_ = PANEL_ID_RIGHT;
	panel[PANEL_ID_RIGHT].device_id_ = DEVICE_ID_UNSET;
	panel[PANEL_ID_RIGHT].x1_ = UI_RIGHT_PANEL_BODY_X1;
	panel[PANEL_ID_RIGHT].y1_ = UI_RIGHT_PANEL_BODY_Y1;
	panel[PANEL_ID_RIGHT].width_ = UI_RIGHT_PANEL_BODY_WIDTH;
	panel[PANEL_ID_RIGHT].x2_ = UI_RIGHT_PANEL_BODY_X2;
	panel[PANEL_ID_RIGHT].y2_ = UI_RIGHT_PANEL_BODY_Y2;

	// set up the buttons - file actions
	uibutton[BUTTON_ID_COPY].id_ = BUTTON_ID_COPY;
	uibutton[BUTTON_ID_COPY].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_COPY].y1_ = UI_MIDDLE_AREA_FILE_CMD_Y;
	uibutton[BUTTON_ID_COPY].string_id_ = ID_STR_FILE_COPY_RIGHT;
	//uibutton[BUTTON_ID_COPY].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_DELETE].id_ = BUTTON_ID_DELETE;
	uibutton[BUTTON_ID_DELETE].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_DELETE].y1_ = UI_MIDDLE_AREA_FILE_CMD_Y + 1;
	uibutton[BUTTON_ID_DELETE].string_id_ = ID_STR_FILE_DELETE;
	//uibutton[BUTTON_ID_DELETE].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_DUPLICATE].id_ = BUTTON_ID_DUPLICATE;
	uibutton[BUTTON_ID_DUPLICATE].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_DUPLICATE].y1_ = UI_MIDDLE_AREA_FILE_CMD_Y + 2;
	uibutton[BUTTON_ID_DUPLICATE].string_id_ = ID_STR_FILE_DUP;
	//uibutton[BUTTON_ID_DUPLICATE].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_RENAME].id_ = BUTTON_ID_RENAME;
	uibutton[BUTTON_ID_RENAME].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_RENAME].y1_ = UI_MIDDLE_AREA_FILE_CMD_Y + 3;
	uibutton[BUTTON_ID_RENAME].string_id_ = ID_STR_FILE_RENAME;
	//uibutton[BUTTON_ID_RENAME].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_TEXT_VIEW].id_ = BUTTON_ID_TEXT_VIEW;
	uibutton[BUTTON_ID_TEXT_VIEW].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_TEXT_VIEW].y1_ = UI_MIDDLE_AREA_FILE_CMD_Y + 4;
	uibutton[BUTTON_ID_TEXT_VIEW].string_id_ = ID_STR_FILE_TEXT_PREVIEW;
	//uibutton[BUTTON_ID_TEXT_VIEW].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_HEX_VIEW].id_ = BUTTON_ID_HEX_VIEW;
	uibutton[BUTTON_ID_HEX_VIEW].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_HEX_VIEW].y1_ = UI_MIDDLE_AREA_FILE_CMD_Y + 5;
	uibutton[BUTTON_ID_HEX_VIEW].string_id_ = ID_STR_FILE_HEX_PREVIEW;
	//uibutton[BUTTON_ID_HEX_VIEW].state_ = UI_BUTTON_STATE_DISABLED;

	// set up the buttons - device actions
	uibutton[BUTTON_ID_NEXT_DEVICE].id_ = BUTTON_ID_NEXT_DEVICE;
	uibutton[BUTTON_ID_NEXT_DEVICE].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_NEXT_DEVICE].y1_ = UI_MIDDLE_AREA_DEV_CMD_Y;
	uibutton[BUTTON_ID_NEXT_DEVICE].string_id_ = ID_STR_DEV_NEXT;
	//uibutton[BUTTON_ID_NEXT_DEVICE].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_REFRESH].id_ = BUTTON_ID_REFRESH;
	uibutton[BUTTON_ID_REFRESH].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_REFRESH].y1_ = UI_MIDDLE_AREA_DEV_CMD_Y + 1;
	uibutton[BUTTON_ID_REFRESH].string_id_ = ID_STR_DEV_REFRESH_LISTING;
	//uibutton[BUTTON_ID_REFRESH].state_ = UI_BUTTON_STATE_DISABLED;

	uibutton[BUTTON_ID_FORMAT].id_ = BUTTON_ID_FORMAT;
	uibutton[BUTTON_ID_FORMAT].x1_ = UI_MIDDLE_AREA_START_X;
	uibutton[BUTTON_ID_FORMAT].y1_ = UI_MIDDLE_AREA_DEV_CMD_Y + 2;
	uibutton[BUTTON_ID_FORMAT].string_id_ = ID_STR_DEV_FORMAT;
	//uibutton[BUTTON_ID_FORMAT].state_ = UI_BUTTON_STATE_DISABLED;
}


// set up screen variables and draw screen for first time
void Screen_Render(void)
{
	Text_ClearScreen(APP_FOREGROUND_COLOR, APP_BACKGROUND_COLOR);
	Screen_DrawUI();
}


// load strings into memory and set up string pointers
void App_LoadStrings(void)
{
	bool		more_to_get = true;
	uint8_t		the_id;
	uint8_t		str_length;
	uint8_t		strings_found = 0;
	uint8_t*	the_buff = (uint8_t*)0xC000;	// mapped to I/O bank Address, but we will map out I/O when this needs to be accessed;

	//DEBUG_OUT(("%s %d: entered", __func__, __LINE__));

	// f256jr:
	//   until load is available, I am going to be side-loading directly into physical memory with the emulator's command line
	//   all strings will be in 1 dedicated ram bank (8k)
	//   for now, rather than read length and id (without putting them in the buffer), I'll be loading the whole file
	//   this function will be about scanning what has already been loaded, and setting up the string pointers, rather than about loading
	
	// OLD NOTE:
	// logic:
	//  - string files are set up as id->len->the string. string len is limited to 255.
	//  - we will read 2 bytes to get id and length
	//  - will then read 'length' worth of bytes into the global_string_buff
	//  - will then write a null on the end of that storage loc.
	//  - will then point the next global_string[] pointer to the starting write addr
	//  - then loop around until all strings read in.

	// map the string buffer into CPU memory space
	zp_bank_num = STRING_STORAGE_VALUE;
	io_bank_value_kernel = Memory_SwapInNewBank(BANK_IO);

	asm("SEI"); // disable interrupts in case some other process has a role here
	Sys_DisableIOBank();

	while (more_to_get && strings_found < NUM_STRINGS)
	{
		the_id = *the_buff;
		//DEBUG_OUT(("%s %d: the_id=%u, the_buff=%p", __func__, __LINE__, the_id, the_buff));
		the_buff[0] = 0;	// overwrite ID spot with a NULL, as separator between strings
		++the_buff;
		
		if (the_id >= NUM_STRINGS)
		{
			more_to_get = false;
			break;
		}

		str_length = *the_buff++;
		//DEBUG_OUT(("%s %d: the_id=%u, len=%u", __func__, __LINE__, the_id, str_length));
		
		// remember where we put this string
		global_string[the_id] = (char*)the_buff;

		// move the read buffer to end of the string just read in
		the_buff += str_length;
		++strings_found;
	}

	// write out a null at end of all strings, to terminate the last one
	++the_buff;
	the_buff[0] = 0;
	
	// prepare for end of map generation mode by re-enabling the I/O page, which unmaps the dungeon map screen from 6502 RAM space
	Sys_DisableIOBank();

	zp_bank_num = io_bank_value_kernel;
	Memory_SwapInNewBank(BANK_IO);

	asm("CLI"); // restore interrupts
}



