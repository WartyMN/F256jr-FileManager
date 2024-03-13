/*
 * screen.h
 *
 *  Created on: Jan 11, 2023
 *      Author: micahbly
 */

#ifndef SCREEN_H_
#define SCREEN_H_

/* about this class
 *
 *** things a screen needs to be able to do
 *
 * draw the file manager screen
 * draw individual buttons
 * update visual state of individual buttons
 *
 *** things a screen has
 *
 *
 */

/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

//#include <string.h>

#include "app.h"
#include "text.h"
#include <stdint.h>


/*****************************************************************************/
/*                            Macro Definitions                              */
/*****************************************************************************/

// there are 12 buttons which can be accessed with the same code
#define NUM_BUTTONS					19

#define BUTTON_ID_COPY				0
#define BUTTON_ID_DELETE			1
#define BUTTON_ID_DUPLICATE			2
#define BUTTON_ID_RENAME			3
#define BUTTON_ID_TEXT_VIEW			4
#define BUTTON_ID_HEX_VIEW			5
#define BUTTON_ID_LOAD				6

#define BUTTON_ID_NEXT_DEVICE		7
#define BUTTON_ID_REFRESH			8
#define BUTTON_ID_FORMAT			9
#define BUTTON_ID_MAKE_DIR			10
#define BUTTON_ID_SORT_BY_TYPE		11
#define BUTTON_ID_SORT_BY_NAME		12
#define BUTTON_ID_SORT_BY_SIZE		13

// app menu buttons
#define BUTTON_ID_SET_CLOCK			14
#define BUTTON_ID_ABOUT				15
#define BUTTON_ID_EXIT_TO_BASIC		16
#define BUTTON_ID_EXIT_TO_DOS		17
#define BUTTON_ID_QUIT				18

#define FIRST_ACTIVATING_BUTTON		BUTTON_ID_COPY
#define LAST_ACTIVATING_BUTTON		BUTTON_ID_LOAD
#define FIRST_PERMSTATE_BUTTON		BUTTON_ID_NEXT_DEVICE
#define LAST_PERMSTATE_BUTTON		BUTTON_ID_QUIT

#define DEVICE_ID_UNSET				-1
#define DEVICE_ID_ERROR				-2

#define UI_BUTTON_STATE_DISABLED	0
#define UI_BUTTON_STATE_NORMAL		1
#define UI_BUTTON_STATE_SELECTED	2

#define UI_MIDDLE_AREA_START_X			35
#define UI_MIDDLE_AREA_START_Y			7
#define UI_MIDDLE_AREA_WIDTH			10

#define UI_MIDDLE_AREA_DEV_MENU_Y		(UI_MIDDLE_AREA_START_Y + 0)
#define UI_MIDDLE_AREA_DEV_CMD_Y		(UI_MIDDLE_AREA_DEV_MENU_Y + 3)

#define UI_MIDDLE_AREA_DIR_MENU_Y		(UI_MIDDLE_AREA_DEV_CMD_Y + 4)
#define UI_MIDDLE_AREA_DIR_CMD_Y		(UI_MIDDLE_AREA_DIR_MENU_Y + 3)

#define UI_MIDDLE_AREA_FILE_MENU_Y		(UI_MIDDLE_AREA_DIR_CMD_Y + 5)
#define UI_MIDDLE_AREA_FILE_CMD_Y		(UI_MIDDLE_AREA_FILE_MENU_Y + 3)

#define UI_MIDDLE_AREA_APP_MENU_Y		(UI_MIDDLE_AREA_FILE_CMD_Y + 8)
#define UI_MIDDLE_AREA_APP_CMD_Y		(UI_MIDDLE_AREA_APP_MENU_Y + 3)

#define UI_PANEL_INNER_WIDTH			33
#define UI_PANEL_OUTER_WIDTH			(UI_PANEL_INNER_WIDTH + 2)
#define UI_PANEL_INNER_HEIGHT			42
#define UI_PANEL_OUTER_HEIGHT			(UI_PANEL_INNER_HEIGHT + 2)
#define UI_PANEL_TAB_WIDTH				28
#define UI_PANEL_TAB_HEIGHT				3

