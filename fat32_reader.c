/***********************************************************
 * Name of program:
 * Authors:  Benjamin Mankowitz 		Ari Roffe
 * Description: FIXME: Need a descr
 **********************************************************/

/* These are the included libraries.  You may need to add more. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <ctype.h>

/* Put any symbolic constants (defines) here */
#define True 1  /* C has no booleans! */
#define False 0
#define L_ENDIAN 0 /* NOTE that fat32 is always little endian */
#define B_ENDIAN 1 /* The local OS may be big endian */

#define MAX_CMD 80
#define MAX_DIR 16

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_DEVICE_FILE 0x40

//image file being made global, represented by the descriptor supplied when first access in init
FILE *fd;

/* Register Functions and Global Variables */
uint32_t convertToLocalEndain(uint32_t original);
uint32_t convertToFAT32Endian(uint32_t original);
void init(char* argv);
void print_convert_to_Hex(int n);
	/*strDummy variable for the compiler */
	char* strDummy = "";
	int intDummy = 0;
	size_t sizeTDummy = 5;
	/*End of strDummy stuff */

	/***********************************************************
 * HELPER FUNCTIONS
 * Use these for all IO operations
 **********************************************************/
int localEndian = -1;

/*pointers for the fread() function*/
int BPB_BytesPerSec;
int BPB_SecPerClus;
int BPB_RsvdSecCnt;
int BPB_NumFATS;
int BPB_FATSz32;
int BPB_RootCluster;
int first_sector_of_cluster;
int bytes_for_reserved;
int fat_bytes;
char  * dir_name;
int present_dir;//not always going to be first dir, meaning once we call 'cd' this will be the present directory

	/**
 * Making the Directory struct 
 * This is technically also a file
 * */
	struct directory{
	char DIR_Name[11];//11 bytes
	uint8_t DIR_Attr; //1 byte
	char padding[8];//8 bytes of padding
	uint16_t DIR_FstClusHi;//2 bytes
	char more_padding[4];//4 bytes padding
	uint16_t DIR_FstClusLo;//2 bytes
	uint32_t DIR_FileSize;//4 bytes
	//in total, 32 bytes

	//to ease calculations, I am attaching the following to each "dir"
	//actually ignoring this for now. unsure if it's wise to play with this struct
	//uint32_t firstSecOfClus = ((DIR_- 2) * BPB_SecPerClus) + bytes_for_reserved + fat_bytes;
};

struct directory dir[MAX_DIR];
//^this will represent the possible directories, 10 is arbitrary...possibly more or less, not sure now
struct directory rootDir;//the root directory, set during init

	/*
 * endian functions
 * ----------------------------
 *   determineLocalEndian is called during init()
 * 	 Both of the converting functions take a 32-bit word and convert 
 */

void determineLocalEndian() {
	int i = 1;
    char *p = (char *)&i;

    if (p[0] == 1)
        localEndian = L_ENDIAN;
    else
        localEndian = B_ENDIAN;
}

uint32_t convertToLocalEndian(uint32_t original){
	if(localEndian == L_ENDIAN){
		//there is nothing to do
		return original;
	}
	else{
		//To swap endian:
		uint32_t b0,b1,b2,b3;
		uint32_t returnValue;

		b0 = (original & 0x000000ff) << 24u;
		b1 = (original & 0x0000ff00) << 8u;
		b2 = (original & 0x00ff0000) >> 8u;
		b3 = (original & 0xff000000) >> 24u;

		returnValue = b0 | b1 | b2 | b3;
		return returnValue;
	}
}
uint32_t convertToFAT32Endian(uint32_t original){
	if(localEndian == L_ENDIAN){
		//there is nothing to do
		return original;
	}
	else{
		//convert from big endian to little endian
		//To swap endian:
				uint32_t b0,b1,b2,b3;
				uint32_t returnValue;

				b0 = (original & 0x000000ff) << 24u;
				b1 = (original & 0x0000ff00) << 8u;
				b2 = (original & 0x00ff0000) >> 8u;
				b3 = (original & 0xff000000) >> 24u;

				returnValue = b0 | b1 | b2 | b3;
				return returnValue;
	}
}

/*
 * init
 * ----------------------------
 *   open the file image and set it up
 */
