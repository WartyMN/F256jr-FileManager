/*
 * folder.c
 *
 *  Created on: Nov 21, 2020
 *      Author: micahbly
 */


/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes
#include "app.h"
#include "comm_buffer.h"
#include "file.h"
#include "folder.h"
#include "general.h"
#include "list.h"
#include "list_panel.h"
#include "text.h"
#include "strings.h"

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <device.h>

// F256 includes
#include <f256.h>



/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/


/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/

extern uint8_t*		global_temp_buff_192b_1;
extern uint8_t*		global_temp_buff_192b_2;
extern uint8_t*		global_temp_buff_192b_3;

extern char*		global_string_buff1;
extern char*		global_string_buff2;



// void cmd_ls(void)
// {
//     DIR			*dir;
//     struct		dirent *dirent;
// 	uint8_t		x1 = 1;
// 	uint8_t		y1 = 5;
// 	uint8_t		file_cnt = 0;
// 	
//     /* print directory listing */
// 
//     dir = opendir(".");
//     if (! dir) {
//         puts("opendir failed");
//         return;
//     }
// 
//     while (dirent = readdir(dir))
//     {
//         // is this is the disk name, or a file?
//         if (dirent->d_type == _CBM_T_HEADER)
//         {
// 			sprintf(global_temp_buff_192b_2, General_GetString(ID_STR_DEV_NAME), 8);
// 			sprintf(global_temp_buff_192b_1, "%s: %s", global_temp_buff_192b_2, dirent->d_name);
//  			Text_DrawStringAtXY(x1, 3, global_temp_buff_192b_1);
//        }
//         else
//         {
// 			Text_DrawStringAtXY(x1, y1  + file_cnt, dirent->d_name);
// 			sprintf(global_temp_buff_192b_1, "%u blck", dirent->d_blocks);
// 			Text_DrawStringAtXY(x1 + 20, y1  + file_cnt, global_temp_buff_192b_1);
// 			//Text_SetCharAtXY(x1 + 30, y1  + file_cnt, dirent->d_type);
// 			Text_DrawStringAtXY(x1 + 29, y1  + file_cnt, General_GetFileTypeString(dirent->d_type));
// 			++file_cnt;
//        }
// 	}
// 	
//     closedir(dir);
// }


/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/

// free every fileobject in the folder's list, and remove the nodes from the list
void Folder_DestroyAllFiles(WB2KFolderObject* the_folder);

// looks through all files in the file list, comparing the passed file object, and turning true if found in the list
// use case: checking if a given file in a selection pool is also the potential target for a drag action
bool Folder_InFileList(WB2KFolderObject* the_folder, WB2KFileObject* the_file, uint8_t the_scope);

// looks through all files in the file list, comparing the passed string to the filename of each file.
// Returns NULL if nothing matches, or returns pointer to first matching list item
WB2KList* Folder_FindListItemByFileName(WB2KFolderObject* the_folder, char* the_file_name);

// looks through all files in the file list, comparing the passed string to the filepath of each file.
// Returns NULL if nothing matches, or returns pointer to first matching list item
WB2KList* Folder_FindListItemByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len);

// looks through all files in the file list, comparing the passed string to the filepath of each file.
// Returns NULL if nothing matches, or returns pointer to first matching FileObject
WB2KFileObject* Folder_FindFileByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len);


/*****************************************************************************/
/*                       Private Function Definitions                        */
/*****************************************************************************/


// free every fileobject in the panel's list, and remove the nodes from the list
void Folder_DestroyAllFiles(WB2KFolderObject* the_folder)
{
	int			num_nodes = 0;
	WB2KList*	the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DESTROY_ALL_FOLDER_WAS_NULL);	// crash early, crash often
	}
	
	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
		
		File_Destroy(&this_file);
		++num_nodes;
		--the_folder->file_count_;

		the_item = the_item->next_item_;
	}

	// now free up the list items themselves
	List_Destroy(the_folder->list_);

	//DEBUG_OUT(("%s %d: %i files freed", __func__ , __LINE__, num_nodes));
	//Buffer_NewMessage("Done destroying all files in folder");
	
	return;
}


// looks through all files in the file list, comparing the passed file object, and turning true if found in the list
// use case: checking if a given file in a selection pool is also the potential target for a drag action
bool Folder_InFileList(WB2KFolderObject* the_folder, WB2KFileObject* the_file, uint8_t the_scope)
{
	WB2KList*	the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}
	
	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject* this_file = (WB2KFileObject*)(the_item->payload_);

		// is this the item we are looking for?
		if (the_scope == LIST_SCOPE_ALL || (the_scope == LIST_SCOPE_SELECTED && this_file->selected_) || (the_scope == LIST_SCOPE_NOT_SELECTED && this_file->selected_ == false))
		{
			if (this_file == the_file)
			{
				return true;
			}
		}

		the_item = the_item->next_item_;
	}

	return false;
}


// looks through all files in the file list, comparing the passed string to the filename of each file.
// Returns NULL if nothing matches, or returns pointer to first matching list item
WB2KList* Folder_FindListItemByFileName(WB2KFolderObject* the_folder, char* the_file_name)
{
	// LOGIC:
	//   iterate through all files in the panel's list
	//   when comparing, the int compare_len is used. This allows an incoming string with .info to be matched easily against a parent filename without the .info.
	WB2KList*	the_item;
	short		the_compare_len = General_Strnlen(the_file_name, FILE_MAX_FILENAME_SIZE);

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}
	
	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject* this_file = (WB2KFileObject *)(the_item->payload_);

		// is this the item we are looking for?
		//DEBUG_OUT(("%s %d: examining file '%s' (len %i) against '%s' (len %i)", __func__ , __LINE__, this_file->file_name_, General_Strnlen(this_file->file_name_, FILE_MAX_PATHNAME_SIZE), the_file_name, the_compare_len));
		
		if ( General_Strnlen(this_file->file_name_, FILE_MAX_FILENAME_SIZE) == the_compare_len )
		{			
			//DEBUG_OUT(("%s %d: lengths reported as match", __func__ , __LINE__));
			
			if ( (General_Strncasecmp(the_file_name, this_file->file_name_, the_compare_len)) == 0)
			{
				return the_item;
			}
		}

		the_item = the_item->next_item_;
	}

	//DEBUG_OUT(("%s %d: no match to filename '%s'", __func__ , __LINE__, the_file_name));
	
	return NULL;
}


// // looks through all files in the file list, comparing the passed string to the filepath of each file.
// // if check_device_name is set, it will not return a match unless the drive codes also match up
// // Returns NULL if nothing matches, or returns pointer to first matching list item
// WB2KList* Folder_FindListItemByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   when comparing, the int compare_len is used. This allows an incoming string with .info to be matched easily against a parent filename without the .info.
// 	WB2KList*	the_item;
// 
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		return NULL;
// 	}
// 	
// 	the_item = *(the_folder->list_);
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject* this_file = (WB2KFileObject *)(the_item->payload_);
// 
// 		// is this the item we are looking for?
// 		//DEBUG_OUT(("%s %d: examining file '%s' (len %i) against '%s' (len %i)", __func__ , __LINE__, this_file->file_path_, General_Strnlen(this_file->file_path_, FILE_MAX_PATHNAME_SIZE), the_file_path, the_compare_len));
// 		
// 		if ( General_Strnlen(this_file->file_path_, FILE_MAX_PATHNAME_SIZE) == the_compare_len )
// 		{			
// 			//DEBUG_OUT(("%s %d: lengths reported as match", __func__ , __LINE__));
// 			
// 			if ( (General_Strncasecmp(the_file_path, this_file->file_path_, the_compare_len)) == 0)
// 			{
// 				return the_item;
// 			}
// 		}
// 
// 		the_item = the_item->next_item_;
// 	}
// 	
// 	return NULL;
// }


// // looks through all files in the file list, comparing the passed string to the filepath of each file.
// // Returns NULL if nothing matches, or returns pointer to first matching FileObject
// WB2KFileObject* Folder_FindFileByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   when comparing, the int compare_len is used. This allows an incoming string with .info to be matched easily against a parent filename without the .info.
// 
// 	WB2KList*	the_item;
// 
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		return NULL;
// 	}
// 
// 	the_item = Folder_FindListItemByFilePath(the_folder, the_file_path, the_compare_len);
// 
// 	if (the_item == NULL)
// 	{
// 		//DEBUG_OUT(("%s %d: couldn't find path '%s'", __func__ , __LINE__, the_file_path));
// 		return NULL;
// 	}
// 	
// 	return (WB2KFileObject *)(the_item->payload_);
// }





/*****************************************************************************/
/*                        Public Function Definitions                        */
/*****************************************************************************/



