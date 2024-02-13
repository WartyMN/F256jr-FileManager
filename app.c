/*
 * app.c
 *
 *  Created on: Jan 10, 2023
 *      Author: micahbly
 *
 *  A pseudo commander-style 2-column file manager
 *
 */
 


/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/


// project includes
#include "app.h"
#include "comm_buffer.h"
#include "file.h"
#include "list_panel.h"
#include "folder.h"
#include "general.h"
#include "text.h"
#include "screen.h"
#include "strings.h"
#include "sys.h"

// C includes
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cc65 includes
#include <device.h>
//#include <unistd.h>
#include <cc65.h>
#include <dirent.h>
#include <f256.h>



/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/




/*****************************************************************************/
/*                          File-scoped Variables                            */
/*****************************************************************************/


static WB2KFolderObject*	app_root_folder[2];
static uint8_t				app_active_panel_id;	// PANEL_ID_LEFT or PANEL_ID_RIGHT
static uint8_t				temp_buff_384b[384];	// will use for both 192 and 384 buffers (shared)
static uint8_t				app_connected_drive_count;

/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/

int8_t					global_connected_device[DEVICE_MAX_DEVICE_COUNT];	// will be 8, 9, etc, if connected, or -1 if not. paired with global_connected_unit.
int8_t					global_connected_unit[DEVICE_MAX_DEVICE_COUNT];		// will be 0 or 1 if connected, or -1 if not. paired with global_connected_device.

WB2KViewPanel			app_file_panel[2];
//WB2KFolderObject		app_root_folder[2];
TextDialogTemplate		global_dlg;	// dialog we'll configure and re-use for different purposes
char					global_dlg_title[36];	// arbitrary
char					global_dlg_body_msg[70];	// arbitrary
char					global_dlg_button[3][10];	// arbitrary

uint8_t*				global_temp_buff_192b_1 = temp_buff_384b;
uint8_t*				global_temp_buff_192b_2 = (temp_buff_384b + 192);
uint8_t*				global_temp_buff_384b = temp_buff_384b;
char*					global_string_buff1 = (char*)temp_buff_384b;
char*					global_string_buff2 = (char*)(temp_buff_384b + 192);

uint8_t					temp_screen_buffer_char[APP_DIALOG_BUFF_SIZE];	// WARNING HBD: don't make dialog box bigger than will fit!
uint8_t					temp_screen_buffer_attr[APP_DIALOG_BUFF_SIZE];	// WARNING HBD: don't make dialog box bigger than will fit!

extern char*			global_string[NUM_STRINGS];


/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/

// swap the active panels. 
void App_SwapActivePanel(void);

// initialize various objects - once per run
void App_Initialize(void);

// handles user input
uint8_t App_MainLoop(void);

// scan for connected devices, and return count. Returns -1 if error.
int8_t	App_ScanDevices(void);

/*****************************************************************************/
/*                       Private Function Definitions                        */
/*****************************************************************************/



// swap the active panels. 
void App_SwapActivePanel(void)
{
	// mark old panel inactive, mark new panel active, set new active panel id
	// TODO
	Panel_ToggleActiveState(&app_file_panel[PANEL_ID_LEFT]);
	Panel_ToggleActiveState(&app_file_panel[PANEL_ID_RIGHT]);
	++app_active_panel_id;
	app_active_panel_id = app_active_panel_id % 2;
	//sprintf(global_string_buff1, "active panel set to %u", app_active_panel_id);
	//Buffer_NewMessage(global_string_buff1);
}