void init(char* argv){
	/* Determine whether our machine is big endian or little endian */
	determineLocalEndian();

	/* Parse args and open our image file */
	fd = fopen(argv, "r"); //https://www.tutorialspoint.com/c_standard_library/c_function_fopen.htm
	printf("%s Opened\n", argv);
	//fd is our pointer to the file, as a reminder
	if(fd == NULL){
		printf("File not exist\n");
		return;
	}

	/* Parse boot sector and get information, move to here*/
	fseek(fd, 0xB, SEEK_SET); //SEEK_SET is the beginning of File, skip to position 11 as per Wiki
	//Super helpful, https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
	sizeTDummy = fread(&BPB_BytesPerSec, 2, 1, fd); //one element that is 2 bytes, 2 Hex's on the HexEdit tool
	sizeTDummy = fread(&BPB_SecPerClus, 1, 1, fd);  //starts at 0xD
	sizeTDummy = fread(&BPB_RsvdSecCnt, 2, 1, fd);// number of reserved sectors, aka number of sectors before "data" sectors
	sizeTDummy = fread(&BPB_NumFATS, 1, 1, fd); //"next line"

	//BPB_FATSz32 offset is 0x2C from the SEEK_SET, Kelly's slides/hints for project
	fseek(fd, 0x24, SEEK_SET);
	sizeTDummy = fread(&BPB_FATSz32, 4, 1, fd);

	/*Get root directory address */
	fseek(fd, 0x2C, SEEK_SET);
	sizeTDummy = fread(&BPB_RootCluster, 4, 1, fd);
	present_dir = BPB_RootCluster;//start at the root cluster and call commands from here

	//moved declaration to global
	bytes_for_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	fat_bytes = BPB_BytesPerSec * BPB_FATSz32 * BPB_NumFATS; //multiply how many FATS by amount of fat sectors and bytes per each sector
	first_sector_of_cluster = ((present_dir - 2) * BPB_SecPerClus) + bytes_for_reserved + fat_bytes;
	fseek(fd, first_sector_of_cluster, SEEK_SET);//at the first sector of data
	//reference the first dir, dir[0] for the root directory
	sizeTDummy = fread(&dir[0], 32, 16, fd);//shorthand for 512 bytes 32*16=512, read into the first dir struct, ie root, everything offset from here
	//printf("Root addr is 0x%x\n", root_addr);
	//set rootDir:
	rootDir = dir[0];
	return;
}

/*
 * freeAll
 * ----------------------------
 *   free all allocated memory
 */ 	 

	void freeAll() {
		//TODO
	}

/*
 * convertToShortName
 * ----------------------------
 *   Converts the given string into the appropriate short name
 * 	 BUG: This is not (yet) equipped to handle filenames
 * 	 that need to be truncated. EX:  reallylongname.txt -> REALLY~0.txt
 */

	char* convertToShortName(char* input) {
		//printf("\nInside of convertToShortName");
		char* firstPart = malloc(sizeof(char) * 9);//+1 for the null terminator
		char* extension = malloc(sizeof(char) * 4);
		int i = 0;//position in input
		int j = 0;//position in outpur
		while(i < 8){
			if(iscntrl(input[i]) || input[i] == '.'){
				//the end of the string is reached before 8 characters
				while(j<8){
					firstPart[j++] = ' ';
				}
				break;
			}
			else{
				firstPart[i++] = toupper(input[j++]);
			}

		}
		firstPart[8] = '\0';//make sure to null terminate string
		j = 0; //reset the counter;
		while(j < 3){
			i++;//i++ here to skip the '.' if it exists
			if(isalnum((unsigned char)input[i])){//the character is alphanumeric
				extension[j++] = toupper(input[i]);
			}
			else{
				extension[j++] = ' ';
			}

		}
		extension[3] = '\0';//null terminated
		strcat(firstPart, extension);
				
		//printf("\nexiting convertToShortName");

		return firstPart;
		


	}

/***********************************************************
 * CMD FUNCTIONS
 * Implementation of all command line arguments
 **********************************************************/

/*
 * info
 * ----------------------------
 *   Description: prints out information about the following fields in both hex and base 10: 
 *		BPB_BytesPerSec, Bytes per Sector
 *		BPB_SecPerClus, amount of sectors in a cluster
 *	 	BPB_RsvdSecCnt, # of reserved sector
 *	 	BPB_NumFATS, Number of File Allocation Tables
 *	 	BPB_FATSz32, Sectors per Fat
 */
void info(){
	//Now print out all the info that was recorded in the init() method when we started the program
	char buff[20];
	sprintf(buff, "%04x", BPB_BytesPerSec);
	printf("BPB_BytesPerSec is: 0x%s %d \n", buff, BPB_BytesPerSec);

	sprintf(buff, "%04x", BPB_SecPerClus);
	printf("BPB_SecPerClus: 0x%s %d\n", buff, BPB_SecPerClus);

	sprintf(buff, "%04x", BPB_RsvdSecCnt);
	printf("BPB_RsvdSecCnt: 0x%s %d\n", buff, BPB_RsvdSecCnt);

	sprintf(buff, "%04x", BPB_NumFATS);
	printf("BPB_NumFATS: 0x%s %d\n", buff, BPB_NumFATS);

	sprintf(buff, "%04x", BPB_FATSz32);
	printf("BPB_FATSz32: 0x%s %d\n", buff, BPB_FATSz32);
}