// **** CONSTRUCTOR AND DESTRUCTOR *****

	
// constructor
// allocates space for the object and any string or other properties that need allocating
// if make_copy_of_folder_file is false, it will use the passed file object as is. Do not pass a file object that is owned by a folder already (without setting to true)!
WB2KFolderObject* Folder_New(WB2KFileObject* the_root_folder_file, bool make_copy_of_folder_file)
{
	WB2KFolderObject*	the_folder;
	WB2KFileObject*		the_copy_of_folder_file;

	if ( (the_folder = (WB2KFolderObject*)calloc(1, sizeof(WB2KFolderObject)) ) == NULL)
	{
		LOG_ERR(("%s %d: could not allocate memory to create new folder object", __func__ , __LINE__));
		goto error;
	}
	LOG_ALLOC(("%s %d:	__ALLOC__	the_folder	%p	size	%i", __func__ , __LINE__, the_folder, sizeof(WB2KFolderObject)));
	
	// initiate the list, but don't add the first node yet (we don't have any items yet)
	if ( (the_folder->list_ = (WB2KList**)calloc(1, sizeof(WB2KList*)) ) == NULL)
	{
		LOG_ERR(("%s %d: could not allocate memory to create new list", __func__ , __LINE__));
		goto error;
	}
	LOG_ALLOC(("%s %d:	__ALLOC__	the_folder->list_	%p	size	%i", __func__ , __LINE__, the_folder->list_, sizeof(WB2KList*)));

	// LOGIC:
	//   if required, duplicate the folder file
	//   When doing Populate folder on window open, this is not generally desired: we have a folder file we can use, and no Folder object owns it yet.
	//   When opening a sub-folder, however, we need to copy the folder file, because it is going to be owned already by another Folder object.
	if (make_copy_of_folder_file == true)
	{
		if ( (the_copy_of_folder_file = File_Duplicate(the_root_folder_file)) == NULL)
		{
			LOG_ERR(("%s %d: Couldn't get a duplicate of the folder file object", __func__ , __LINE__));
			App_Exit(ERROR_FILE_DUPLICATE_FAILED); // crash early, crash often
		}
		
		the_folder->folder_file_ = the_copy_of_folder_file;
	}
	else
	{
		the_folder->folder_file_ = the_root_folder_file;
	}
	
	// set some other props
	the_folder->file_count_ = 0;
	the_folder->total_blocks_ = 0;
	the_folder->selected_blocks_ = 0;

	return the_folder;

error:
	if (the_folder)		free(the_folder);
	return NULL;
}


// reset the folder, without destroying it, to a condition where it can be completely repopulated
// destroys all child objects except the folder file, which is emptied out
// returns false on any error
bool Folder_Reset(WB2KFolderObject* the_folder)
{
	WB2KFileObject**	this_file;
	char**				this_string_p;
	
	// free all files in the folder's file list
	Folder_DestroyAllFiles(the_folder);
	LOG_ALLOC(("%s %d:	__FREE__	(the_folder)->list_	%p	size	%i", __func__ , __LINE__, (the_folder)->list_, sizeof(WB2KList*)));
	free((the_folder)->list_);
	(the_folder)->list_ = NULL;
	
	// free the file size string, file name and file path of the folder file, as they are no longer valid.

	this_string_p = &the_folder->folder_file_->file_size_string_;
	the_folder->folder_file_->file_size_string_ = NULL;
	if (*this_string_p)
	{
		free(*this_string_p);
	}
	
	this_string_p = &the_folder->folder_file_->file_name_;
	the_folder->folder_file_->file_name_ = NULL;
	if (*this_string_p)
	{
		free(*this_string_p);
	}
	
	this_string_p = &the_folder->file_path_;
	the_folder->file_path_ = NULL;
	if (*this_string_p)
	{
		free(*this_string_p);
	}
	
	// initiate the list, but don't add the first node yet (we don't have any items yet)
	if ( (the_folder->list_ = (WB2KList**)calloc(1, sizeof(WB2KList*)) ) == NULL)
	{
		LOG_ERR(("%s %d: could not allocate memory to create new list", __func__ , __LINE__));
		goto error;
	}
	LOG_ALLOC(("%s %d:	__ALLOC__	the_folder->list_	%p	size	%i", __func__ , __LINE__, the_folder->list_, sizeof(WB2KList*)));
	
	// reset some other props
	the_folder->file_count_ = 0;
	the_folder->total_blocks_ = 0;
	the_folder->selected_blocks_ = 0;

	return true;

error:
	if (the_folder)		free(the_folder);
	return false;
}


// destructor
// frees all allocated memory associated with the passed object, and the object itself
void Folder_Destroy(WB2KFolderObject** the_folder)
{
	if (*the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_FOLDER_TO_DESTROY_WAS_NULL);	// crash early, crash often
	}

	if ((*the_folder)->folder_file_ != NULL)
	{
		File_Destroy(&(*the_folder)->folder_file_);
	}

	// free all files in the folder's file list
	Folder_DestroyAllFiles(*the_folder);
	LOG_ALLOC(("%s %d:	__FREE__	(*the_folder)->list_	%p	size	%i", __func__ , __LINE__, (*the_folder)->list_, sizeof(WB2KList*)));
	free((*the_folder)->list_);
	(*the_folder)->list_ = NULL;
	
	// free the folder object itself
	LOG_ALLOC(("%s %d:	__FREE__	*the_folder	%p	size	%i", __func__ , __LINE__, *the_folder, sizeof(WB2KFolderObject)));
	free(*the_folder);
	*the_folder = NULL;
}




// **** SETTERS *****




// sets the row num (-1, or 0-n) of the currently selected file
void Folder_SetCurrentRow(WB2KFolderObject* the_folder, int16_t the_row_number)
{
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_SET_CURR_ROW_FOLDER_WAS_NULL);	// crash early, crash often
	}
	
	the_folder->cur_row_ = the_row_number;
}



// **** GETTERS *****

// returns the list of files associated with the folder
WB2KList** Folder_GetFileList(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}
	
	return the_folder->list_;
}


// returns the file object for the root folder
WB2KFileObject* Folder_GetFolderFile(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}
	
	return the_folder->folder_file_;
}


// returns true if folder has any files/folders in it. based on curated file_count_ property, not on a live check of disk.
bool Folder_HasChildren(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}
	
	if (the_folder == NULL)
	{
		return false;
	}

	if (the_folder->file_count_ == 0)
	{
		return false;
	}

	return true;
}


// returns total number of files in this folder
uint16_t Folder_GetCountFiles(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}
	
	return the_folder->file_count_;
}


// returns the row num (-1, or 0-n) of the currently selected file
int16_t Folder_GetCurrentRow(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_GET_CURR_ROW_FOLDER_WAS_NULL);	// crash early, crash often
	}
	
	return the_folder->cur_row_;
}


// returns true if folder has any files/folders showing as selected
bool Folder_HasSelections(WB2KFolderObject* the_folder)
{
	// TODO: OPTIMIZATION - think about having dedicated loop for this check that stops on first hit. will be faster with bigger folders.
	// TODO: OPTIMIZATION - think about tracking this as a class property instead, and update when files are selected/unselected? Not sure which would be faster. 
	
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	if (Folder_GetCountSelectedFiles(the_folder) == 0)
	{
		return false;
	}

	return true;
}


// returns number of currently selected files in this folder
uint16_t Folder_GetCountSelectedFiles(WB2KFolderObject* the_folder)
{
	// LOGIC:
	//   iterate through all files in the folder's list and count any that are marked as selected

	uint16_t	the_count = 0;
	WB2KList*		the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);

		if (this_file->selected_)
		{
			++the_count;
		}

		the_item = the_item->next_item_;
	}

	return the_count;
}


// returns the first selected file/folder in the folder.
// use Folder_GetCountSelectedFiles() first if you need to make sure you will be getting the only selected file.
WB2KFileObject* Folder_GetFirstSelectedFile(WB2KFolderObject* the_folder)
{
	// LOGIC:
	//   iterate through all files in the folder's list and return the first file/folder marked as selected

	WB2KList*		the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}

	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);

		if (this_file->selected_)
		{
			return this_file;
		}

		the_item = the_item->next_item_;
	}

	return NULL;
}


// returns the first file/folder in the folder.
WB2KFileObject* Folder_GetFirstFile(WB2KFolderObject* the_folder)
{
	WB2KList*		the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}

	the_item = *(the_folder->list_);

	if (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
		return this_file;
	}

	return NULL;
}