#define UI_PANEL_FILENAME_OFFSET		1	// from left edge of panel to start of filename
#define UI_PANEL_FILETYPE_OFFSET		23	// from start of filename to start of filesize
#define UI_PANEL_FILESIZE_OFFSET		5	// from start of filesize to start of filetype
#define UI_PANEL_FILENAME_SORT_OFFSET	(UI_PANEL_FILENAME_OFFSET + 3)	// from start of col header to pos right of it for sort icon
#define UI_PANEL_FILETYPE_SORT_OFFSET	(UI_PANEL_FILENAME_SORT_OFFSET + UI_PANEL_FILETYPE_OFFSET)	// from start of col header to pos right of it for sort icon
#define UI_PANEL_FILESIZE_SORT_OFFSET	(UI_PANEL_FILETYPE_SORT_OFFSET + UI_PANEL_FILESIZE_OFFSET)	// from start of col header to pos right of it for sort icon

#define UI_LEFT_PANEL_TITLE_TAB_X1		0
#define UI_LEFT_PANEL_TITLE_TAB_Y1		3
#define UI_LEFT_PANEL_TITLE_TAB_WIDTH	UI_PANEL_TAB_WIDTH
#define UI_LEFT_PANEL_TITLE_TAB_HEIGHT	UI_PANEL_TAB_HEIGHT
#define UI_LEFT_PANEL_TITLE_TAB_X2		(UI_LEFT_PANEL_TITLE_TAB_X1 + UI_LEFT_PANEL_TITLE_TAB_WIDTH - 1)
#define UI_LEFT_PANEL_TITLE_TAB_Y2		(UI_LEFT_PANEL_TITLE_TAB_Y1 + UI_LEFT_PANEL_TITLE_TAB_HEIGHT - 1)
#define UI_LEFT_PANEL_HEADER_Y			(UI_LEFT_PANEL_TITLE_TAB_Y2 + 2)
#define UI_LEFT_PANEL_BODY_X1			0
#define UI_LEFT_PANEL_BODY_Y1			6
#define UI_LEFT_PANEL_BODY_WIDTH		UI_PANEL_OUTER_WIDTH
#define UI_LEFT_PANEL_BODY_HEIGHT		UI_PANEL_OUTER_HEIGHT
#define UI_LEFT_PANEL_BODY_X2			(UI_LEFT_PANEL_BODY_X1 + UI_LEFT_PANEL_BODY_WIDTH - 1)
#define UI_LEFT_PANEL_BODY_Y2			(UI_LEFT_PANEL_BODY_Y1 + UI_LEFT_PANEL_BODY_HEIGHT - 1)

#define UI_RIGHT_PANEL_X_DELTA			45

#define UI_RIGHT_PANEL_TITLE_TAB_X1		(UI_LEFT_PANEL_TITLE_TAB_X1 + UI_RIGHT_PANEL_X_DELTA)
#define UI_RIGHT_PANEL_TITLE_TAB_Y1		(UI_LEFT_PANEL_TITLE_TAB_Y1)
#define UI_RIGHT_PANEL_TITLE_TAB_WIDTH	UI_PANEL_TAB_WIDTH
#define UI_RIGHT_PANEL_TITLE_TAB_HEIGHT	UI_PANEL_TAB_HEIGHT
#define UI_RIGHT_PANEL_TITLE_TAB_X2		(UI_RIGHT_PANEL_TITLE_TAB_X1 + UI_RIGHT_PANEL_TITLE_TAB_WIDTH - 1)
#define UI_RIGHT_PANEL_TITLE_TAB_Y2		(UI_RIGHT_PANEL_TITLE_TAB_Y1 + UI_RIGHT_PANEL_TITLE_TAB_HEIGHT - 1)
#define UI_RIGHT_PANEL_HEADER_Y			UI_LEFT_PANEL_HEADER_Y
#define UI_RIGHT_PANEL_BODY_X1			(UI_LEFT_PANEL_BODY_X1 + UI_RIGHT_PANEL_X_DELTA)
#define UI_RIGHT_PANEL_BODY_Y1			(UI_LEFT_PANEL_BODY_Y1)
#define UI_RIGHT_PANEL_BODY_WIDTH		UI_PANEL_OUTER_WIDTH
#define UI_RIGHT_PANEL_BODY_HEIGHT		UI_PANEL_OUTER_HEIGHT
#define UI_RIGHT_PANEL_BODY_X2			(UI_RIGHT_PANEL_BODY_X1 + UI_RIGHT_PANEL_BODY_WIDTH - 1)
#define UI_RIGHT_PANEL_BODY_Y2			(UI_RIGHT_PANEL_BODY_Y1 + UI_RIGHT_PANEL_BODY_HEIGHT - 1)

