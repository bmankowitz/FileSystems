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
 * Use these for all IO operations TODO: and others?
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
};

struct directory dir[MAX_DIR];
//^this will represent the possible directories, 10 is arbitrary...possibly more or less, not sure now

	/*
 * endian functions
 * ----------------------------
 *   determineLocalEndian is called during init()
 * 	 Both of the converting functions take a 32-bit word and convert 
 */

	void
	determineLocalEndian() {
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
		//To convert from little endian to big endian
		return htonl((uint32_t) original);
	}
}
uint32_t convertToFAT32Endian(uint32_t original){
	if(localEndian == L_ENDIAN){
		//there is nothing to do
		return original;
	}
	else{
		//convert from big endian to little endian
		return ntohl((uint32_t) original);
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

	int bytes_for_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	int fat_bytes = BPB_BytesPerSec * BPB_FATSz32 * BPB_NumFATS; //multiply how many FATS by amount of fat sectors and bytes per each sector
	first_sector_of_cluster = ((present_dir - 2) * BPB_SecPerClus) + bytes_for_reserved + fat_bytes;
	fseek(fd, first_sector_of_cluster, SEEK_SET);//at the first sector of data
	//reference the first dir, dir[0] for the root directory
	sizeTDummy = fread(&dir[0], 32, 16, fd);//shorthand for 512 bytes 32*16=512, read into the first dir struct, ie root, everything offset from here
	//printf("Root addr is 0x%x\n", root_addr);
	return;
}

/***********************************************************
 * CMD FUNCTIONS
 * Implementation of all command line arguments
 **********************************************************/

/*
 * info
 * ----------------------------
 *   Description: prints out information about the following fields in both hex and base 10: 
 *		BPB_BytesPerSec == https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#BPB
 *		BPB_SecPerClus, sector/cluster == see the above wiki in the same table
 *	 	BPB_RsvdSecCnt, # of reserved sector,
 *	 	BPB_NumFATS, "Number of File Allocation Tables"
 *	 	BPB_FATSz32, Sectors per Fat

 *		USING THE HEXEDIT TOOL ON THE WEBSITE https://hexed.it/ AND PASTING THE fat32.img FILE
 */
void info(){
	//Now print out all the info
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
 *	
 *	path: the path to examine  
 */
void ls(char* path){
	int bytes_in_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	int fat_sector_bytes = BPB_FATSz32 * BPB_NumFATS * BPB_BytesPerSec;//see the setting up method, init for same procedure
	int change = ((present_dir - 2) * BPB_BytesPerSec) + bytes_in_reserved + fat_sector_bytes;

	//printf("Inside ls");//used for debugging purposes
	//skip to "change" in the file but don't read yet
	fseek(fd, change, SEEK_SET);

	//TODO: go thru each directory steming from start_dir
	//this is the dir we begin the program in and it changes based on the cd command
	//some sort of loop goes here to "pick up" possible files that are child files (directories) pf start_dir
	printf(".\t..\t");
	for(int i = 0; i < 16; i++){
		sizeTDummy = fread(&dir[i], 32, 1, fd);//one item, a single dir, each 32 bytes
		//See the chart in the beginning of the source code for clarification on what gets printed
		if ((dir[i].DIR_Name[0] != (char)0xe5) && (dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr == ATTR_DIRECTORY || dir[i].DIR_Attr == ATTR_ARCHIVE)){
			printf("%s\t", dir[i].DIR_Name);//this seperates the directories by follow up tab
		}
	}
	printf("\n");//put the "/]"" on the next line once all the dirs are listed
}

void change_directory(char *would_like_to_cd_into){
	int cluster_hit = -1;//this indicates the dir is not found
	int change_to_cluster;
	/*TODO: Check here to see if we should "go up" a dir, if the would_like_to_cd_into is ".."
	reference the cluster_hi cluster_lo bytes to see how to "move up" in a file directory*/
	if (strcmp(would_like_to_cd_into, "..") == 0)
	{
		for (int i = 0; i < 10; i++)
		{
			int comp = strncmp(dir[i].DIR_Name, "..", 2);
			if (!comp)
			{
				change_to_cluster = ((dir[i].DIR_FstClusLo - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);
				present_dir = dir[i].DIR_FstClusLo;
				fseek(fd, change_to_cluster, SEEK_SET);
				sizeTDummy = fread(&dir[0], 32, 16, fd);
				return; //stop here
			}
		}
	}
	//when we aren't "moving up" do the following
	//first examine if the dir we want to cd into exists
	for (int i = 0; i < 10; i++){
		char *directory = malloc(11);
		memset(directory, '\0', 11);
		memcpy(directory, dir[i].DIR_Name, 11);
		//compare variable 'directory' with what we'd like to cd into, namely the parameter provided in the method
		int bool = strncmp(directory, would_like_to_cd_into, 11);
		if(!bool){
			cluster_hit = dir[i].DIR_FstClusLo;//see page 25 for more info
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
	change_to_cluster = (cluster_hit - 2) * BPB_BytesPerSec + reserved_byte_count + bytes_in_fat;

	present_dir = change_to_cluster;
	fseek(fd, change_to_cluster, SEEK_SET);
	sizeTDummy = fread(&dir[0], 32, 16, fd);//this now replaces the "first" dir, not always root, really represents the present_dir hence the switch to lines above
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
		if(!strcmp(path, dir[i].DIR_Name) /* TODO: show hidden files? */){
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
		printf("/] ");//better readability when typing commands
		strDummy = fgets(cmd_line,MAX_CMD,stdin);

		/* Start comparing input */
		if(strncmp(cmd_line,"info",4)==0) {
			printf("Going to display info.\n");
			info();
		}

		else if(strncmp(cmd_line, "stat",4)==0){
			//TODO: remove trailing '\n' and decide on common FS.Attr_name format ex: "FSINFO   TXT " vs "FSINFO.TXT"
			printf("Going to stat!\n");
			filestat(&cmd_line[5]);
			//TODO: remove this
			printf("%x",convertToFAT32Endian(0x1415));
			printf("%x",convertToLocalEndian(0x1415));
		}

		else if(strncmp(cmd_line, "test",4)==0){//REMOVE THIS BEFORE WE FINISH
			//TODO: remove this
			printf("%x",convertToFAT32Endian(0x1415));
			printf("%x",convertToLocalEndian(0x1415));
			printf("\n the result is %d", 0x10 & 0x11);
		}
		
		else if(strncmp(cmd_line,"ls",2)==0) {
			printf("Going to ls.\n");
			ls(&cmd_line[3]);
		}

		else if(strncmp(cmd_line,"open",4)==0) {
			printf("Going to open!\n");
		}

		else if(strncmp(cmd_line,"close",5)==0) {
			printf("Going to close!\n");
		}
		
		else if(strncmp(cmd_line,"size",4)==0) {
			printf("Going to size!\n");
		}

		else if(strncmp(cmd_line,"cd",2)==0) {
			printf("Going to cd!\n");
			change_directory(&cmd_line[3]);
		}

		else if(strncmp(cmd_line,"read",4)==0) {
			printf("Going to read!\n");
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

