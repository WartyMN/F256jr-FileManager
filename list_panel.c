/*
 * list_panel.c
 *
 *  Created on: Sep 4, 2020
 *      Author: micahbly
 *
 *  This is a huge cut-down of the Amiga WorkBench2000 code, for F256 f/manager and B128 f/manager
 *    8-bit version started Jan 12, 2023
 */





/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes
#include "comm_buffer.h"
#include "file.h"
#include "app.h"
#include "folder.h"
#include "general.h"
#include "text.h"
#include "list.h"
#include "list_panel.h"
#include "screen.h"
#include "strings.h"
//#include "mouse.h"

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <device.h>
#include <cc65.h>
#include <dirent.h>
//#include <unistd.h>

// F256 includes
#include <f256.h>



/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/



/*****************************************************************************/
/*                          File-scoped Variables                            */
/*****************************************************************************/

extern TextDialogTemplate	global_dlg;	// dialog we'll configure and re-use for different purposes
extern char					global_dlg_title[36];	// arbitrary
extern char					global_dlg_body_msg[70];	// arbitrary
extern char					global_dlg_button[3][10];	// arbitrary

/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/

extern char*		global_string_buff1;
extern char*		global_string_buff2;
extern uint8_t*		global_temp_buff_384b_2;
extern uint8_t		temp_screen_buffer_char[SCREEN_TOTAL_BYTES/5];	// WARNING HBD: dialog can't be set bigger than 50x24 (eg)!
extern uint8_t		temp_screen_buffer_attr[SCREEN_TOTAL_BYTES/5];	// WARNING HBD: dialog can't be set bigger than 50x24 (eg)!
extern int8_t		global_connected_device[DEVICE_MAX_DEVICE_COUNT];	// will be 8, 9, etc, if connected, or -1 if not. paired with global_connected_unit.
extern int8_t		global_connected_unit[DEVICE_MAX_DEVICE_COUNT];		// will be 0 or 1 if connected, or -1 if not. paired with global_connected_device.



/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/



/*****************************************************************************/
/*                       Private Function Definitions                        */
/*****************************************************************************/





// on change to size of parent surface, redefine dimensions of the panel as appropriate for list mode
// define the width of each list column, and whether or not there is enough space to render it.
void Panel_UpdateSizeListMode(WB2KViewPanel* the_panel, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
	// LOGIC:
	//   As window gets too narrow, we shrink filename column, then start dropping columns entirely
	//   Priority for displaying columns is: ALWAYS: icon and name; type > date > size

	bool		scroller_reset_needed = false;

	// first check if new height/width/x/y are different. if not, stop here. 
	if (the_panel->x_ == x && the_panel->y_ == y && the_panel->width_ == width && the_panel->height_ == height)
	{
		return;
	}
	
	// check if height changed: if so, reset content top to 0, because we probably have to reflow
	if (the_panel->height_ != height)
	{
		the_panel->content_top_ = 0;
		scroller_reset_needed = true;
	}
	
	// accept new size/pos data
	the_panel->width_ = width;
	the_panel->height_ = height;
	the_panel->x_ = x;
	the_panel->y_ = y;

// 	// set our required inner width (and publish to parent surface as these are identical in case of list mode)
// 	the_panel->required_inner_width_ = required_h_space;
// 	
// 	if (the_panel->my_parent_surface_->required_inner_width_ != the_panel->required_inner_width_)
// 	{
// 		// inform window it should recalculate the scrollbar sliders
// 		the_panel->my_parent_surface_->required_inner_width_ = the_panel->required_inner_width_;
// 		scroller_reset_needed = true;
// 	}
// 
// 	// reset scrollers if either required inner height/width changed, or if available height/width changed
// 	//   (whatever is appropriate to this view mode)
// 	if (scroller_reset_needed)
// 	{
// 		Window_ResetScrollbars(the_panel->my_parent_surface_);
// 	}
	
	return;
}

// display the title of the panel only
// inverses the title if the panel is active, draws it normally if inactive
void Panel_RenderTitleOnly(WB2KViewPanel* the_panel);




/*****************************************************************************/
/*                        Public Function Definitions                        */
/*****************************************************************************/



// **** CONSTRUCTOR AND DESTRUCTOR *****


// (re)initializer: does not allocate. takes a valid panel and resets it to starting values (+ those passed)
void Panel_Initialize(WB2KViewPanel* the_panel, WB2KFolderObject* root_folder, uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
	if (root_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_INIT_FOLDER_WAS_NULL);	// crash early, crash often
	}
	
	// set some other props
	the_panel->root_folder_ = root_folder;
	the_panel->x_ = x;
	the_panel->y_ = y;
	the_panel->width_ = width;
	the_panel->height_ = height;
	the_panel->content_top_ = 0;
	the_panel->required_inner_height_ = 0;
	the_panel->sort_compare_function_ = (void*)&File_CompareName;

	//DEBUG_OUT(("%s %d: filename='%s'", __func__ , __LINE__, the_panel->root_folder_->folder_file_->file_name_));

	return;
}