/*
 * ls
 * ----------------------------
 *	Description: lists the contents of DIR_NAME, including “.” and “..”.
 *	path: the path to examine  
 */
void ls(char* path){
	//calculate the place in file, called change
	int bytes_in_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	int fat_sector_bytes = BPB_FATSz32 * BPB_NumFATS * BPB_BytesPerSec;//see the setting up method, init for same procedure
	int change = ((present_dir - 2) * BPB_BytesPerSec) + bytes_in_reserved + fat_sector_bytes;

	//printf("Inside ls");//used for debugging purposes
	//skip to "change" in the file but don't read yet
	fseek(fd, change, SEEK_SET);

	//TODO: go thru each directory steming from start_dir
	//this is the dir we begin the program in and it changes based on the cd command
	//some sort of loop goes here to "pick up" possible files that are child files (directories) pf start_dir
	//printf(".\t..\t");
	for(int i = 0; i < 16; i++){
		sizeTDummy = fread(&dir[i], 32, 1, fd);//one item, a single dir, each 32 bytes
		//See the chart in the beginning of the source code for clarification on what gets printed
		//TODO: the ATTR attributes are a mask, not a value
		if ((dir[i].DIR_Name[0] != (char)0xe5) && (dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr == ATTR_DIRECTORY || dir[i].DIR_Attr == ATTR_ARCHIVE || dir[i].DIR_Attr == ATTR_DEVICE_FILE)){
			printf("%s\t", dir[i].DIR_Name);//this seperates the directories by follow up tab
		}
	}
	printf("\n");//put the "/]"" on the next line once all the dirs are listed
}

/**
 * This method is used to clean up the dirname when passed to cd
 * In short, it is used to separate the dot (if it exists) in the extension
 * Then it comebines the two possible halfs
 * Regardless, it makes everything one case so the cd method can take uppercase, lowercase, or a mix
 */
