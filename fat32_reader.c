/***********************************************************
 * Name of program:
 * Authors:  Benjamin Mankowitz 		Ari Roffe
 * Description: FIXME: Need a descr
 **********************************************************/

/* These are the included libraries.  You may need to add more. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>

/* Put any symbolic constants (defines) here */
#define True 1  /* C has no booleans! */
#define False 0
#define L_ENDIAN 0 /* NOTE that fat32 is always little endian */
#define B_ENDIAN 1 /* The local OS may be big endian */

#define MAX_CMD 80

//image file being made global, represented by the descriptor supplied when first access in init
FILE *fd;

	/* Register Functions and Global Variables */
	char *
	convertToLocalEndain(char *original);
char* convertToFAT32Endian(char* original);
void init(char* argv);
void print_convert_to_Hex(int n);

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
int starting_dir;

	/**
 * Making the Directory struct 
 * */
	struct directory{
	char DIR_Name[11];
	uint8_t DIR_Attr; //one byte
	uint16_t DIR_FstClusHi;
	uint16_t DIR_FstClusLo;
	uint32_t DIR_FileSize;
};

struct directory dir[10];
//^this will represent the possible directories, 10 is arbitrary...possibly more or less, not sure now

	/*
 * endian functions
 * ----------------------------
 *   determineLocalEndian is called during init()
 * 	 Both of the converting functions take a 32-bit word and convert 
 */

	void
	determineLocalEndian()
{
	int i = 1;
    char ∗p = (char ∗)&i;

    if (p[0] == 1)
        localEndian = L_ENDIAN;
    else
        localEndian = B_ENDIAN;
}