// Forget all its files, and repopulate from the next drive in the system. 
// max_drive_num is the highest available connected drive/unit combo in the system. an index to global_connected_device/global_connected_unit arrays.
bool Panel_SwitchToNextDrive(WB2KViewPanel* the_panel, uint8_t max_drive_num)
{
	int8_t		the_drive_index;
	uint8_t		the_new_device;
	uint8_t		the_new_unit;
	
	if (Folder_Reset(the_panel->root_folder_) == false)
	{
		LOG_ERR(("%s %d: could not free the panel's root folder", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}
	
	the_drive_index = the_panel->drive_index_ + 1;

	//sprintf(global_string_buff1, "drive index was %u; will be %u, max drive num=%u", the_panel->drive_index_, the_drive_index, max_drive_num);
	//Buffer_NewMessage(global_string_buff1);

	if (the_drive_index > max_drive_num)
	{
		the_drive_index = 0;
	}
	
	the_new_device = global_connected_device[the_drive_index];
	the_new_unit = global_connected_unit[the_drive_index];
	
// 	sprintf(global_string_buff1, "Switching to device %u, unit %u", the_new_device, the_new_unit);
// 	Buffer_NewMessage(global_string_buff1);
	
	the_panel->drive_index_ = the_drive_index;
	the_panel->device_number_ = the_new_device;
	the_panel->unit_number_ = the_new_unit;
	the_panel->root_folder_->device_number_ = the_new_device;
	the_panel->root_folder_->unit_number_ = the_new_unit;
	
	Panel_Init(the_panel);
	
	return true;
}




// **** SETTERS *****


// sets the current device number (CBM drive number, eg, 8) + unit number (eg, 0 or 1) the panel is using
// does not refresh. Repopulate to do that.
void Panel_SetCurrentUnit(WB2KViewPanel* the_panel, uint8_t the_device_num, uint8_t the_unit_num)
{
	the_panel->device_number_ = the_device_num;
	the_panel->unit_number_ = the_unit_num;
}


// tells the panel to toggle its active/inactive state, and redraw its title appropriately
void Panel_ToggleActiveState(WB2KViewPanel* the_panel)
{
	if (the_panel->active_ == true)
	{
		the_panel->active_ = false;
	}
	else
	{
		the_panel->active_ = true;
	}
	
	Panel_RenderTitleOnly(the_panel);
	Panel_RenderContents(the_panel);
}


// **** GETTERS *****


// returns true if the folder in the panel has any currently selected files/folders
bool Panel_HasSelections(WB2KViewPanel* the_panel)
{
	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
	}
	
	return Folder_HasSelections(the_panel->root_folder_);
}


// returns number of currently selected files in this panel
uint16_t Panel_GetCountSelectedFiles(WB2KViewPanel* the_panel)
{
	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
	}
	
	return Folder_GetCountSelectedFiles(the_panel->root_folder_);
}


// return the root folder
WB2KFolderObject* Panel_GetRootFolder(WB2KViewPanel* the_panel)
{
	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
	}
	
	return the_panel->root_folder_;
}



// **** OTHER FUNCTIONS *****


// initialize a new panel and get directory listing or info view data
bool Panel_Init(WB2KViewPanel* the_panel)
{
	uint8_t		the_error_code;
	
	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
	}
	
	if (!the_panel)
	{
		return false;
	}
	
	// tell cc65 we want to work with the drive associated with the panel
	//_curunit = the_panel->cur_unit_;
// 	if (the_panel->cur_unit_ == 9)
// 	{
// 		chdir("9");
// 	}
// 	else
// 	{
// 		chdir("8");
// 	}
	
	// have root folder populate its list of files
	if ( (the_error_code = Folder_PopulateFiles(the_panel->root_folder_)) > ERROR_NO_ERROR)
	{		
		LOG_INFO(("%s %d: Root folder reported that file population failed with error %u", __func__ , __LINE__, the_error_code));
		Panel_RenderContents(the_panel);	// force it to clear out list
		return false;
	}

	// sort files
	Panel_SortFiles(the_panel);
		
	return true;
}