// returns the lowest or highest row number used by all the selected files in the folder
// WARNING: will always return a number, even if no files selected, so calling function must have made it's own checks on selection where necessary
uint16_t Folder_GetMinOrMaxSelectedRow(WB2KFolderObject* the_folder, bool find_max)
{
	// LOGIC:
	//   iterate through all files in the folder's list and keep track of the lowest row # for those that are selected

	uint16_t	boundary = 0xFFFF;
	WB2KList*		the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	the_item = *(the_folder->list_);

	if (find_max)
	{
		boundary = 0;
	}

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);

		if (this_file->selected_)
		{
			uint16_t		this;

			this = this_file->row_;
			
			if (find_max)
			{
				if (this > boundary) boundary = this;
			}
			else
			{
				if (this < boundary) boundary = this;
			}
		}

		the_item = the_item->next_item_;
	}

	return boundary;
}


// looks through all files in the file list, comparing the passed string to the filename_ of each file.
// Returns NULL if nothing matches, or returns pointer to first FileObject with a filename that starts with the same string as the one passed
// DOES NOT REQUIRE a match to the full filename. case insensitive search is used.
WB2KFileObject* Folder_FindFileByFileNameStartsWith(WB2KFolderObject* the_folder, char* string_to_match, int compare_len)
{
	// LOGIC:
	//   iterate through all files in the panel's list
	//   when comparing, the int compare_len is used to limit the number of chars of filename that are searched

	WB2KList*		the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}

	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);

		// is this the item we are looking for?
		if ( General_Strncasecmp(string_to_match, this_file->file_name_, compare_len) == 0)
		{
			return this_file;
		}

		the_item = the_item->next_item_;
	}

	DEBUG_OUT(("%s %d: couldn't find filename match for '%s'. compare_len=%i", __func__ , __LINE__, string_to_match, compare_len));

	return NULL;
}


// looks through all files in the file list, comparing the passed row to that of each file.
// Returns NULL if nothing matches, or returns pointer to first matching FileObject
WB2KFileObject* Folder_FindFileByRow(WB2KFolderObject* the_folder, uint8_t the_row)
{
	WB2KList*	the_item;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		return NULL;
	}
	
	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);

		// is this the item we are looking for?
		if ( this_file->row_ == the_row)
		{
			return this_file;
		}

		the_item = the_item->next_item_;
	}

	DEBUG_OUT(("%s %d: couldn't find row %i", __func__ , __LINE__, the_row));

	return NULL;
}






// **** OTHER FUNCTIONS *****


// populate the files in a folder by doing a directory command
uint8_t Folder_PopulateFiles(WB2KFolderObject* the_folder)
{
	uint16_t			file_cnt = 0;
	uint8_t				the_error_code = ERROR_NO_ERROR;
	char				the_parent_path_buffer[FILE_MAX_PATHNAME_SIZE];
	char*				the_parent_path = the_parent_path_buffer;
	uint16_t			path_len;
	WB2KFileObject*		this_file;
	char*				this_file_name;
	char				the_path_buffer[FILE_MAX_PATHNAME_SIZE] = "";
	char*				the_path = the_path_buffer;
	char				extension_buffer[FILE_MAX_FILENAME_SIZE];
	char*				the_file_extension = extension_buffer;
	bool				file_added;
	struct DIR*			dir;
	struct dirent*		dirent;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_POPULATE_FILES_FOLDER_WAS_NULL);	// crash early, crash often
	}

	if (the_folder->folder_file_ == NULL)
	{
		sprintf(global_string_buff1, "folder file was null");
		Buffer_NewMessage(global_string_buff1);
		LOG_ERR(("%s %d: passed file was null", __func__ , __LINE__));
		App_Exit(ERROR_FOLDER_FILE_WAS_NULL);	// crash early, crash often
	}

	if (the_folder->folder_file_->file_path_ == NULL)
	{
		sprintf(global_string_buff1, "filepath for folder file '%s' as null", the_folder->folder_file_->file_path_);
		Buffer_NewMessage(global_string_buff1);
		LOG_ERR(("%s %d: passed file's filepath was null", __func__ , __LINE__));
		App_Exit(ERROR_FOLDER_WAS_NULL);	// crash early, crash often
	}

	// set up base path for the folder + /. we will use this to build the filepaths for the files individually
	
	General_Strlcpy(the_parent_path, the_folder->folder_file_->file_path_, FILE_MAX_PATHNAME_SIZE);
// 	path_len = General_Strnlen(the_folder->folder_file_->file_path_, FILE_MAX_PATHNAME_SIZE);
// 	General_Strlcat(the_parent_path, (char*)"/", FILE_MAX_PATHNAME_SIZE);

	// reset panel's file count, as we will be starting over from zero
	the_folder->file_count_ = 0;

#define _CBM_T_HEADER   0x05U   /* Disk header / title */

    /* print directory listing */

	dir = opendir(the_folder->folder_file_->file_path_);
	//dir = opendir(".");
	if (! dir) {
		//sprintf(global_string_buff1, "opendir failed. filepath='%s'. errno=%u", the_folder->folder_file_->file_path_, errno);
		sprintf(global_string_buff1, "opendir failed. filepath='%s'", the_folder->folder_file_->file_path_);
		Buffer_NewMessage(global_string_buff1);
		return ERROR_COULD_NOT_OPEN_DIR;
	}
    while ( (dirent = readdir(dir)) != NULL )
    {
        // is this is the disk name, or a file?
//         if (dirent->d_type == _CBM_T_HEADER)
//         {
// 			General_Strlcpy(the_folder->folder_file_->file_name_, dirent->d_name, FILE_MAX_FILENAME_SIZE);
// 			the_folder->folder_file_->file_type_ = dirent->d_type;
// 		}
//         else
        {
			this_file_name = dirent->d_name;
			General_Strlcpy(the_path, the_parent_path, FILE_MAX_PATHNAME_SIZE);
			General_Strlcat(the_path, this_file_name, FILE_MAX_PATHNAME_SIZE);

			this_file = File_New(this_file_name, the_path, false, 111, 1, the_folder->device_number_, the_folder->unit_number_, file_cnt);

			if (this_file == NULL)
			{
				LOG_ERR(("%s %d: Could not allocate memory for file object", __func__ , __LINE__));
				the_error_code = ERROR_COULD_NOT_CREATE_NEW_FILE_OBJECT;
				goto error;
			}

			// Add this file to the list of files
			file_added = Folder_AddNewFile(the_folder, this_file);

			// if this is first file in scan, preselect it
			if (file_cnt == 0)
			{
				this_file->selected_ = true;
			}
	
			++file_cnt;
			
			sprintf(global_string_buff1, "cnt=%u, new file='%s' ('%s')", file_cnt, this_file->file_name_, this_file->file_path_);
			Buffer_NewMessage(global_string_buff1);
		}

	}

	closedir(dir);

	// set current row to first file, or -1
	the_folder->cur_row_ = (file_cnt > 0 ? 0 : -1);
	
	// debug
// 	List_Print(the_folder->list_, &File_Print);
// 	DEBUG_OUT(("%s %d: Total bytes %lu", __func__ , __LINE__, the_folder->total_bytes_));
	Folder_Print(the_folder);
	
	return (the_error_code);
	
error:
	if (dir)	closedir(dir);
	
	return (the_error_code);
}



// copies the passed file/folder. If a folder, it will create directory on the target volume if it doesn't already exist
bool Folder_CopyFile(WB2KFolderObject* the_folder, WB2KList* the_item, WB2KFolderObject* the_target_folder)
{
	WB2KFileObject*		the_file = (WB2KFileObject*)the_item->payload_;
	int16_t			bytes_copied;
	char		the_path_buffer[FILE_MAX_PATHNAME_SIZE] = "";
	char*		the_target_file_path = the_path_buffer;
	char*		the_target_folder_path;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_COPY_FILE_SOURCE_FOLDER_WAS_NULL);	// crash early, crash often
	}

	if (the_target_folder == NULL)
	{
		LOG_ERR(("%s %d: param the_target_folder was null", __func__ , __LINE__));
		App_Exit(ERROR_COPY_FILE_TARGET_FOLDER_WAS_NULL);	// crash early, crash often
	}
	
	// NOTE 2023/01/14: b128 doesn't support subdirectories, and F256 SD card stuff is not ready yet, so look at this later. 
	return true;
	