char* convertToLocalEndian(char* original){
	if(localEndian == L_ENDIAN){
		//there is nothing to do
		return original;
	}
	else{
		//To convert from little endian to big endian
		return htonl(original);
	}
}
char* convertToFAT32Endian(char* original){
	if(localEndian == L_ENDIAN){
		//there is nothing to do
		return original;
	}
	else{
		//convert from big endian to little endian
		return ntohl(original);
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
	fd = fopen(&argv[1], "r"); //https://www.tutorialspoint.com/c_standard_library/c_function_fopen.htm

	/* Parse boot sector and get information, move to here*/
	fseek(fd, 0xB, SEEK_SET); //SEEK_SET is the beginning of File, skip to position 0xB as per Wiki
	//Super helpful, https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
	size_t read1 = fread(&BPB_BytesPerSec, 2, 1, fd); //one element that is 2 bytes, 2 Hex's on the HexEdit tool
	size_t read2 = fread(&BPB_SecPerClus, 1, 1, fd);  //starts at 0xD
	size_t read3 = fread(&BPB_RsvdSecCnt, 2, 1, fd);// number of reserved sectors, aka number of sectors before "data" sectors
	size_t read4 = fread(&BPB_NumFATS, 1, 1, fd); //"next line"

	//BPB_FATSz32 offset is 0x2C from the SEEK_SET, Kelly's slides/hints for project
	fseek(fd, 0x24, SEEK_SET);
	size_t read5 = fread(&BPB_FATSz32, 4, 1, fd);

	/*Get root directory address */
	fseek(fp, 0x2C, SEEK_SET);
	fread(&BPB_RootCluster, 4, 1, fp);
	starting_dir = BPB_RootCluster;//start at the root cluster and call commands from here

	int bytes_for_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	int fat_bytes = BPB_BytesPerSec * BPB_FATSz32 * BPB_NumFATS; //multiply how many FATS by amount of fat sectors and bytes per each sector
	first_sector_of_cluster = ((starting_dir - 2) * BPB_SecPerClus) + bytes_for_reserved + fat_bytes;
	fseek(fp, first_sector_of_cluster, SEEK_SET);//at the first sector of data
	//reference the first dir, dir[0] for the root directory
	fread(&dir[0], 32, 16, fp);//shorthand for 512 bytes 32*16=512, read into the first dir struct, ie root, everything offset from here
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
	printf("Bytes per Sector: %d", BPB_BytesPerSec);
	print_convert_to_Hex(BPB_BytesPerSec);

	printf("Sector per Cluster: %d", BPB_SecPerClus);
	print_convert_to_Hex(BPB_SecPerClus);

	printf("Count of Reserves Logical Sectors: %d", BPB_RsvdSecCnt);
	print_convert_to_Hex(BPB_RsvdSecCnt);

	printf("Number of File Allocation Tables: %d", BPB_NumFATS);
	print_convert_to_Hex(BPB_NumFATS);

	printf("Number of Sectors per FAT: %d", BPB_FATSz32);
	print_convert_to_Hex(BPB_FATSz32);
	return ;
}

// function to convert decimal to hexadecimal
void print_convert_to_Hex(int n) {
	char hexaDeciNum[100];

	int i = 0;
	while (n != 0) {
		int temp = 0;
		temp = n % 16;//get remainder
		if (temp < 10){
			hexaDeciNum[i] = temp + 48;
			i++;
		}
		else{
			hexaDeciNum[i] = temp + 55;
			i++;
		}
		n = n / 16;
	}

	// for litte-endian hex
	for (int j = i - 1; j >= 0; j--){
		printf("Hex: %c", hexaDeciNum[j]);//print out the hex, skip line
	}
	printf("\n");
	
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
	int change = (starting_dir - 2 * BPB_BytesPerSec) + bytes_in_reserved + fat_sector_bytes;

	//skip to "change" in the file but don't read yet
	fseek(fd, change, SEEK_SET);

	//TODO: go thru each directory steming from start_dir
	//this is the dir we begin the program in and it changes based on the cd command
	//some sort of loop goes here to "pick up" possible files that are child files (directories) pf start_dir
	for(int i = 0; i < 10; i++){
		fread(&dir[i], 32, 1, fd);//one item, 32 bits
		/* for reference on the dir_attr meanings, 
		see piazza and notes for which we should include in our display of ls, I put a -> next to them for reference later
			->ATTR_READ_ONLY   	0x01
			ATTR_HIDDEN 		0x02
			ATTR_SYSTEM 		0x04
			ATTR_VOLUME_ID		0x08
			->ATTR_DIRECTORY	0x10
			->ATTR_ARCHIVE  	0x20
		*/
		if((dir[i].DIR_Name[0] != (char)0xe5) && (dir[i].DIR_Attr == 0x1 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)){
			//copy the characters into a new char to display it without tampering the img file WHATSOEVER 
			char *something_to_be_listed = malloc(11);
			memset(something_to_be_listed, '\0', 11);//make empty string
			//copy from DIR_Name...I think this is short...confirmation needed
			memcpy(something_to_be_listed, dir[i].DIR_Name, 11);//this is taka the copying
			printf("%s\t", something_to_be_listed);//this seperates the directories by follow up tab
		}
	}
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
		void filestat(char *path)
		{

			//TODO: IMPLEMENT ME
			return;
}


/***********************************************************
 * MAIN
 * Program main
 **********************************************************/
int main(int argc, char *argv[])
{
	char cmd_line[MAX_CMD];

	/*Dummy variable for the compiler */
	char* dummy = "";
	printf("%s", dummy);
	/*End of dummy stuff */



	init(*argv); /*for Ari to work on */



	/* Main loop.  You probably want to create a helper function
       for each command besides quit. */

	while(True) {
		bzero(cmd_line, MAX_CMD);
		printf("/]");
		dummy = fgets(cmd_line,MAX_CMD,stdin);

		/* Start comparing input */
		if(strncmp(cmd_line,"info",4)==0) {
			printf("Going to display info.\n");
			info();
		}

		else if(strncmp(cmd_line, "stat",4)==0){
			printf("Going to stat!\n");
			filestat(NULL /* TODO: implement substring(6,80) */);
		}
		
		else if(strncmp(cmd_line,"ls",2)==0) {
			printf("Going to ls.\n");
			ls(NULL /* TODO: implement substring(6,80) */);
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