// // check to see if an already-selected file is under the mouse pointer
// bool Panel_CheckForAlreadySelectedIconUnderMouse(WB2KViewPanel* the_panel, MouseTracker* the_mouse)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   check the coords passed for intersection with the icon's x, y, height, width. same for label
// 
// 	WB2KList*	the_item;
// 	
// 	if (the_panel == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME); // crash early, crash often
// 	}
// 
// 	// define the selection area based on pointer + pointer radius (we won't be in lasso mode if this function is called)
// 	Mouse_UpdateSelectionRectangle(the_mouse, the_panel->my_parent_surface_->content_left_, the_panel->content_top_);
// 
// 	the_item = *(the_panel->root_folder_->list_);
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject* this_file = (WB2KFileObject*)(the_item->payload_);
// 		WB2KLabel* this_label = this_file->label_[PANEL_LIST_NAME_COL_NUM];
// 
// 		if ( Mouse_DetectOverlap(the_mouse, this_label->rect_) || Mouse_DetectOverlap(the_mouse, this_file->icon_rect_) )
// 		{
// 			if (this_file->selected_)
// 			{
// 				return true;
// 			}
// 		}
// 
// 		the_item = the_item->next_item_;
// 	}
// 
// 	return false;
// }


// check if the passed X coordinate is owned by this panel. returns true if x is between x_ and x_ + width_
bool Panel_OwnsX(WB2KViewPanel* the_panel, int16_t x)
{
	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
	}

	if (x >= the_panel->x_ && x <= (the_panel->x_ + the_panel->width_))
	{
		return true;
	}
	
	return false;
}


// // For mouse drag only: check to see if a folder is at the coordinates passed (in case user will drop them here)
// // if a folder is under the mouse, it will set that folder as the global drop target
// // returns true if any folder is under the mouse pointer
// bool Panel_CheckForMouseOverFolder(WB2KViewPanel* the_panel, MouseTracker* the_mouse, bool highlight_if_folder)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   check the coords passed for intersection with the icon's x, y, height, width. same for label
// 
// 	WB2KList*			the_item;
// 	uint16_t		x_bound;
// 	uint16_t		y_bound;
// 	
// 	if (the_panel == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME); // crash early, crash often
// 	}
// 
// 	x_bound = the_panel->x_ + the_panel->width_;
// 	y_bound = the_panel->y_ + the_panel->height_;
// 
// 	// define the selection area based on lasso coords (if lasso mode) or just pointer + pointer radius
// 	Mouse_UpdateSelectionRectangle(the_mouse, the_panel->my_parent_surface_->content_left_, the_panel->content_top_);
// 
// 	the_item = *(the_panel->root_folder_->list_);
// 
// 	if (the_item == NULL)
// 	{
// 		return false;
// 	}
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
// 		WB2KLabel*			this_label = this_file->label_[PANEL_LIST_NAME_COL_NUM];
// 
// 		if ( Mouse_DetectOverlap(the_mouse, this_label->rect_) || Mouse_DetectOverlap(the_mouse, this_file->icon_rect_) )
// 		{
// 			if (this_file->is_directory_)
// 			{
// 				// mouse is dragged over a folder. if this folder is already selected, it means it is part of the drag. prevent it from being ALSO selected as the drag target.
// 				if (this_file->selected_)
// 				{
// 					return false;
// 				}
// 				
// 				if (highlight_if_folder)
// 				{
// 					// remember the last (=only) folder found, on global level
// 					if (File_MarkAsDropTarget(this_file, the_panel->my_parent_surface_->content_left_, the_panel->content_top_, x_bound, y_bound) != false)
// 					{
// 						FileMover_SetTargetFolderFile(global_app->file_mover_, the_panel->my_parent_surface_, the_panel, this_file);
// 					}
// 				}
// 
// 				return true;
// 			}
// 		}
// 		
// 		the_item = the_item->next_item_;
// 	}
// 
// 	return false;
// }