#define UI_FULL_PATH_LINE_Y				(UI_LEFT_PANEL_BODY_Y2 + 1)	// row, not in any boxes, under file panels, above comms panel, for showing full path of a file.

#define UI_COPY_PROGRESS_Y				(UI_MIDDLE_AREA_FILE_CMD_Y)
#define UI_COPY_PROGRESS_LEFTMOST		(UI_MIDDLE_AREA_START_X + 3)
#define UI_COPY_PROGRESS_RIGHTMOST		(UI_COPY_PROGRESS_LEFTMOST + 5)

#define CH_PROGRESS_BAR_SOLID_CH1		134		// for drawing progress bars that use solid bars, this is the first char (least filled in)
#define CH_PROGRESS_BAR_CHECKER_CH1		207		// for drawing progress bars that use checkerboard bars, this is the first char (least filled in)
#define CH_INVERSE_SPACE				7		// useful for progress bars as a slot fully used up
#define CH_CHECKERBOARD					199		// useful for progress bars as a slot fully used up
#define CH_UNDERSCORE					148		// this is one line up from a pure underscore, but works if text right under it. 0x5f	// '_'
#define CH_OVERSCORE					0x0e	// opposite of '_'
#define CH_SORT_ICON					248		// downward disclosure triangle in f256 fonts


/*****************************************************************************/
/*                               Enumerations                                */
/*****************************************************************************/

/*****************************************************************************/
/*                                 Structs                                   */
/*****************************************************************************/

typedef struct File_Panel
{
	uint8_t		id_;
	int8_t		device_id_;	// 8, 9, etc. -1 if not set/error
	uint8_t		x1_;
	uint8_t		y1_;
	uint8_t		x2_;
	uint8_t		y2_;
	uint8_t		width_;
	uint8_t		is_active_;	// is this the active panel?
} File_Panel;

typedef struct UI_Button
{
	uint8_t		id_;
	uint8_t		x1_;
	uint8_t		y1_;
	uint8_t		string_id_;
	bool		active_;	// 0-disabled/inactive, 1-enabled/active
} UI_Button;

/*****************************************************************************/
/*                       Public Function Prototypes                          */
/*****************************************************************************/

// swap the copy >>> button for copy <<< and vice versa
void Screen_SwapCopyDirectionIndicator(void);

// populate button objects, etc. no drawing.
void Screen_InitializeUI(void);

// set up screen variables and draw screen for first time
void Screen_Render(void);

// redraw file menu buttons in activated/inactivated state as appropriate
// device buttons are always activated, so are only drawn once
void Screen_DrawFileMenuItems(bool as_active);

// have screen function draw the sort triangle in the right place
void Screen_UpdateSortIcons(uint8_t the_panel_x, void* the_sort_compare_function);

// display information about f/manager, the machine, and the MicroKernel
void Screen_ShowAboutInfo(void);

// read the real time clock and display it
void Screen_DisplayTime(void);


#endif /* SCREEN_H_ */