// scan for connected devices, and return count. Returns -1 if error.
int8_t	App_ScanDevices(void)
{
	uint8_t		drive_num = 0;
	uint8_t		device;
	uint8_t		unit = 0;
	DIR*		dir;
	char		drive_path[3];
	char*		the_drive_path = drive_path;
	
	// LOGIC: 
	//   F256 can have up to: 1 SD card (device 0:), 2 IEC drives (device 1: and 2:), and a flash drive (3:)
	//     2023/01/19: per Gadget, will eventually support 8 devices, but only 4 named so far, and flash drive is not ready. 
	//     2023/01/23: per Gadget, do you support unit0/1? > " the underlying driver does, but I don't register the second partition in the file-system device table"
	//   to see if a device is connected, we iterate through the possible device #s and try to open the directory file. 
	//     there is probably a better way to do this, but not sure what it is.
	
	app_connected_drive_count = 0;
	
	for (device = DEVICE_LOWEST_DEVICE_NUM; device <= DEVICE_HIGHEST_DEVICE_NUM; device++)
	{
		sprintf(the_drive_path, "%u:", device);
//	Buffer_NewMessage(the_drive_path);
		dir = opendir(the_drive_path);

		if (dir)
		{
			// this device exists/is connected
			global_connected_device[drive_num] = device;
			global_connected_unit[drive_num] = unit;
			++drive_num;
			
			if (closedir(dir) == -1)
			{
				//Buffer_NewMessage("clsdir fail");
			}
		//sprintf(the_drive_path, "device was ok. device=%u, unit=%u", device, unit);
		//Buffer_NewMessage(the_drive_path);
			//sprintf(global_string_buff1, "%u: open ok", device);
		}
// 		else
// 		{
// 			sprintf(global_string_buff1, "%u: open F", device);
// 		}
// 			Buffer_NewMessage(global_string_buff1);
	}
	
	return drive_num;
}


// initialize various objects - once per run
void App_Initialize(void)
{
	uint8_t				the_drive_index = 0;
	WB2KFileObject*		root_folder_file_left;
	WB2KFileObject*		root_folder_file_right;
	char				drive_path[3];
	char*				the_drive_path = drive_path;

	// initialize the comm buffer
	Buffer_Initialize();
	Buffer_Clear();

	// show app name, version, and credit
	sprintf(global_string_buff1, General_GetString(ID_STR_APP_NAME_PLATFORM_VERSION), MAJOR_VERSION, MINOR_VERSION, UPDATE_VERSION);
	Buffer_NewMessage(global_string_buff1);
	Buffer_NewMessage(General_GetString(ID_STR_ABOUT_COPYRIGHT));

	// set up the dialog template we'll use throughout the app
	global_dlg.x_ = (SCREEN_NUM_COLS - APP_DIALOG_WIDTH)/2;
	global_dlg.y_ = 16;
	global_dlg.width_ = APP_DIALOG_WIDTH;
	global_dlg.height_ = APP_DIALOG_HEIGHT;
	global_dlg.num_buttons_ = 2;
	global_dlg.btn_shortcut_[0] = CH_ESC; // OSF_CH_ESC;
	global_dlg.btn_shortcut_[1] = CH_ENTER;
	global_dlg.btn_is_affirmative_[0] = false;
	global_dlg.btn_is_affirmative_[1] = true;

	global_dlg.title_text_ = global_dlg_title;
	global_dlg.body_text_ = global_dlg_body_msg;
	global_dlg.btn_label_[0] = global_dlg_button[0];
	global_dlg.btn_label_[1] = global_dlg_button[1];

	// scan which devices are connected, so we know what panels can access
	Buffer_NewMessage(General_GetString(ID_STR_MSG_SCANNING));
	app_connected_drive_count = App_ScanDevices();

	sprintf(global_string_buff1, General_GetString(ID_STR_MSG_SHOW_DRIVE_COUNT), app_connected_drive_count);
	Buffer_NewMessage(global_string_buff1);
	
	// if no drives connected, there's nothing for us to do. just quit. 
	if (app_connected_drive_count < 1)
	{
		Buffer_NewMessage(General_GetString(ID_STR_MSG_NO_DRIVES_AVAILABLE));
		App_Exit(ERROR_NO_CONNECTED_DRIVES_FOUND);
	}


	// set up the left panel pointing to the lowest available device
	
	sprintf(the_drive_path, "%u:", global_connected_device[the_drive_index]);

	if ( (root_folder_file_left = File_New("", the_drive_path, true, 0, 1, global_connected_device[the_drive_index], 0, 0) ) == NULL)
	{
		App_Exit(ERROR_COULD_NOT_CREATE_ROOT_FOLDER_FILE_LEFT);
	}

	app_active_panel_id = PANEL_ID_LEFT;
	
	if ( (app_root_folder[PANEL_ID_LEFT] = Folder_New(root_folder_file_left, true, global_connected_device[the_drive_index], global_connected_unit[the_drive_index]) ) == NULL)
	{
		App_Exit(ERROR_COULD_NOT_CREATE_ROOT_FOLDER_OBJ_LEFT);
	}

	app_file_panel[PANEL_ID_LEFT].drive_index_ = the_drive_index;


	// set up the right panel pointing to the next lowest available device (will be same if only 1 device)
	
	++the_drive_index;

	// set the 2nd panel to the next device, unless there is only 1 device. in that case, have same device on both sides. 
	if (the_drive_index >= app_connected_drive_count)
	{
		--the_drive_index;
	}
	else
	{
		sprintf(the_drive_path, "%u:", global_connected_device[the_drive_index]);
	}

	if ( (root_folder_file_right = File_New("", the_drive_path, true, 0, 1, global_connected_device[the_drive_index], 0, 0) ) == NULL)
	{
		App_Exit(ERROR_COULD_NOT_CREATE_ROOT_FOLDER_FILE_RIGHT);
	}
	
	if ( (app_root_folder[PANEL_ID_RIGHT] = Folder_New(root_folder_file_right, true, global_connected_device[the_drive_index], global_connected_unit[the_drive_index]) ) == NULL)
	{
		App_Exit(ERROR_COULD_NOT_CREATE_ROOT_FOLDER_OBJ_RIGHT);
	}

	app_file_panel[PANEL_ID_RIGHT].drive_index_ = the_drive_index;


	Panel_Initialize(
		&app_file_panel[PANEL_ID_LEFT], 
		app_root_folder[PANEL_ID_LEFT], 
		(UI_LEFT_PANEL_BODY_X1 + 1), (UI_LEFT_PANEL_BODY_Y1 + 2), 
		(UI_LEFT_PANEL_BODY_WIDTH - 2), (UI_LEFT_PANEL_BODY_HEIGHT - 3)
	);
	Panel_Initialize(
		&app_file_panel[PANEL_ID_RIGHT], 
		app_root_folder[PANEL_ID_RIGHT], 
		(UI_RIGHT_PANEL_BODY_X1 + 1), (UI_RIGHT_PANEL_BODY_Y1 + 2), 
		(UI_RIGHT_PANEL_BODY_WIDTH - 2), (UI_RIGHT_PANEL_BODY_HEIGHT - 3)
	);

	Panel_SetCurrentUnit(&app_file_panel[PANEL_ID_LEFT], app_root_folder[PANEL_ID_LEFT]->device_number_, app_root_folder[PANEL_ID_LEFT]->unit_number_);
	Panel_SetCurrentUnit(&app_file_panel[PANEL_ID_RIGHT], app_root_folder[PANEL_ID_RIGHT]->device_number_, app_root_folder[PANEL_ID_RIGHT]->unit_number_);
	app_file_panel[PANEL_ID_LEFT].active_ = true;	// we always start out with left panel being the active one
	app_file_panel[PANEL_ID_RIGHT].active_ = false;	// we always start out with left panel being the active one

	//Buffer_NewMessage("Initialization complete.");
}