// // check to see if any files were selected at the coordinates passed.
// // returns 0 if nothing selected, 1 if 1 file, 2 if multiple files, or 3 if one folder (using enums)
// // if do_selection is true, and icon(s) are found, the icons will be flagged as selected and re-rendered with selection appearance
// // if highlight_if_folder is true, and ONE folder is found, that folder will get re-rendered as selected (but not marked as selected)
// IconSelectionResult Panel_CheckForMouseSelection(WB2KViewPanel* the_panel, MouseTracker* the_mouse, bool do_selection, bool highlight_if_folder)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   check the coords passed for intersection with the icon's x, y, height, width. same for label
// 	//   if do_selection is TRUE, then the goal is to select icons. it will set source for FileMover, and clear target.
// 	//   if do_selection is FALSE, then the goal is to see if we are dragging something onto another folder. it will set TARGET for FileMover
// 
// 	int					num_files_selected = 0;
// 	WB2KList*			the_item;
// 	uint16_t		x_bound;
// 	uint16_t		y_bound;
// 	bool				at_least_one_folder_selected = false;
// 	
// 	if (the_panel == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME); // crash early, crash often
// 	}
// 
// 	x_bound = the_panel->x_ + the_panel->width_;
// 	y_bound = the_panel->y_ + the_panel->height_;
// 
// 	// define the selection area based on lasso coords (if lasso mode) or just pointer + pointer radius
// 	Mouse_UpdateSelectionRectangle(the_mouse, the_panel->my_parent_surface_->content_left_, the_panel->content_top_);
// 
// 	the_item = *(the_panel->root_folder_->list_);
// 
// 	if (the_item == NULL)
// 	{
// 		return selection_error;
// 	}
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
// 		WB2KLabel*			this_label = this_file->label_[PANEL_LIST_NAME_COL_NUM];
// 
// 		if ( Mouse_DetectOverlap(the_mouse, this_label->rect_) || Mouse_DetectOverlap(the_mouse, this_file->icon_rect_) )
// 		{
// 			++num_files_selected;
// 
// 			if (this_file->is_directory_)
// 			{
// 				// this folder is not part of the drag selection, so ok to let it be the drag target
// 				at_least_one_folder_selected = true;
// 			}			
// 
// 			// mark file as selected?
// 			if (do_selection)
// 			{
// 				if (File_MarkSelected(this_file, the_panel->my_parent_surface_->content_left_, the_panel->content_top_, x_bound, y_bound, the_panel->col_highest_visible_) == false)
// 				{
// 					// the passed file was null. do anything?
// 					LOG_ERR(("%s %d: couldn't mark file '%s' as selected", __func__ , __LINE__, this_file->file_name_));
// 					App_Exit(ERROR_DEFINE_ME);
// 				}
// 			}
// 		}
// 
// 		the_item = the_item->next_item_;
// 	}
// 
// 
// 	// now proceed with rest of return logic
// 	if (num_files_selected == 0)
// 	{
// 		// if this was not a check to see if we were dropping on a folder, then we have selected nothing, so clear any existing FileMover source
// 		if (do_selection)
// 		{
// 			FileMover_ClearSource(global_app->file_mover_);
// 		}
// 		
// 		return selection_none;
// 	}
// 	else
// 	{
// 		FileMover_SetSource(global_app->file_mover_, the_panel->my_parent_surface_, the_panel, the_panel->root_folder_);
// 		
// 		if (num_files_selected == 1)
// 		{
// 			if (at_least_one_folder_selected)
// 			{
// 				return selection_single_folder;
// 			}
// 			else
// 			{
// 				return selection_single_file;
// 			}
// 		}
// 		else
// 		{
// 			return selection_multiple_file;
// 		}
// 	}
// }

					
// rename the currently selected file
bool Panel_RenameCurrentFile(WB2KViewPanel* the_panel)
{
	int16_t				the_current_row;
	WB2KFileObject*		the_file;
	bool				success;
	int8_t				the_player_choice;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (the_current_row < 0)
	{
		return false;
	}
	
	the_file = Folder_FindFileByRow(the_panel->root_folder_, the_current_row);

// 	global_dlg.title_text_ = STR_MSG_TINKER_START_TITLE;
// 	global_dlg.body_text_ = STR_MSG_TINKER_START_BODY;
// 	global_dlg.btn_label_[0] = STR_MSG_TINKER_START_NO;
// 	global_dlg.btn_label_[1] = STR_MSG_TINKER_START_YES;
// 
// 	the_player_choice = Text_DisplayDialog(&tinker_end_dlg, char_save_mem);
// 
// 	if (the_player_choice != 1)
// 	{
// 		continue;
// 	}

	success = File_GetHexContents(the_file, (char*)global_temp_buff_384b_2);
	
	Screen_Render();	// the hex view has completely overwritten the screen
	Panel_RenderContents(the_panel);
	
	return success;
}


// delete the currently selected file
bool Panel_DeleteCurrentFile(WB2KViewPanel* the_panel)
{
	int16_t				the_current_row;
	WB2KFileObject*		the_file;
	bool				success;
	int8_t				the_player_choice;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (the_current_row < 0)
	{
		return false;
	}
	
	the_file = Folder_FindFileByRow(the_panel->root_folder_, the_current_row);

	General_Strlcpy((char*)&global_dlg_title, General_GetString(ID_STR_MSG_CONFIRM_DELETION), 36);
	General_Strlcpy((char*)&global_dlg_body_msg, General_GetString(ID_STR_DLG_ARE_YOU_SURE), 70);
	General_Strlcpy((char*)&global_dlg_button[0], General_GetString(ID_STR_DLG_NO), 10);
	General_Strlcpy((char*)&global_dlg_button[1], General_GetString(ID_STR_DLG_YES), 10);

	the_player_choice = Text_DisplayDialog(&global_dlg, (char*)&temp_screen_buffer_char, (char*)&temp_screen_buffer_attr);

	if (the_player_choice != 1)
	{
		return false;
	}

	success = File_Delete(the_file, NULL);
	
	if (success == false)
	{
		return false;
	}
	
	// renew file listing
	Folder_RefreshListing(the_panel->root_folder_);
	Panel_Init(the_panel);
	
	return success;
}