char * clean_up_dir_name(char* dirname){
	int i;//avoid comp warning
	char *result;//store the string here
	char long_name[12];
	memset(long_name, ' ', 12);//empty string to place the dir into later
	char *has_dot = strtok(dirname, "."); //resource for the strtok method, deubugging: https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm
	//if the dirname has a period, ie when we get the tokens the return is !NULL
	if (has_dot){
		strncpy(long_name, has_dot, strlen(has_dot));//transfer the file up until the dot
		has_dot = strtok(NULL, ".");//continue tokenizing the string used before, see https://stackoverflow.com/questions/23456374/why-do-we-use-null-in-strtok
		if (has_dot){
			//insert the extension in the longer dir name
			strncpy((char *) long_name + 8, has_dot, strlen(has_dot));//doesn't compile because of this line, find another way to go to position 8 in the char*
		}
		long_name[11] = '\0';//end of string marker
		for (i = 0; i < 11; i++){
			long_name[i] = toupper(long_name[i]);//make everything uppercase SINCE EVERYTHING IN THE FAT32 IS UPPERCASE...not yelling here
		}
	} else{
		//here code runs if we want to cd into a dir without an extension like DIR in our example
		strncpy(long_name, dirname, strlen(dirname));
		long_name[11] = '\0';//end of string marker
	}
	result = long_name;
	return result;
}

	/**
 * cd command 
*/
	void
	change_directory(char *would_like_to_cd_into){
	//skip this step of cleaning up the dir if we are trying to dir "up"
	if(strncmp(would_like_to_cd_into, "..", 2) != 0){
		would_like_to_cd_into = clean_up_dir_name(would_like_to_cd_into);
	}
	int cluster_hit = -1; //this indicates the dir is not found
	int change_to_cluster;
	/*TODO: Check here to see if we should "go up" a dir, if the would_like_to_cd_into is ".."
	reference the cluster_hi cluster_lo bytes to see how to "move up" in a file directory*/
	if (strncmp(would_like_to_cd_into, "..", 2) == 0){
		for (int i = 0; i < 16; i++){
			//printf("Entering the loop");
			int bool = strncmp(dir[i].DIR_Name, "..", 2);
			if (bool == 0){
				change_to_cluster = ((dir[i].DIR_FstClusLo - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);//see below for it fleshed out
				present_dir = dir[i].DIR_FstClusLo;
				printf("Seeking to the above directory");
				fseek(fd, change_to_cluster, SEEK_SET);
				sizeTDummy = fread(&dir[0], 32, 16, fd);//read from beginning of where the seek went to, not dir[i]
				return; //exit the method
			}
		}
	}
	//when we aren't "moving up" do the following
	//first examine if the dir we want to cd into exists
	for (int i = 0; i < 16; i++){
		char *directory = malloc(11);
		memset(directory, '\0', 11);
		memcpy(directory, dir[i].DIR_Name, 11);
		//compare variable 'directory' with what we'd like to cd into, namely the parameter provided in the method
		if (strncmp(directory, would_like_to_cd_into, 11) == 0){
			cluster_hit = dir[i].DIR_FstClusLo; //see page 25 for more info
			break;
		}
	}
	if (cluster_hit == -1){
		printf("Directory not found");
		return;
	}
	//to what are we changing to? first see if its a name (ie not '..')
	//refer to the ls command for similar structure from here
	int reserved_byte_count = BPB_BytesPerSec * BPB_RsvdSecCnt;
	int bytes_in_fat = BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec;
	change_to_cluster = ((cluster_hit - 2) * BPB_BytesPerSec) + reserved_byte_count + bytes_in_fat;

	//use this for debugging
	//printf("This is the cluster to change into: %d", change_to_cluster);

	present_dir = cluster_hit;
	fseek(fd, change_to_cluster, SEEK_SET);
	sizeTDummy = fread(&dir[0], 32, 16, fd); //this now replaces the "first" dir, not always root, really represents the present_dir hence the switch to lines above
}

	/*
* filestat
* ----------------------------
*	Description: prints the size of the file or directory name, the attributes of the
*	file or directory name, and the first cluster number of the file or directory name
*	if it is in the present working directory.  Return an error if FILE_NAME/DIR_NAME
*	does not exist. (Note: The size of a directory will always be zero.) 
*
*	path: the path to examine. Determine if this is a file or directory and print accordingly
*/
void filestat(char *path){
	for(int i = 0; i < MAX_DIR; i++){
		if(!strncmp(path, dir[i].DIR_Name, 11) /* TODO: using strncmp bc DIR_Name has a trailing space */){
			//found match!
			printf("Size is %d\n", dir[i].DIR_FileSize);

			if(!!(ATTR_READ_ONLY & dir[i].DIR_Attr))
				printf("Attributes ATTR_READ_ONLY\n");

			if(!!(ATTR_HIDDEN & dir[i].DIR_Attr)){
				printf("Attributes ATTR_HIDDEN\n");
				printf("If you see this, there is an error in filestat not hiding hidden files\n");
			}
			if(!!(ATTR_SYSTEM & dir[i].DIR_Attr)){
				printf("Attributes ATTR_SYSTEM\n");
			}
			if(!!(ATTR_VOLUME_ID & dir[i].DIR_Attr)){
				printf("Attributes ATTR_VOLUME_ID\n");
			}
			if(!!(ATTR_DIRECTORY & dir[i].DIR_Attr)){
				printf("Attributes ATTR_DIRECTORY\n");
			}
			if(!!(ATTR_ARCHIVE & dir[i].DIR_Attr)){
				printf("Attributes ATTR_ARCHIVE\n");
			}
			//TODO: verify this is what we should print
			printf("First cluster number is %x%x\n", dir[i].DIR_FstClusLo,dir[i].DIR_FstClusHi);
			return;
		}
	}
	//File not found
	printf("Error: file/directory does not exist\n");
	return;
}


/*
* size
* ----------------------------
*	Description: prints the size of file FILE_NAME in the present working directory.
*	Log an error if FILE_NAME does not exist.
*
*	path: the path to examine. Determine if this is a file or directory and print accordingly
*	return: returns the size if file exists, otherwise 0
*/
int size(char* path, int shouldPrint){
	for(int i = 0; i < MAX_DIR; i++){
		if(!strncmp(path, dir[i].DIR_Name, 11) /* TODO: show hidden files? */){
			if(dir[i].DIR_Attr & ATTR_DIRECTORY){
				if(shouldPrint) printf("This is a folder");
				return 0;
			}
			if(shouldPrint) printf("Size is %d", dir[i].DIR_FileSize);
			return dir[i].DIR_FileSize;
		}
	}
	//if we got here, there were no matches
	if(shouldPrint) printf("Error: file not found");
	return 0;
}

/*
* read
* ----------------------------
*	Description: reads from a file named FILE_NAME, starting at POSITION, and prints NUM_BYTES.
*	Return an error when trying to read an unopened file. ?! TODO: what is an opened file
*
*	path: the path to examine. Determine if this is a file or directory and if it exists
*/
//TODO: can we just use the system read call? 
void fileread(char* file, int startPos, int numBytes){
	for(int i = 0; i < MAX_DIR; i++){
		if(!strncmp(dir[i].DIR_Name, file, 11)){
			//check if directory
			if(!!(dir[i].DIR_Attr & ATTR_DIRECTORY)){
				printf("ERROR: attempt to read directory");
				return;
			}

			//we have a match!
			//ensure what we want to read is within the length:
			int fsize = size(file, False);
			if(numBytes >= fsize){
				printf("ERROR: Attempted to read outside of file bounds");
				return;
			}

			//find the position of the file to read:
			uint32_t position;
			char buf[fsize+1/*null terminator*/];
			uint32_t high = dir[i].DIR_FstClusHi << 4;
			uint16_t low = dir[i].DIR_FstClusLo;
			position = low + high;
			printf("found starting position %x at file %s",position, dir[i].DIR_Name);
			int firstSecOfClus = ((position - 2) * BPB_SecPerClus) + bytes_for_reserved + fat_bytes + BPB_RootCluster;
			fseek(fd, firstSecOfClus, SEEK_SET);
			//TODO: this works for consecutive data, but fix for nonconsecutive data
			sizeTDummy = fread(buf, sizeof(char), fsize, fd);
			buf[fsize] = '\0';
			printf("%s", buf);
			return;
		}
	}
	printf("ERROR: Unable to find file");
}

void volume(){
	if(rootDir.DIR_Name == NULL){
		printf("ERROR: Volume name not found");
		return;
	}
	rootDir.DIR_Name[11] = '\0';
	printf("%s",rootDir.DIR_Name);
	return;
}

/***********************************************************
 * MAIN
 * Program main
 **********************************************************/
int main(int argc, char *argv[])
{
	char cmd_line[MAX_CMD];


	//printf("This is the file: %s", argv[1]);
	init(argv[1]); /*for Ari to work on */

	/* Main loop.  You probably want to create a helper function
       for each command besides quit. */

	while(True) {
		bzero(cmd_line, MAX_CMD);
		//printf("\n/%s] ", dir[1].DIR_Name);//better readability when typing commands
		printf("\n/] ");//better readability when typing commands
		strDummy = fgets(cmd_line,MAX_CMD,stdin);

		/* Start comparing input */
		if(strncmp(cmd_line,"info",4)==0) {
			printf("Going to display info.\n");
			info();
		}

		else if(strncmp(cmd_line, "stat",4)==0){
			printf("Going to stat!\n");
			//cmd_line[strlen(&cmd_line[0])-1] = '\0';//remove trailing newline
			filestat(convertToShortName(&cmd_line[5]));
		}

		else if(strncmp(cmd_line, "test",4)==0){//REMOVE THIS BEFORE WE FINISH
			//TODO: remove this
			printf("%x",convertToFAT32Endian(0x1415));
			printf("%x",convertToLocalEndian(0x1415));
			//printf("\n the result is %d", 0x10 & 0x11);
			//printf("The converted string is: %s", convertToShortName(&cmd_line[5]));
		}
		
		else if(strncmp(cmd_line,"ls",2)==0) {
			printf("Going to ls.\n");
			ls(&cmd_line[3]);
		}
		
		else if(strncmp(cmd_line,"read",4)==0) {
			printf("Going to read!\n");
			char* file = convertToShortName(strtok(&cmd_line[5], " "));
			int startingPosition = atoi(strtok(NULL, " "));
			int numBytes = atoi(strtok(NULL, " "));
			fileread(file, startingPosition, numBytes);
		}
		
		else if(strncmp(cmd_line,"size",4)==0) {
			printf("Going to size!\n");
			size(convertToShortName(&cmd_line[5]), True);
		}

		else if(strncmp(cmd_line,"volume",4)==0) {
			printf("Going to size!\n");
			volume();
		}

		else if(strncmp(cmd_line,"cd",2)==0) {
			printf("Going to cd into %s\n", &cmd_line[3]);//fix this line
			change_directory(&cmd_line[3]);
		}
		else if(strncmp(cmd_line,"mkdir",4)==0) {
			printf("Going to mkdir!\n");
			//TODO
		}
		else if(strncmp(cmd_line,"rmdir",4)==0) {
			printf("Going to rmdir!\n");
			//TODO
		}
		
		else if(strncmp(cmd_line,"quit",4)==0) {
			printf("Quitting.\n");
			break;
		}
		else
			printf("Unrecognized command.\n");


	}

	/* Close the file */

	return 0; /* Success */
}