// handles user input
uint8_t App_MainLoop(void)
{
	uint8_t				user_input;
	bool				exit_main_loop = false;
	bool				success;
	bool				file_menu_active;
	WB2KViewPanel*		the_panel;
	
	// main loop
	while (! exit_main_loop)
	{
		// turn off cursor - seems to turn itself off when kernal detects cursor position has changed. 
		Sys_EnableTextModeCursor(false);
		
		the_panel = &app_file_panel[app_active_panel_id];
		
		//DEBUG: track software stack pointer
		//sprintf(global_string_buffer, "sp: %x%x", *(char*)0x52, *(char*)0x51);
		//Buffer_NewMessage(global_string_buffer);

		do
		{
			// LOGIC:
			//   enable/disable menu buttons -- 
			//   our rule is simple: if active panel has 1 or more files, 1 must be selected, so all file menu items are active
			//   have separate switches for when file menu is active/inactive
			
			// redraw any menu buttons that could change (file menu only at this point)
			file_menu_active = (the_panel->num_rows_ > 0);
			Screen_DrawFileMenuItems(file_menu_active);

			user_input = getchar();
	
			// first switch: for file menu only, and skip if file menu is inactive
			//   slightly inefficient in that it has to go through them all twice, but this is not a performance bottleneck
			//   note: we also put the sort commands here because it doesn't make sense to sort if no files
			if (file_menu_active)
			{
				switch (user_input)
				{
					case ACTION_SORT_BY_NAME:
						//DEBUG_OUT(("%s %d: Sort by name", __func__ , __LINE__));
						the_panel->sort_compare_function_ = (void*)&File_CompareName;
						Panel_SortFiles(the_panel);
						Buffer_NewMessage(General_GetString(ID_STR_MSG_SORTED_BY_NAME));
						break;

	// 				case ACTION_SORT_BY_DATE:
	// 					//DEBUG_OUT(("%s %d: Sort by date", __func__ , __LINE__));
	// 					the_panel->sort_compare_function_ = (void*)&File_CompareDate;
	// 					Panel_SortFiles(the_panel);
	// 					Buffer_NewMessage("Files now sorted by date.");
						break;
	// 
					case ACTION_SORT_BY_SIZE:
						//DEBUG_OUT(("%s %d: Sort by size", __func__ , __LINE__));
						the_panel->sort_compare_function_ = (void*)&File_CompareSize;
						Panel_SortFiles(the_panel);
						Buffer_NewMessage(General_GetString(ID_STR_MSG_SORTED_BY_SIZE));
						break;
			
					case ACTION_SORT_BY_TYPE:
						//DEBUG_OUT(("%s %d: Sort by type", __func__ , __LINE__));
						the_panel->sort_compare_function_ = (void*)&File_CompareFileTypeID;
						Panel_SortFiles(the_panel);
						Buffer_NewMessage(General_GetString(ID_STR_MSG_SORTED_BY_TYPE));
						break;
			
					case ACTION_VIEW_AS_HEX:
						//DEBUG_OUT(("%s %d: view as hex", __func__ , __LINE__));
						success = Panel_ViewCurrentFileAsHex(the_panel);	
						Screen_Render();	// the hex view has completely overwritten the screen
						Panel_RenderContents(&app_file_panel[PANEL_ID_LEFT]);
						Panel_RenderContents(&app_file_panel[PANEL_ID_RIGHT]);
						//sprintf(global_string_buff1, "view as hex success = %u", success);
						//Buffer_NewMessage(global_string_buff1);
						break;
				
					case ACTION_VIEW_AS_TEXT:
						//DEBUG_OUT(("%s %d: view as hex", __func__ , __LINE__));
						success = Panel_ViewCurrentFileAsText(the_panel);	
						Screen_Render();	// the hex view has completely overwritten the screen
						Panel_RenderContents(&app_file_panel[PANEL_ID_LEFT]);
						Panel_RenderContents(&app_file_panel[PANEL_ID_RIGHT]);
						//sprintf(global_string_buff1, "view as text success = %u", success);
						//Buffer_NewMessage(global_string_buff1);
						break;
				
					case ACTION_COPY:
						success = Panel_CopyCurrentFile(the_panel, &app_file_panel[(app_active_panel_id + 1) % 2]);
						//sprintf(global_string_buff1, "copy file success = %u", success);
						//Buffer_NewMessage(global_string_buff1);
						break;
				
					case ACTION_DUPLICATE:
						success = Panel_CopyCurrentFile(the_panel, the_panel);	// to duplicate, we just pass same panel as target.
						//sprintf(global_string_buff1, "copy file success = %u", success);
						//Buffer_NewMessage(global_string_buff1);
						break;
					
					case ACTION_RENAME:
						success = Panel_RenameCurrentFile(the_panel);
						break;
				
					case ACTION_DELETE:
					case ACTION_DELETE_ALT:
						if ( (success = Panel_DeleteCurrentFile(the_panel)) == false )
						{
							Buffer_NewMessage(General_GetString(ID_STR_MSG_DELETE_FAILURE));
						}
						break;

					default:
						// no need to do any default action: we WANT it to fall through to next switch if none of above happened.
						break;
				}
			}
			
			switch (user_input)
			{
				case ACTION_SWAP_ACTIVE_PANEL:
					// mark old panel inactive, mark new panel active, set new active panel id
					App_SwapActivePanel();
					Screen_SwapCopyDirectionIndicator();
					the_panel = &app_file_panel[app_active_panel_id];
					break;

				case ACTION_NEXT_DEVICE:
					// tell panel to forget all its files, and repopulate itself from the next drive in the system. 
					success = Panel_SwitchToNextDrive(the_panel, app_connected_drive_count - 1);
					break;
					
				case ACTION_REFRESH_PANEL:
					Folder_RefreshListing(the_panel->root_folder_);
					Panel_Init(the_panel);			
					break;
				
				case ACTION_FORMAT_DISK:
					success = Panel_FormatDrive(the_panel);
					if (success)
					{
						Folder_RefreshListing(the_panel->root_folder_);
						Panel_Init(the_panel);			
					}
					break;
					
				case MOVE_UP:
				case MOVE_UP_ALT:
					success = Panel_SelectPrevFile(the_panel);
					//sprintf(global_string_buff1, "prev file selection success = %u", success);
					//Buffer_NewMessage(global_string_buff1);
					break;

				case MOVE_DOWN:
				case MOVE_DOWN_ALT:
					success = Panel_SelectNextFile(the_panel);
					//sprintf(global_string_buff1, "next file selection success = %u", success);
					//Buffer_NewMessage(global_string_buff1);
					break;

				case MOVE_LEFT:
				case MOVE_LEFT_ALT:
					if (app_active_panel_id == PANEL_ID_RIGHT)
					{
						// mark old panel inactive, mark new panel active, set new active panel id
						App_SwapActivePanel();
						the_panel = &app_file_panel[app_active_panel_id];
					}
					break;

				case MOVE_RIGHT:
				case MOVE_RIGHT_ALT:
					if (app_active_panel_id == PANEL_ID_LEFT)
					{
						// mark old panel inactive, mark new panel active, set new active panel id
						App_SwapActivePanel();
						the_panel = &app_file_panel[app_active_panel_id];
					}
					break;
					
				case ACTION_QUIT:
					exit_main_loop = true;
					continue;
					break;

				default:
					//sprintf(global_string_buff1, "didn't know key %u", user_input);
					//Buffer_NewMessage(global_string_buff1);
					//DEBUG_OUT(("%s %d: didn't know key %u", __func__, __LINE__, user_input));
					//fatal(user_input);
					user_input = ACTION_INVALID_INPUT;
					break;
			}
		} while (user_input == ACTION_INVALID_INPUT);				
	} // while for exit loop
	
	// normal returns all handled above. this is a catch-all
	return 0;
}