// show the contents of the currently selected file as a hex dump
bool Panel_ViewCurrentFileAsHex(WB2KViewPanel* the_panel)
{
	int16_t				the_current_row;
	WB2KFileObject*		the_file;
	bool				success;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (the_current_row < 0)
	{
		return false;
	}
	
	the_file = Folder_FindFileByRow(the_panel->root_folder_, the_current_row);
	success = File_GetHexContents(the_file, (char*)global_temp_buff_384b_2);
	
	return success;
}


// show the contents of the currently selected file as text
bool Panel_ViewCurrentFileAsText(WB2KViewPanel* the_panel)
{
	int16_t				the_current_row;
	WB2KFileObject*		the_file;
	bool				success;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (the_current_row < 0)
	{
		return false;
	}

	the_file = Folder_FindFileByRow(the_panel->root_folder_, the_current_row);
	success = File_GetTextContents(the_file, (char*)global_temp_buff_384b_2);
	
	return success;
}


// change file selection - user did cursor up
// returns false if action was not possible (eg, you were at top of list already)
bool Panel_SelectPrevFile(WB2KViewPanel* the_panel)
{
	int16_t		the_current_row;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (--the_current_row < 0)
	{
		return false;
	}
	
	return Panel_SetFileSelectionByRow(the_panel, the_current_row, true);
}


// change file selection - user did cursor down
// returns false if action was not possible (eg, you were at bottom of list already)
bool Panel_SelectNextFile(WB2KViewPanel* the_panel)
{
	int16_t		the_current_row;
	uint16_t	the_file_count;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	the_file_count = Folder_GetCountFiles(the_panel->root_folder_);
	
	if (++the_current_row == the_file_count)
	{
		// we're already on the last file
		return false;
	}
	
	return Panel_SetFileSelectionByRow(the_panel, the_current_row, true);
}


// select or unselect 1 file by row id
bool Panel_SetFileSelectionByRow(WB2KViewPanel* the_panel, uint16_t the_row, bool do_selection)
{
	// LOGIC:
	//   iterate through all files in the panel's list
	//   if do_selection is TRUE, then the goal is to mark the file at that row as selected
	//   if do_selection is FALSE, then the goal is to mark the file at that row as unselected.

	uint8_t		content_top = the_panel->content_top_;
	bool		success;
	
	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_SET_FILE_SEL_BY_ROW_PANEL_WAS_NULL); // crash early, crash often
	}

	success = Folder_SetFileSelectionByRow(the_panel->root_folder_, the_row, do_selection, the_panel->y_);
	//Panel_RenderContents(the_panel);

	//sprintf(global_string_buff1, "row=%u, contenttop=%u, height=%u, top+height=%u", the_row, content_top, the_panel->height_, content_top + the_panel->height_);
	//Buffer_NewMessage(global_string_buff1);
	
	// is the newly selected file visible? If not, scroll to make it visible
	if (success)
	{
		if (the_row >= (content_top + the_panel->height_))
		{
			// row is off the bottom of the screen. 
			// to make it visible, increase content_top and reflow and re-render panel
			++the_panel->content_top_;
			Panel_ReflowContent(the_panel);
			Panel_RenderContents(the_panel);
		}
		else if (the_row < content_top)
		{
			--the_panel->content_top_;
			Panel_ReflowContent(the_panel);
			Panel_RenderContents(the_panel);
		}
	}
	
	return success;
}


// de-select all files
bool Panel_UnSelectAllFiles(WB2KViewPanel* the_panel)
{
	// LOGIC:
	//   iterate through all files in the folder's list
	//   for any file that is listed as selected, instruct it to de-select itself
	//   NOTE: this_filetype is used purely to double-check that we have a valid list node. Remove that code if/when I figure out how to only get valid list nodes

	WB2KList*		the_item;
	uint16_t	x_bound;

	if (the_panel == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
	}

	if (the_panel->root_folder_ == NULL)
	{
		LOG_ERR(("%s %d: passed class object had a null root folder", __func__ , __LINE__));
		App_Exit(ERROR_PANEL_ROOT_FOLDER_WAS_NULL); // crash early, crash often
	}

	x_bound = the_panel->x_ + the_panel->width_;

	the_item = *(the_panel->root_folder_->list_);

	if (the_item == NULL)
	{
		return false;
	}

	while (the_item != NULL)
	{
		WB2KFileObject* this_file = (WB2KFileObject*)(the_item->payload_);
		//printf("Folder_UnSelectAllFiles: file %s selected=%i\n", this_file->file_name_, this_file->selected_);

		if (File_MarkUnSelected(this_file, the_panel->y_) == false)
		{
			// the passed file was null. do anything?
		}

		the_item = the_item->next_item_;
	}

	return true;
}