// 	// LOGIC:
// 	//   if a file, call routine to copy bytes of file. then call again for the info file.
// 	//   if a folder:
// 	//     We only have a folder object for the current source folder, we do not have one for the target
// 	//     So every time we encounter a source folder, we have to generate the equivalent path for the target and store in FileMover
// 	//     Then check if the folder path exists on the target volume. Call makedir to create the folder path. then copy info file bytes. 
// 	//       NOTE: there is an assumption that the target folder is a real, existing path, so no need to create "up the chain". This assumption is based on how copy files works.
// 	//     This routine will not attempt to delete folders and their contents if they already exist, it will happily copy files into those folders. (more windows than mac)
// 	//   in either case, add the file and its info file (if any) to any open windows showing the target folder
// 	//     NOTE: in case of folder, we have to copy the info file before updating the target folder chain, or info file will be placed IN the folder, rather than next to it
// 	
// 	if (the_file->is_directory_)
// 	{
// 		// handle a folder...
// 
// 		BPTR 			the_dir_lock;
// 		BPTR 			the_new_dir_lock;
// //		FileMover_UpdateCurrentTargetFolderPath(App_GetFileMover(global_app), the_file->file_path_);
// 		the_target_folder_path = FileMover_GetCurrentTargetFolderPath(App_GetFileMover(global_app));
// 		
// 		// have FileMover build a new target folder path
// 		FileMover_UpdateCurrentTargetFolderPath(App_GetFileMover(global_app), the_file->file_path_);
// 		the_target_folder_path = FileMover_GetCurrentTargetFolderPath(App_GetFileMover(global_app));
// 				
// 		// try to get lock on the  directory, and if we can't, make a new directory
// 		if ( (the_dir_lock = Lock((STRPTR)the_target_folder_path, SHARED_LOCK)) == 0)
// 		{
// 			//DEBUG_OUT(("%s %d: not able to lock target folder '%s'; suggests it doesn't exist yet; will create", __func__ , __LINE__, the_target_folder_path));
// 			
// 			if ( (the_new_dir_lock = CreateDir((STRPTR)the_target_folder_path)) == 0)
// 			{
// 				LOG_ERR(("%s %d: not able to create target folder '%s'! This will cause subsequent copy actions to fail!", __func__ , __LINE__, the_target_folder_path));
// 				UnLock(the_dir_lock);
// 				return false;
// 			}
// 			
// 			DEBUG_OUT(("%s %d: created folder '%s'", __func__ , __LINE__, the_target_folder_path));
// 			UnLock(the_new_dir_lock);
// 	
// 			FileMover_IncrementProcessedFileCount(App_GetFileMover(global_app));
// 		}
// 		else
// 		{
// 			DEBUG_OUT(("%s %d: got a lock on folder '%s'; suggests it already exists", __func__ , __LINE__, the_target_folder_path));
// 		}
// 	}
// 	else
// 	{
// 		// handle a file...
// 
// 		// update target file path without adding the source file's filename to it
// 		FileMover_UpdateCurrentTargetFolderPath(App_GetFileMover(global_app), the_folder->folder_file_->file_path_);
// 		the_target_folder_path = FileMover_GetCurrentTargetFolderPath(App_GetFileMover(global_app));
// 		
// 		// build a file path for target file, based on FileMover's current target folder path and source file name
// 		General_CreateFilePathFromFolderAndFile(the_target_file_path, the_target_folder_path, the_file->file_name_);
// 
// 		// call function to copy file bits
// 		DEBUG_OUT(("%s %d: copying file '%s' to '%s'...", __func__ , __LINE__, the_file->file_path_, the_target_file_path));
// 		bytes_copied = FileMover_CopyFileBytes(App_GetFileMover(global_app), the_file->file_path_, the_target_file_path);
// 	
// 		FileMover_IncrementProcessedFileCount(App_GetFileMover(global_app));
// 		
// 		// for either folder or file, if there is an info file, copy that too
// 		if (the_file->info_file_ != NULL)
// 		{
// 			// build a file path for target file, based on FileMover's current target folder path and source file name
// 			the_target_folder_path = FileMover_GetCurrentTargetFolderPath(App_GetFileMover(global_app));
// 			General_CreateFilePathFromFolderAndFile(the_target_file_path, the_target_folder_path, the_file->info_file_->file_name_);
// 
// 			DEBUG_OUT(("%s %d: file '%s' had an info file, copying to '%s'", __func__ , __LINE__, the_file->file_path_, the_target_file_path));
// 			bytes_copied = FileMover_CopyFileBytes(App_GetFileMover(global_app), the_file->info_file_->file_path_, the_target_file_path);
// 	
// 			FileMover_IncrementProcessedFileCount(App_GetFileMover(global_app));
// 		}
// 		else
// 		{
// 			//DEBUG_OUT(("%s %d: file '%s' did not have an info file, so none will be copied.", __func__ , __LINE__, the_file->file_path_));
// 		}
// 	}	
// 	
// 	// mark the file as not selected 
// 	//File_SetSelected(the_file, false);
// 
// 	// add a copy of the file to any open panels match the target folder (but aren't it), then add the original to this target folder
// 	//DEBUG_OUT(("%s %d: Adding file '%s' to any matching open windows/panels...", __func__ , __LINE__, the_file->file_name_));
// // 	change_made = App_ModifyOpenFolders(global_app, the_target_folder, the_file, &Folder_AddNewFileAsCopy);
// // 	change_made = Folder_AddNewFile(the_target_folder, the_file);
// 			
// 	return true;
}


// deletes the passed file/folder. If a folder, it must have been previously emptied of files.
bool Folder_DeleteFile(WB2KFolderObject* the_folder, WB2KList* the_item, WB2KFolderObject* not_needed)
{
	WB2KFileObject*		the_file;
	bool				result_doesnt_matter;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	the_file = (WB2KFileObject*)the_item->payload_;
	
// 	FileMover_SetCurrentFileName(App_GetFileMover(global_app), the_file->file_name_);
	
	// delete the files
	if (File_Delete(the_file, NULL) == false)
	{
		return false;
	}

// 	FileMover_IncrementProcessedFileCount(App_GetFileMover(global_app));

	LOG_INFO(("%s %d: deleted file '%s' from disk", __func__ , __LINE__, the_file->file_name_));

// 	// if this was a folder file, check if any open windows were representing its contents, and close them. 
// 	if (the_file->is_directory_)
// 	{
// 		WB2KList*			the_window_item;
// 	
// 		while ((the_window_item = App_FindSurfaceListItemByFilePath(global_app, the_file->file_path_)) != NULL)
// 		{
// 			// a window was open with this volume / file as its root folder. close the window
// 			App_CloseOneWindow(global_app, the_window_item);
// 		}
// 	}
// 
	// update the count of files and remove this item from the folder's list of files
	Folder_RemoveFileListItem(the_folder, the_item, DESTROY_FILE_OBJECT);
	
	return true;
}


// removes the passed list item from the list of files in the folder. Does NOT delete file from disk. Optionally frees the file object.
void Folder_RemoveFileListItem(WB2KFolderObject* the_folder, WB2KList* the_item, bool destroy_the_file_object)
{
	WB2KFileObject*		the_file;
	uint32_t			bytes_removed = 0;
	uint16_t			blocks_removed = 0;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	the_file = (WB2KFileObject*)(the_item->payload_);
	
	// before removing, count up the bytes for the file and it's info file, if any. 
// 	bytes_removed = the_file->size_;
// 	blocks_removed = the_file->num_blocks_;
	
	//DEBUG_OUT(("%s %d: file '%s' is being removed from folder '%s' (current bytes=%lu, bytes being removed=%lu)", __func__ , __LINE__, the_file->file_name_, the_folder->folder_file_->file_name_, the_folder->total_bytes_, bytes_removed));
	
	if (destroy_the_file_object)
	{
		File_Destroy(&the_file);
	}
	
	--the_folder->file_count_;
// 	the_folder->total_bytes_ -= bytes_removed;
// 	the_folder->total_blocks_ -= blocks_removed;
	List_RemoveItem(the_folder->list_, the_item);
	LOG_ALLOC(("%s %d:	__FREE__	the_item	%p	size	%i", __func__ , __LINE__, the_item, sizeof(WB2KList)));
	free(the_item);
	the_item = NULL;
	
	return;
}


// removes the passed list item from the list of files in the folder. Does NOT delete file from disk. Does NOT delete the file object.
// returns true if a matching file was found and successfully removed.
// NOTE: this is part of series of functions designed to be called by Window_ModifyOpenFolders(), and all need to return bools.
bool Folder_RemoveFile(WB2KFolderObject* the_folder, WB2KFileObject* the_file)
{
	WB2KList*		the_item;
	
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	the_item = Folder_FindListItemByFileName(the_folder, the_file->file_name_);
	
	if (the_item == NULL)
	{
		// just means this folder never contained a version of this file
		return false;
	}
	
	Folder_RemoveFileListItem(the_folder, the_item, DO_NOT_DESTROY_FILE_OBJECT);
	
	//DEBUG_OUT(("%s %d: file '%s' was removed from folder '%s'", __func__ , __LINE__, the_file->file_name_, the_folder->folder_file_->file_name_));
	
	return true;
}