/*****************************************************************************/
/*                        Public Function Definitions                        */
/*****************************************************************************/


// if ending on error: display error message, wait for user to confirm, and exit
// if no error, just exit
void App_Exit(uint8_t the_error_number)
{
	if (the_error_number != ERROR_NO_ERROR)
	{
		sprintf(global_string_buff1, General_GetString(ID_STR_MSG_FATAL_ERROR), the_error_number);
		General_Strlcpy((char*)&global_dlg_title, global_string_buff1, MAX_STRING_COMP_LEN);
		General_Strlcpy((char*)&global_dlg_body_msg, General_GetString(ID_STR_MSG_FATAL_ERROR_BODY), APP_DIALOG_WIDTH);
		General_Strlcpy((char*)&global_dlg_button[0], General_GetString(ID_STR_DLG_OK), 10);
		
		global_dlg.num_buttons_ = 2;
		
		Text_DisplayDialog(&global_dlg, (char*)&temp_screen_buffer_char, (char*)&temp_screen_buffer_attr);
	}
	
	// turn cursor back on
	Sys_EnableTextModeCursor(true);
	
	exit(0);	
}




int main(void)
{

	// open log file, if debugging flags were passed
	#ifdef LOG_LEVEL_5
		General_LogInitialize();
	#endif
	
	if (Sys_InitSystem() == false)
	{
		//asm("JMP ($FFFC)");
		exit(0);
	}
	Sys_SetBorderSize(0, 0); // want all 80 cols and 60 rows!
	
	Screen_InitializeUI();
	Screen_Render();

	App_Initialize();
	
	Buffer_NewMessage(General_GetString(ID_STR_MSG_READING_DIR));
	Panel_Init(&app_file_panel[PANEL_ID_LEFT]);

	if (app_connected_drive_count > 1)
	{
		Buffer_NewMessage(General_GetString(ID_STR_MSG_READING_DIR));
		Panel_Init(&app_file_panel[PANEL_ID_RIGHT]);
	}
	
	App_MainLoop();
	
	// restore screen, etc.
	App_Exit(ERROR_NO_ERROR);
	
	return 0;
}