// // Performs an "Open" action on any files in the panel that are marked as selected
// bool Panel_OpenSelectedFiles(WB2KViewPanel* the_panel)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   for any file that is listed as selected, instruct it to open itself
// 	//   NOTE: this_filetype is used purely to double-check that we have a valid list node. Remove that code if/when I figure out how to only get valid list nodes
// 
// 	WB2KList*	the_item;
// 
// 	if (the_panel == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
// 	}
// 
// 	if (the_panel->root_folder_ == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object had a null root folder", __func__ , __LINE__));
// 		App_Exit(ERROR_PANEL_ROOT_FOLDER_WAS_NULL); // crash early, crash often
// 	}
// 
// 	the_item = *(the_panel->root_folder_->list_);
// 
// 	if (the_item == NULL)
// 	{
// 		return false;
// 	}
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
// 		WB2KFolderObject*	the_root_folder;
// 
// 		if (File_IsSelected(this_file) == true)
// 		{
// 			// LOGIC:
// 			//    if folder, open in new file browser window
// 			//    if file, perform some open action that makes sense
// 			if (File_IsFolder(this_file) == true)
// 			{
// 				WB2KWindow*			new_surface;
// 
// 				//if ( (the_root_folder = Folder_GetRootFolderNEW(this_file)) == NULL)
// 				if ( (the_root_folder = Folder_GetRootFolder(this_file->file_path_, this_file->device_name_, this_file->rport_, this_file->datetime_.dat_Stamp)) == NULL)
// 				{
// 					LOG_ERR(("%s %d: Unable to create a folder object for '%s'", __func__ , __LINE__, this_file->file_path_));
// 					goto error;
// 				}
// 
// 				//DEBUG_OUT(("%s %d: root folder's filename='%s', is_disk=%i", __func__ , __LINE__, this_file->file_name_, the_root_folder->is_disk_));
// 
// 				// create a new surface
// 				new_surface = App_GetOrCreateWindow(global_app, the_root_folder, the_panel->view_mode_, this_file, the_panel->my_parent_surface_);
// 
// 				if ( new_surface == NULL )
// 				{
// 					LOG_ERR(("%s %d: Couldn't open a new WB surface", __func__ , __LINE__));
// 					Folder_Destroy(&the_root_folder);
// 					goto error;
// 				}
// 			}
// 			else
// 			{
// 				if (File_Open(this_file) == false)
// 				{
// 					LOG_ERR(("%s %d: Couldn't open the selected file '%s' using WBStartup", __func__ , __LINE__, this_file->file_name_));
// 					goto error;
// 				}
// 			}
// 		}
// 		the_item = the_item->next_item_;
// 	}
// 
// 	return true;
// 	
// error:
// 	return false;
// }


// // move every currently selected file into the specified folder
// // returns -1 in event of error, or count of files moved
// int16_t Panel_MoveSelectedFiles(WB2KViewPanel* the_panel, WB2KFolderObject* the_target_folder)
// {
// 	if (the_panel == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_PANEL_WAS_NULL); // crash early, crash often
// 	}
// 
// 	if (the_target_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed the_target_folder was null", __func__ , __LINE__));
// 		App_Exit(ERROR_PANEL_TARGET_FOLDER_WAS_NULL); // crash early, crash often
// 	}
// 
// 	return Folder_MoveSelectedFiles(the_panel->root_folder_, the_target_folder);
// }