// Create a new folder on disk, and a new file object for it, and assign it to this folder. 
// if try_until_successful is set, will rename automatically with trailing number until it can make a new folder (by avoiding already-used names)
bool Folder_CreateNewFolder(WB2KFolderObject* the_folder, char* the_file_name, bool try_until_successful, bool create_info_file)
{
	// NOTE 2023/01/14: b128 doesn't support subdirectories, and F256 SD card stuff is not ready yet, so look at this later. 
	return true;

	

// 	WB2KFileObject*		the_file;
// 	bool				created_file_ok = false;
// 	BPTR 				the_dir_lock;
// 	BPTR 				the_new_dir_lock;
// 	char		the_path_buffer[FILE_MAX_PATHNAME_SIZE] = "";
// 	char*		the_target_folder_path = the_path_buffer;
// 	char		the_filename_buffer[FILE_MAX_FILENAME_SIZE] = "";
// 	char*		the_target_file_name = the_filename_buffer;
// 	uint16_t		next_filename_count = 2;	// "unnamed folder 2", "unnamed folder 3", etc. 
// 	struct DiskObject*	the_disk_object;
// 	struct DateStamp*	the_datetime;
// 	
// 	// LOGIC:
// 	//   create a new directory on disk with the filename specified
// 	//     if try_until_successful is set, rename automatically with trailing number until success
// 	//     first try for a lock on the path; if lock succeeds, you know there is an existing file
// 	//     NO OVERCREATION!
// 	//   call Folder_AddNewFile() to add the WB2K file object to the folder
// 	//   mark the folder (file) as selected (expected behavior)
// 
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
// 
// 		
// 	//DEBUG_OUT(("%s %d: before adding new folder, folder '%s' has %lu total bytes, %lu selected files", __func__ , __LINE__, the_folder->folder_file_->file_path_, the_folder->total_bytes_, Folder_GetCountSelectedFiles(the_folder)));
// 
// 	// copy the preferred filename into local storage
// 	General_Strlcpy(the_target_file_name, the_file_name, FILE_MAX_FILENAME_SIZE);
// 	
// 	// loop as many times as necessary until we confirm no file/folder exists at the specified path
// 	//   unless try_until_successful == false
// 	
// 	while (created_file_ok == false)
// 	{
// 		// build a file path for folder file, based on current (parent) folder file path and passed file name
// 		General_CreateFilePathFromFolderAndFile(the_target_folder_path, the_folder->folder_file_->file_path_, the_target_file_name);
// 	
// 		// try to get lock on the  directory to see if it's already in use
// 		if ( (the_dir_lock = Lock((STRPTR)the_target_folder_path, SHARED_LOCK)) == 0)
// 		{
// 			//DEBUG_OUT(("%s %d: not able to lock target folder '%s'; suggests it doesn't exist yet; will create", __func__ , __LINE__, the_target_folder_path));
// 			
// 			if ( (the_new_dir_lock = CreateDir((STRPTR)the_target_folder_path)) == 0)
// 			{
// 				LOG_ERR(("%s %d: not able to create target folder '%s'!", __func__ , __LINE__, the_target_folder_path));
// 				UnLock(the_dir_lock);
// 				return false;
// 			}
// 			
// 			//DEBUG_OUT(("%s %d: created folder '%s'", __func__ , __LINE__, the_target_folder_path));
// 			UnLock(the_new_dir_lock);
// 			
// 			created_file_ok = true;
// 		}
// 		else
// 		{
// 			//DEBUG_OUT(("%s %d: got a lock on folder '%s'; suggests it already exists", __func__ , __LINE__, the_target_folder_path));
// 			
// 			if (try_until_successful == false)
// 			{
// 				// give up at first fail; do not attempt to make unnamed folder 2, 3, etc.
// 				LOG_WARN(("%s %d: requested folder name was already taken while trying to create a new folder at '%s'", __func__ , __LINE__, the_target_folder_path));
// 				return false;
// 			}
// 			
// 			// add/change the number at end of folder name and try again
// 			sprintf((char*)the_target_file_name, "%s %u", the_file_name, next_filename_count);
// 		
// 			// check if we should abandon the effort
// 			if (next_filename_count > FOLDER_MAX_TRIES_AT_FOLDER_CREATION)
// 			{
// 				LOG_ERR(("%s %d: Reached maximum allowed folder names while trying to create a new folder at '%s'", __func__ , __LINE__, the_target_folder_path));
// 				return false;
// 			}
// 		}
// 		
// 		next_filename_count++;
// 	}
// 	
// 	// get timestamp we can use for the folder and the folder.info file
// 	// won't be exactly accurate necessarily, but is for display purposes. If we didn't make it, we'd have to get file lock and example both folder and .info file
// 	the_datetime = General_GetCurrentDateStampWithAlloc();
// 	
// 	// make WB2K file object for the folder that now exists on disk
// 	the_file = File_New(the_target_file_name, the_target_folder_path, the_folder->folder_file_->device_name_, NEW_FILE_IS_FOLDER, the_folder->icon_rport_, 0, *the_datetime, NULL);
// 
// 	LOG_ALLOC(("%s %d:	__ALLOC__	the_datetime	%p	size	%i", __func__ , __LINE__, the_datetime, sizeof(struct DateStamp)));
// 	free(the_datetime);
// 	the_datetime = NULL;
// 	
// 	if (the_file == NULL)
// 	{
// 		LOG_ERR(("%s %d: Could not allocate memory for file object", __func__ , __LINE__));
// 		return false;
// 	}
// 	
// 	// we want the file to be selected for the user
// 	File_SetSelected(the_file, true);
// 
// 	// Add this file to the list of files
// 	if ( Folder_AddNewFile(the_folder, the_file) == true && create_info_file == true)
// 	{
// 		// create info file and associate with the file
// 		
// 		// Create info file on disk too (one will not exist, but this function will have AmigaOS create one)
// 		// NOTE: do this before changing target folder path to point to the .info file
// 		the_disk_object = General_GetInfoStructFromPath(the_target_folder_path, WBDRAWER);
// 		PutDiskObject(the_target_folder_path, the_disk_object);
// 
// 		General_Strlcat(the_target_file_name, FILE_INFO_EXTENSION, FILE_MAX_PATHNAME_SIZE);
// 		General_Strlcat(the_target_folder_path, FILE_INFO_EXTENSION, FILE_MAX_PATHNAME_SIZE);
// 
// 		// TODO: do more robust/elegant solution for showing size of a default folder info file. user could have set their system up with a huge info file
// 
// 		the_info_file = InfoFile_New(the_target_file_name, the_target_folder_path, NULL, FOLDER_UGLY_HACK_DEFAULT_FOLDER_INFO_FILE_SIZE);
// 
// 		if ( the_info_file == NULL)
// 		{
// 			LOG_ERR(("%s %d: Could not create an info file object for '%s'", __func__ , __LINE__, the_target_folder_path));
// 			File_Destroy(&the_file);
// 			return false;
// 		}
// 
// 		// assign the disk structure to the info file (whether we just created it, or it had been there all the time)
// 		the_file->info_file_->info_struct_ = the_disk_object;
// 
// 		// assign the info file to the file
// 		the_file->info_file_ = the_info_file;
// 
// 		// add the info file's size to the ancestor folder
// 		the_folder->total_bytes_ += the_file->info_file_->size_;
// 		
// 		//DEBUG_OUT(("%s %d: after adding new folder, folder '%s' has %lu total bytes, %lu selected files", __func__ , __LINE__, the_folder->folder_file_->file_path_, the_folder->total_bytes_, Folder_GetCountSelectedFiles(the_folder)));
// 	}
// 		
// 	return true;
}

	
// Add a file object to the list of files without checking for duplicates.
// returns true in all cases. 
// NOTE: this is part of series of functions designed to be called by Window_ModifyOpenFolders(), and all need to return bools.
bool Folder_AddNewFile(WB2KFolderObject* the_folder, WB2KFileObject* the_file)
{
	WB2KList*	the_new_item;
// 	uint32_t	bytes_added = 0;
// 	uint16_t	blocks_added = 0;
	
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	// account for bytes of file and info file, if any
// 	bytes_added = the_file->size_;
// 	blocks_added = the_file->num_blocks_;
	
	the_new_item = List_NewItem((void *)the_file);
	List_AddItem(the_folder->list_, the_new_item);
	the_folder->file_count_++;
// 	the_folder->total_bytes_ += bytes_added;
// 	the_folder->total_blocks_ += blocks_added;
	
	//DEBUG_OUT(("%s %d: file '%s' was added to folder '%s'", __func__ , __LINE__, the_file->file_name_, the_folder->folder_file_->file_name_));
	//DEBUG_OUT(("%s %d: file '%s' was added to folder folder '%s' (current bytes=%lu, bytes being added=%lu)", __func__ , __LINE__, the_file->file_name_, the_folder->folder_file_->file_name_, the_folder->total_bytes_, bytes_added));
	
	return true;
}
	
	
// Add a file object to the list of files without checking for duplicates. This variant makes a copy of the file before assigning it. Use case: MoveFiles or CopyFiles.
// returns true in all cases. 
// NOTE: this is part of series of functions designed to be called by Window_ModifyOpenFolders(), and all need to return bools.
bool Folder_AddNewFileAsCopy(WB2KFolderObject* the_folder, WB2KFileObject* the_file)
{
	WB2KFileObject*		the_copy_of_file;
	
	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	if ( (the_copy_of_file = File_Duplicate(the_file)) == NULL)
	{
		LOG_ERR(("%s %d: Couldn't get a duplicate of the file object", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME); // crash early, crash often
		return false;
	}

	return Folder_AddNewFile(the_folder, the_copy_of_file);
}


// // compare 2 folder objects. When done, the original_root_folder will have been updated with removals/additions as necessary to match the updated file list
// // if the folder passed is a system root object, and if a folder (disk) has been removed from it, then any windows open from that disk will be closed
// // returns true if any changes were detected, or false if files appear to be identical
// bool Folder_SyncFolderContentsByFilePath(WB2KFolderObject* original_root_folder, WB2KFolderObject* updated_root_folder)
// {
// 	bool				changes_made = false;
// 	WB2KList**			original_files_list;
// 	WB2KList**			updated_files_list;
// 	WB2KList*			the_original_list_item;
// 	WB2KList*			the_updated_list_item;
// 	WB2KList*			the_item_to_be_deleted;
// 	
// 	// LOGIC for folder compare:
// 	//   we have a list of files from an original version of a folder
// 	//   we have a list of files from a (potentially) updated version of a folder
// 
// 	//   iterate through original file list, comparing each to new file list
// 	//   for any filename match, remove the new item from the new list
// 	//   for a with no filename match, it means it was ejected. 
// 	//   for a file with no filename match, add to the "removed files" list
// 	
// 	// LOGIC for per-file compare:
// 	//   compare name and date of local file to remote descriptor
// 	//   for anything NEW, add a file-request to the queue
// 	//   for anything newer, add a file-request to the queue
// 	//   don't do anything for not-found-locally, or same-as-local
// 	
// 	if (original_root_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: param original_root_folder was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
// 
// 	if (updated_root_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: param updated_root_folder was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
// 
// 	original_files_list = Folder_GetFileList(original_root_folder);
// 	updated_files_list = Folder_GetFileList(updated_root_folder);
// 	
// 	// DEBUG
// 	//DEBUG_OUT(("%s %d: orig files before starting:", __func__ , __LINE__));
// 	//List_Print(original_files_list, &File_Print);
// 	//DEBUG_OUT(("%s %d: updated files before starting:", __func__ , __LINE__));
// 	//List_Print(updated_files_list, &File_Print);
// 	
// 	// iterate through original file list, comparing to everything in the new files list
// 	the_original_list_item = *(original_files_list);
// 
// 	while (the_original_list_item != NULL)
// 	{		
// 		WB2KFileObject* 	the_original_list_file = (WB2KFileObject*)(the_original_list_item->payload_);
// 		short				the_compare_len = strlen((char*)the_original_list_file->file_path_);
// 		
// 		the_updated_list_item = Folder_FindListItemByFilePath(updated_root_folder, the_original_list_file->file_path_, the_compare_len);
// 	
// 		if (the_updated_list_item == NULL)
// 		{
// 			// this file only exists in the original files list. it has been removed/is unavailable
// 			//DEBUG_OUT(("%s %d: orig file '%s' not found in updated list", __func__ , __LINE__, the_original_list_file->file_path_));
// 			the_item_to_be_deleted = the_original_list_item;
// 			changes_made = true;
// 		}
// 		else
// 		{
// 			// this file was in original listing, and in new listing. remove from updated list.
// 			//DEBUG_OUT(("%s %d: orig file '%s' found in both lists", __func__ , __LINE__, the_original_list_file->file_path_));
// 			the_item_to_be_deleted = NULL;
// 			Folder_RemoveFileListItem(updated_root_folder, the_updated_list_item, DESTROY_FILE_OBJECT);			
// 		}		
// 		
// 		the_original_list_item = the_original_list_item->next_item_;
// 		
// 		if (the_item_to_be_deleted)
// 		{
// 			Folder_RemoveFileListItem(original_root_folder, the_item_to_be_deleted, DESTROY_FILE_OBJECT); // do after getting next item in list
// 		}
// 	}
// 
// 	// LOGIC
// 	//   we have checked all the original files against the updated ones.
// 	//   anything still left in the updated list can be understood to have been recently added (eg, an inserted disk)
// 	//   we want to transfer these items to the original folder
// 	//   we do NOT want to remove/destroy them from the updated folder, because the payloads are shared. 
// 
// 	// DEBUG
// 	//DEBUG_OUT(("%s %d: orig files after first pass:", __func__ , __LINE__));
// 	//List_Print(original_files_list, &File_Print);
// 	//DEBUG_OUT(("%s %d: updated files after first pass:", __func__ , __LINE__));
// 	//List_Print(updated_files_list, &File_Print);
// 	
// 	the_updated_list_item = *(updated_files_list);
// 
// 	while (the_updated_list_item != NULL)
// 	{
// 		WB2KFileObject* 	the_updated_list_file = (WB2KFileObject*)(the_updated_list_item->payload_);
// 		bool				file_added;
// 	
// 		//DEBUG_OUT(("%s %d: updated file '%s' had no equivalent; adding to original folder", __func__ , __LINE__, the_updated_list_file->file_path_));
// 
// 		// Add this file to the list of files
// 		file_added = Folder_AddNewFile(original_root_folder, the_updated_list_file);
// 		
// 		// remove this file from the updated folder object so we can destroy the folder safely later
// 		the_item_to_be_deleted = the_updated_list_item;
// 		the_updated_list_item = the_updated_list_item->next_item_;
// 		List_RemoveItem(updated_files_list, the_item_to_be_deleted); // do after getting next item in list
// 		LOG_ALLOC(("%s %d:	__FREE__	the_item_to_be_deleted	%p	size	%i", __func__ , __LINE__, the_item_to_be_deleted, sizeof(WB2KList)));
// 		free(the_item_to_be_deleted);
// 		the_item_to_be_deleted = NULL;
// 		
// 		changes_made = true;
// 	}
// 
// 	// DEBUG
// 	//DEBUG_OUT(("%s %d: orig files after 2nd pass:", __func__ , __LINE__));
// 	//List_Print(original_files_list, &File_Print);
// 	//DEBUG_OUT(("%s %d: updated files after 2nd pass:", __func__ , __LINE__));
// 	//List_Print(updated_files_list, &File_Print);
// 
// 	return changes_made;
// }


// counts the bytes in the passed file/folder, and adds them to folder.selected_bytes_
bool Folder_CountBytes(WB2KFolderObject* the_folder, WB2KList* the_item, WB2KFolderObject* not_needed)
{
	WB2KFileObject*		the_file;

	if (the_folder == NULL)
	{
		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	the_file = (WB2KFileObject*)the_item->payload_;
	
// NOTE Jan 14, 2023: need to look into this. probably needs total redesign. unsure it's even needed though. maybe for FAT32 on f256.
// 	FileMover_AddToSelectedCount(App_GetFileMover(global_app), the_file->size_);

	//DEBUG_OUT(("%s %d: counted file '%s' in '%s'; selected bytes now = %lu", __func__ , __LINE__, the_file->file_name_,  the_folder->folder_file_->file_name_, FileMover_GetSelectedByteCount()));
	
	return true;
}


// // processes, with recursion where necessary, the contents of a folder, using the passed function pointer to process individual files/empty folders.
// // returns -1 in event of error, or count of files affected
// int Folder_ProcessContents(WB2KFolderObject* the_folder, WB2KFolderObject* the_target_folder, uint8_t the_scope, bool do_folder_before_children, bool (* action_function)(WB2KFolderObject*, WB2KList*, WB2KFolderObject*))
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list
// 	//   for each file that matches the passed scope:
// 	//     if the "file" is a folder, then recurse by calling this function again in order to process the children of that folder
// 	//     if the file is a file, then call the action_function passed
// 	//   note: scope is only allowed to be set at the first call, not in recursions. all recursion calls from this function will be passed LIST_SCOPE_ALL
// 
// 	int			num_files = 0;
// 	int			result;
// 	WB2KList*	the_item;
// 	WB2KList*	next_item;
// 	uint8_t		the_error_code;
// 	
// 	// sanity checks
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed source folder was NULL", __func__ , __LINE__));
// 		return -1;
// 	}
// 
// 	the_item = *(the_folder->list_);
// 
// 	if (the_item == NULL)
// 	{
// 		return false;
// 	}
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file;
// 		
// 		next_item = the_item->next_item_; // capture this early, because if the action function is a delete, we may be removing this list item very shortly
// 
// 		this_file = (WB2KFileObject*)(the_item->payload_);
// 		//DEBUG_OUT(("%s %d: looking at file '%s'", __func__ , __LINE__, this_file->file_name_));
// 
// 		if (the_scope == LIST_SCOPE_ALL || (the_scope == LIST_SCOPE_SELECTED && File_IsSelected(this_file)) || (the_scope == LIST_SCOPE_NOT_SELECTED && !File_IsSelected(this_file)))
// 		{
// 			// if this is a folder, get a folder object for it, and recurse if not empty
// 			if (this_file->is_directory_)
// 			{
// 				WB2KFolderObject*	the_sub_folder;
// 
// 				// LOGIC:
// 				//   For a folder, there are 2 options in this function
// 				//   1. Process the folder file with the action before processing it's children OR
// 				//   2. Process the folder file with the action AFTER processing the children
// 				//   Typically, a copy type operation will need the folder to get actioned first (so that target folder structure gets created)
// 				//   A delete action would need the inverse: delete the children, then the folder. 
// 				//   For some other actions, it won't matter (eg, count bytes)
// 				
// 				if (do_folder_before_children)
// 				{
// 					//DEBUG_OUT(("%s %d: Executing helper function on folder file '%s' before processing children", __func__ , __LINE__, this_file->file_name_));
// 					
// 					if ((*action_function)(the_folder, the_item, the_target_folder) == false)
// 					{
// 						DEBUG_OUT(("%s %d: Error executing helper function on folder file '%s'", __func__ , __LINE__, this_file->file_name_));
// 						goto error;
// 					}
// 				}
// 				
// 				if ( (the_sub_folder = Folder_New(this_file, USE_COPY_OF_FOLDER_FILE) ) == NULL)
// 				{
// 					// couldn't get a folder object. probably should be returning some kind of error condition. TODO
// 					LOG_ERR(("%s %d:  couldn't get a folder object for '%s'", __func__ , __LINE__, this_file->file_name_));
// 					goto error;
// 				}
// 				else
// 				{
// 					// LOGIC: if folder is empty, we can process it as a file. otherwise, recurse
// 					// 2021/06/03: this will never be true, because Folder_New doesn't call populate! all HasChildren does is look at file_count_ as 0 or not 0. 
// 					
// 					// have root folder populate its list of files
// 					if ( (the_error_code = Folder_PopulateFiles(the_sub_folder)) > ERROR_NO_ERROR)
// 					{
// 						LOG_ERR(("%s %d: folder '%s' reported that file population failed with error %u", __func__ , __LINE__, the_sub_folder->folder_file_->file_name_, the_error_code));
// 						goto error;
// 					}
// 
// 					
// 					if (Folder_HasChildren(the_sub_folder))
// 					{
// 						DEBUG_OUT(("%s %d: folder '%s' has 1 or more children", __func__ , __LINE__, the_sub_folder->folder_file_->file_name_));
// 						result = Folder_ProcessContents(the_sub_folder, the_target_folder, LIST_SCOPE_ALL, do_folder_before_children, action_function);
// 
// 						if (result == -1)
// 						{
// 							// error condition
// 							LOG_ERR(("%s %d: Folder_ProcessContents failed on folder '%s'", __func__ , __LINE__, the_sub_folder->folder_file_->file_name_));
// 							goto error;
// 						}
// 
// 						num_files += result;
// 					}
// 					else
// 					{
// 						DEBUG_OUT(("%s %d: folder '%s' has no children", __func__ , __LINE__, this_file->file_name_));
// 					}
// 
// 					if (do_folder_before_children == false)
// 					{
// 						//DEBUG_OUT(("%s %d: Executing helper function on folder file '%s' after processing children", __func__ , __LINE__, this_file->file_name_));
// 					
// 						if ((*action_function)(the_folder, the_item, the_target_folder) == false)
// 						{
// 							DEBUG_OUT(("%s %d: Error executing helper function on folder file '%s'", __func__ , __LINE__, this_file->file_name_));
// 							goto error;
// 						}
// 					}
// 
// 					++num_files;
// 				}
// 
// 				Folder_Destroy(&the_sub_folder);
// 			}
// 			else
// 			{
// 				// call the action function
// 				if ((*action_function)(the_folder, the_item, the_target_folder) == false)
// 				{
// 					goto error;
// 				}
// 
// 				++num_files;
// 			}
// 		}
// 
// 		the_item = next_item;
// 	}
// 
// 	return num_files;
// 
// error:
// 	return -1;
// }


// move every currently selected file into the specified folder. Use when you DO have a folder object to work with
// returns -1 in event of error, or count of files moved
// //   NOTE: calling function must have already checked that folders are on the same device!
// int Folder_MoveSelectedFiles(WB2KFolderObject* the_folder, WB2KFolderObject* the_target_folder)
// {
// 	// LOGIC:
// 	//   iterate through all files in the source folder's list
// 	//   for any file that is listed as selected, move it (via rename) to the specified target folder
// 
// 	int				num_files = 0;
// 	WB2KList*		the_item;
// 	WB2KList*		temp_item;
// 
// 	if (the_folder == NULL || the_target_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: the source and/or target folder was NULL", __func__ , __LINE__));
// 		goto error;
// 	}
// 
// 	the_item = *(the_folder->list_);
// 
// 	if (the_item == NULL)
// 	{
// 		return -1;
// 	}
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
// 
// 		if (File_IsSelected(this_file) == true)
// 		{
// 			WB2KFileObject*		same_named_file_in_target;
// 			char				target_file_path_buffer[FILE_MAX_PATHNAME_SIZE];
// 			char*				target_file_path = target_file_path_buffer;
// 
// 
// 			// check that files are not being moved/copied into a sub-folder
// 			
// 			// LOGIC:
// 			//   If source folder path is found, in its entirety, in the target path, we must block the copy/move
// 			//   We only do this check with the target file is a directory
// 			//   We have to ensure the source path ends with : or /, or it can find a partial name. (eg, RAM:folder would show as bad match for RAM:folder1/some folder)
// 			//   We have to compare with whatever the shortest of the 2 paths is
// 			//     case A: "RAM:AcmeTest/" > "RAM:AcmeTest/lvl1/" (into a subfolder)
// 			//     case B: "RAM:AcmeTest/" > "RAM:AcmeTest/" (into itself)
// 			
// 			if (this_file->is_directory_)
// 			{
// 				int32_t			src_file_path_len;
// 				int32_t			tgt_folder_path_len;
// 				int32_t			shortest_path;
// 				char			temp_source_path[FILE_MAX_PATHNAME_SIZE];
// 				char*			source_path_for_compare = temp_source_path;
// 
// 				// prep source file path for comparison
// 			
// 				src_file_path_len = strlen((char*)this_file->file_path_);
// 			
// 				General_Strlcpy(source_path_for_compare, this_file->file_path_, FILE_MAX_PATHNAME_SIZE);
// 
// 				if ( source_path_for_compare[src_file_path_len - 1] != ':')
// 				{
// 					General_Strlcat(source_path_for_compare, (char*)"/", FILE_MAX_PATHNAME_SIZE); // TODO: replace hard-coded
// 					src_file_path_len++;
// 				}
// 			
// 				// compare prepped source folder path to target folder path
// 
// 				tgt_folder_path_len = strlen((char*)the_target_folder->folder_file_->file_path_);
// 				shortest_path = src_file_path_len < tgt_folder_path_len ? src_file_path_len : tgt_folder_path_len;
// 			
// 				if (General_Strncasecmp(source_path_for_compare, the_target_folder->folder_file_->file_path_, shortest_path) == 0)
// 				{
// 					// TODO: implement this once I add back the text-based-dialog window functions
// // 					General_ShowAlert(App_GetBackdropSurface(global_app)->window_, MSG_StatusMovingFiles, ALERT_DIALOG_SHOW_AS_ERROR, ALERT_DIALOG_NO_CANCEL_BTN, (char*)MSG_StatusMovingFilesIntoChildError);
// 					DEBUG_OUT(("%s %d: **NOT safe to move folder** (%s > %s)", __func__ , __LINE__, this_file->file_path_, the_target_folder->folder_file_->file_path_));
// 					return -1;	
// 				}
// 				else
// 				{
// 					//DEBUG_OUT(("%s %d: (safe to move folder) (%s > %s) (%lu, %lu)", __func__ , __LINE__, this_file->file_path_, the_target_folder->folder_file_->file_path_, tgt_folder_path_len, src_file_path_len));
// 				}
// 			}
// 
// 
// 			// compare new path to existing paths in target dir to see if (same-named) file already exists there
// 
// 			General_CreateFilePathFromFolderAndFile(target_file_path, the_target_folder->folder_file_->file_path_, this_file->file_name_);
// 			same_named_file_in_target = Folder_FindFileByFilePath(the_target_folder, target_file_path, strlen((char*)target_file_path));
// 
// 			if (same_named_file_in_target != NULL)
// 			{
// 				// do what, exactly? warn user one file at a time? With a dialogue box? fail/stop? ignore this file, and continue with the rest? 
// 				DEBUG_OUT(("%s %d: File '%s' already exists in the destination folder. This file will not be moved.", __func__ , __LINE__, this_file->file_name_));
// 			
// 				the_item = the_item->next_item_;
// 			}
// 			else
// 			{
// 				bool		result_doesnt_matter;
// 				
// 				if ( File_Rename(this_file, this_file->file_name_, target_file_path) == false)
// 				{
// 					LOG_ERR(("%s %d: Move action failed with file '%s' -> '%s'", __func__ , __LINE__, this_file->file_path_, target_file_path));
// 					goto error;
// 				}
// 
// 				// mark the file as not selected in its new location
// 				File_SetSelected(this_file, false);
// 
// 				// add a copy of the file to any open panels match the target folder (but aren't it), then add the original to this target folder
// 				//DEBUG_OUT(("%s %d: Adding file '%s' to any matching open windows/panels...", __func__ , __LINE__, this_file->file_name_));
// 				// NOTE Jan 14, 2023: think about having something to handle when same disk is open in both panels. TODO
// // 				result_doesnt_matter = App_ModifyOpenFolders(global_app, the_target_folder, this_file, &Folder_AddNewFileAsCopy);
// 				result_doesnt_matter = Folder_AddNewFile(the_target_folder, this_file);
// 				
// 				++num_files;
// 				
// 				// remove file from any open panels match the source folder (but aren't it), then remove from this source folder
// 				//DEBUG_OUT(("%s %d: Removing file '%s' from any matching open windows/panels...", __func__ , __LINE__, this_file->file_name_));
// 				// NOTE Jan 14, 2023: think about having something to handle when same disk is open in both panels. TODO
// // 				result_doesnt_matter = App_ModifyOpenFolders(global_app, the_folder, this_file, &Folder_RemoveFile);
// 				temp_item = the_item->next_item_;
// 				Folder_RemoveFileListItem(the_folder, the_item, DO_NOT_DESTROY_FILE_OBJECT);
// 				the_item = temp_item;
// 			}
// 		}
// 		else
// 		{
// 			the_item = the_item->next_item_;
// 		}
// 	}
// 
// 	return num_files;
// 	
// error:
// 	return -1;
// }


// // move every currently selected file into the specified folder file. Use when you only have a target folder file, not a full folder object to work with.
// // returns -1 in event of error, or count of files moved
// int Folder_MoveSelectedFilesToFolderFile(WB2KFolderObject* the_folder, WB2KFileObject* the_target_folder_file)
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list
// 	//   for any file that is listed as selected, move it (via rename) to the specified target folder
// 
// 	int				num_files = 0;
// 	char	target_file_path_buffer[FILE_MAX_PATHNAME_SIZE];
// 	char*	target_file_path = target_file_path_buffer;
// 	WB2KList*		the_item;
// 	WB2KList*		temp_item;
// 
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed class object was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
// 
// 	if (the_target_folder_file == NULL)
// 	{
// 		LOG_ERR(("%s %d: the target folder file was NULL", __func__ , __LINE__));
// 		goto error;
// 	}
// 
// 	the_item = *(the_folder->list_);
// 
// 	if (the_item == NULL)
// 	{
// 		return false;
// 	}
// 
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
// 
// 		if (File_IsSelected(this_file) == true)
// 		{
// 			// move file
// 			General_CreateFilePathFromFolderAndFile(target_file_path, the_target_folder_file->file_path_, this_file->file_name_);
// 
// 			if ( rename( this_file->file_path_, target_file_path ) == 0)
// 			{
// 				LOG_ERR(("%s %d: Move action failed with file '%s'", __func__ , __LINE__, this_file->file_name_));
// 				goto error;
// 			}
// 
// 			// mark the file as not selected (in its new location, it wouldn't be selected()
// 			// no point in doing this, as we are going to destroy this file object shortly anyway
// 			//File_SetSelected(this_file, false);
// 
// 			++num_files;
// 
// 			// remove file from the parent panel's list of files
// 			--the_folder->file_count_;
// 			temp_item = the_item->next_item_;
// 			File_Destroy(&this_file);
// 			List_RemoveItem(the_folder->list_, the_item);
// 			LOG_ALLOC(("%s %d:	__FREE__	the_item	%p	size	%i", __func__ , __LINE__, the_item, sizeof(WB2KList)));
// 			free(the_item);
// 			the_item = temp_item;
// 		}
// 		else
// 		{
// 			the_item = the_item->next_item_;
// 		}
// 	}
// 
// 	return num_files;
// 	
// error:
// 	return -1;
// }


// select or unselect 1 file by row id, and change cur_row_ accordingly
bool Folder_SetFileSelectionByRow(WB2KFolderObject* the_folder, uint16_t the_row, bool do_selection, uint8_t y_offset)
{
	WB2KFileObject*		the_file;
	WB2KFileObject*		the_prev_selected_file;

	the_file = Folder_FindFileByRow(the_folder, the_row);
	
	if (the_file == NULL)
	{
		return false;
	}


	if (do_selection)
	{
		// is this already the currently selected file? do we need to unselect a different one? (only 1 allowed at a time)	
		if (the_folder->cur_row_ == the_row)
		{
			// we re-selected the current file. 
		}
		else
		{
			// something else was selected. find it, and mark it unselected. 
			the_prev_selected_file = Folder_FindFileByRow(the_folder, the_folder->cur_row_);
		
			if (the_prev_selected_file == NULL)
			{
			}
			else
			{
				if (File_MarkUnSelected(the_prev_selected_file, y_offset) == false)
				{
					// the passed file was null. do anything?
					LOG_ERR(("%s %d: couldn't mark file '%s' as selected", __func__ , __LINE__, the_prev_selected_file->file_name_));
					App_Exit(ERROR_FILE_MARK_UNSELECTED_FILE_WAS_NULL);
				}
			}
		}

		the_folder->cur_row_ = the_row;

		if (File_MarkSelected(the_file, y_offset) == false)
		{
			// the passed file was null. do anything?
			LOG_ERR(("%s %d: couldn't mark file '%s' as selected", __func__ , __LINE__, the_file->file_name_));
			App_Exit(ERROR_FILE_MARK_SELECTED_FILE_WAS_NULL);
		}
	}
	else
	{
		if (the_folder->cur_row_ == the_row)
		{
			// we unselected the current file. set current selection to none.
			the_folder->cur_row_ = -1;
		}

		if (File_MarkUnSelected(the_file, y_offset) == false)
		{
			// the passed file was null. do anything?
			LOG_ERR(("%s %d: couldn't mark file '%s' as selected", __func__ , __LINE__, the_file->file_name_));
			App_Exit(ERROR_FILE_MARK_UNSELECTED_FILE_WAS_NULL);
		}
	}

	return true;
}	


// toss out the old folder, start over and renew
void Folder_RefreshListing(WB2KFolderObject* the_folder)
{
	Folder_DestroyAllFiles(the_folder);
	return;
}





// TEMPORARY DEBUG FUNCTIONS

// helper function called by List class's print function: prints folder total bytes, and calls print on each file
void Folder_Print(void* the_payload)
{
	WB2KFolderObject*		the_folder = (WB2KFolderObject*)(the_payload);

	DEBUG_OUT(("+----------------------------------+-+------------+----------+--------+"));
	DEBUG_OUT(("|File                              |S|Size (bytes)|Date      |Time    |"));
	DEBUG_OUT(("+----------------------------------+-+------------+----------+--------+"));
	List_Print(the_folder->list_, &File_Print);
	DEBUG_OUT(("+----------------------------------+-+------------+----------+--------+"));
	DEBUG_OUT(("Total bytes %lu", the_folder->total_bytes_));
}