// calculate and set positions for the panel's files, when viewed as list
// call this when window is first opened, or when window size changes, or when list of files is changed.
// this does not actually render any icons, it just calculates where they should be in the window's bitmap.
// note: this also sets/resets the surface's required_inner_width_ property (logical internal width vs physical internal width)
void Panel_ReflowContent(WB2KViewPanel* the_panel)
{
	WB2KList*	the_item;
	uint8_t		num_rows;
	uint8_t		max_rows = PANEL_LIST_MAX_ROWS;
	uint8_t		num_files = 0;
	uint16_t	row;
	int8_t		file_display_row;
	uint8_t		first_viz_row = the_panel->content_top_;
	uint8_t		last_viz_row = first_viz_row + the_panel->height_ - 1;
	
	// LOGIC:
	//   we will scroll as needed vertically
	//   the panel's content_top is 0 when unscrolled (initial position)
	//   a file's y position is calculated based on content top, first row of (inner) panel, and the row # of the file
	//   when scrolled down, any file's that have scrolled up out of the panel will have y positions < top of panel
	//   negative positions will happen for longer directories. 
	
	num_files = Folder_GetCountFiles(the_panel->root_folder_);

	// see how many rows and V space we need by taking # of files (do NOT include space for a header row: that row is part of different spacer)
	num_rows = num_files;
	
	if (num_rows > max_rows)
	{
		LOG_WARN(("%s %d: this folder is showing %u files, which is more than max of %u", __func__ , __LINE__, num_files, max_rows));
	}
	

	// if there are no files in the folder the panel is showing, we can stop here
	if (num_files == 0)
	{
		LOG_INFO(("%s %d: this folder ('%s') shows a file count of 0", __func__ , __LINE__, the_panel->root_folder_->folder_file_->file_name_));
		return;
	}
	
	// set the x and y positions of every file
	// for labels, if the column isn't to be shown, set it's y property to -1
	the_item = *(the_panel->root_folder_->list_);

	// no files?
	if ( the_item == NULL )
	{
		LOG_ERR(("%s %d: this folder ('%s') shows a file count of %u but file list seems to be empty!", __func__ , __LINE__, the_panel->root_folder_->folder_file_->file_name_, num_files));
		App_Exit(ERROR_NO_FILES_IN_FILE_LIST); // crash early, crash often
	}
	
	for (row = 0; row < num_rows && the_item; row++)
	{
		WB2KFileObject*	this_file;
		
		if (row >= first_viz_row && row <= last_viz_row)
		{
			file_display_row = row - first_viz_row;
		}
		else
		{
			file_display_row = -1;
		}

		this_file = (WB2KFileObject*)(the_item->payload_);

		// store the icon's x, y, and rect info so we can use it for mouse detection
		File_UpdatePos(this_file, the_panel->x_, file_display_row, row);
		
		// get next node
		the_item = the_item->next_item_;
	}
	//printf("width checker found %i files; num_cols: %i, col=%i\n", num_files, num_cols, col);

	// remember number of rows used
	the_panel->num_rows_ = num_rows;
}


// clears the panel in preparation for re-rendering it
void Panel_ClearDisplay(WB2KViewPanel* the_panel)
{
	// LOGIC:
	//   for panel in a backdrop window, just set entire thing to a simple pattern
	//   for panel in a regular window, set everything to background color
	
	Text_FillBox(the_panel->x_ + 1, the_panel->y_ + 1, the_panel->x_ + the_panel->width_, the_panel->y_ + the_panel->height_, CH_SPACE, LIST_ACTIVE_COLOR, APP_BACKGROUND_COLOR);
	
	return;

}


// display the main contents of a panel (excludes list header, if any)
// call this whenever window needs to be redrawn
// Panel_ReflowContent must be called (at least once) before calling this
// this routine only renders, it does not do any positioning of icons
void Panel_RenderContents(WB2KViewPanel* the_panel)
{
	WB2KList*	the_item;
	
	// clear the panel. if this is a refresh, it isn't guaranteed panel UI was just drawn. eg, file was deleted. 
	Text_FillBox(
		the_panel->x_, the_panel->y_, 
		the_panel->x_ + (UI_PANEL_INNER_WIDTH - 1), the_panel->y_ + (UI_PANEL_INNER_HEIGHT - 1), 
		CH_SPACE, LIST_ACTIVE_COLOR, APP_BACKGROUND_COLOR
	);
	
	the_item = *(the_panel->root_folder_->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
		
		File_Render(this_file, File_IsSelected(this_file), the_panel->y_, the_panel->active_);
		
		the_item = the_item->next_item_;
	}
	
	// draw folder title in the top tab
	Panel_RenderTitleOnly(the_panel);

	return;
}


// display the title of the panel only
// inverses the title if the panel is active, draws it normally if inactive
void Panel_RenderTitleOnly(WB2KViewPanel* the_panel)
{
	uint8_t		back_color;
	uint8_t		fore_color;

	if (the_panel->active_ == true)
	{
		fore_color = PANEL_BACKGROUND_COLOR;
		back_color = LIST_ACTIVE_COLOR;
	}
	else
	{
		fore_color = LIST_INACTIVE_COLOR;
		back_color = PANEL_BACKGROUND_COLOR;
	}
	
	Text_FillBox(the_panel->x_, the_panel->y_ - 2, the_panel->x_ + (UI_PANEL_TAB_WIDTH - 3), the_panel->y_ - 2, CH_SPACE, fore_color, back_color);
	Text_DrawStringAtXY( the_panel->x_, the_panel->y_ - 2, the_panel->root_folder_->folder_file_->file_name_, fore_color, back_color);
}


// sorts the file list by date/name/etc, then calls the panel to renew its view.
// TODO: consider adding a boolean "do reflow". 
void Panel_SortFiles(WB2KViewPanel* the_panel)
{
	int16_t				the_current_row;
	WB2KFileObject*		the_current_file;
	
	// LOGIC: 
	//   the panel has a concept of currently selected row. this is used to determine bounds for cursor up/down file selection
	//   after sort, the files will visually be in the right order, but current row won't necessarily match up any more
	//   so we get a reference the current file, then sort, then get that file's (possibly) new row number, and use it for current row
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (the_current_row >= 0)
	{
		the_current_file = Folder_FindFileByRow(the_panel->root_folder_, the_current_row);
	}

	List_InitMergeSort(the_panel->root_folder_->list_, the_panel->sort_compare_function_);
	
	//DEBUG_OUT(("%s %d: files sorted", __func__ , __LINE__));
	
	Panel_ReflowContent(the_panel);
	Panel_RenderContents(the_panel);
	
	// now re-set the folder's idea of what the current file is
	if (the_current_file != NULL)
	{
		Folder_SetCurrentRow(the_panel->root_folder_, the_current_file->row_);
	}
}


// Re-select the current file.
// typically after a sort, so that panel's current row gets reset to match new row the previously selected file had
// returns false if action was not possible
bool Panel_ReSelectCurrentFile(WB2KViewPanel* the_panel)
{
	int16_t		the_current_row;
	
	the_current_row = Folder_GetCurrentRow(the_panel->root_folder_);
	
	if (--the_current_row < 0)
	{
		return false;
	}
	
	return Panel_SetFileSelectionByRow(the_panel, the_current_row, true);
}






// TEMPORARY DEBUG FUNCTIONS

// bool printdir (char *newdir)
// {
// return true;
// }
// 
// void test_dir_stuff (void)
// {
// return;
// }

// // returns true for error, false for OK
// bool printdir (char *newdir)
// {
//     char *olddir;
//     char *curdir;
//     DIR *dir;
//     struct dirent *ent;
//     char *subdirs = NULL;
//     unsigned dirnum = 0;
//     unsigned num;
// 
//     olddir = malloc (FILENAME_MAX);
//     if (olddir == NULL) {
//       perror ("cannot allocate memory");
//       return true;
//     }
// 
//     getcwd (olddir, FILENAME_MAX);
//     if (chdir (newdir)) {
// 
//         /* If chdir() fails we just print the
//         ** directory name - as done for files.
//         */
//         printf ("  Dir  %s\n", newdir);
//         free (olddir);
//         return false;
//     }
// 
//     curdir = malloc (FILENAME_MAX);
//     if (curdir == NULL) {
//       perror ("cannot allocate memory");
//       return true;
//     }
// 
//     /* We call getcwd() in order to print the
//     ** absolute pathname for a subdirectory.
//     */
//     getcwd (curdir, FILENAME_MAX);
//     printf (" Dir %s:\n", curdir);
//     free (curdir);
// 
//     /* Calling opendir() always with "." avoids
//     ** fiddling around with pathname separators.
//     */
//     dir = opendir (".");
//     while (ent = readdir (dir)) {
// 
//         if (_DE_ISREG (ent->d_type)) {
//             printf ("  File %s\n", ent->d_name);
//             continue;
//         }
// 
//         /* We defer handling of subdirectories until we're done with the
//         ** current one as several targets don't support other disk i/o
//         ** while reading a directory (see cc65 readdir() doc for more).
//         */
//         if (_DE_ISDIR (ent->d_type)) {
//             subdirs = realloc (subdirs, FILENAME_MAX * (dirnum + 1));
//             strcpy (subdirs + FILENAME_MAX * dirnum++, ent->d_name);
//         }
//     }
//     closedir (dir);
// 
//     for (num = 0; num < dirnum; ++num) {
//         if (printdir (subdirs + FILENAME_MAX * num))
//             break;
//     }
//     free (subdirs);
// 
//     chdir (olddir);
//     free (olddir);
//     return false;
// }
// 
// 
// void test_dir_stuff (void)
// {
//     unsigned char device;
//     char *devicedir;
// 
//     devicedir = malloc (FILENAME_MAX);
//     if (devicedir == NULL) {
//       perror ("cannot allocate memory");
//       return;
//     }
// 
//     /* Calling getfirstdevice()/getnextdevice() does _not_ turn on the motor
//     ** of a drive-type device and does _not_ check for a disk in the drive.
//     */
//     device = getfirstdevice ();
//     while (device != INVALID_DEVICE) {
//         printf ("Device %d:\n", device);
// 
//         /* Calling getdevicedir() _does_ check for a (formatted) disk in a
//         ** floppy-disk-type device and returns NULL if that check fails.
//         */
//         if (getdevicedir (device, devicedir, FILENAME_MAX)) {
//             printdir (devicedir);
//         } else {
//             printf (" N/A\n");
//         }
// 
//         device = getnextdevice (device);
//     }
// 
//     if (doesclrscrafterexit ()) {
//         getchar ();
//     }
// 
//     free (devicedir);
// }