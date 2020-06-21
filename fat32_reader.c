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
#define DEBUG 1

#define MAX_CMD 80
#define MAX_DIR 255
#define MAX_FAT 8192

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_DEVICE_FILE 0x40

#define EOC 0x0FFFFFF8
#define FREE 0x00000000
#define BAD_CLUSTER 0x0FFFFFF7

//image file being made global, represented by the descriptor supplied when first access in init
FILE *fd;

/* Register Functions and Global Variables */
uint32_t convertToLocalEndain(uint32_t original);
uint32_t convertToFAT32Endian(uint32_t original);
void refreshDir(uint32_t cluster);
void init(char* argv);

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
//uint32_t fat[MAX_FAT];
uint32_t *fat;
uint32_t clusterSize;

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
struct directory stagingDir[MAX_DIR];
struct directory rootDir;//the root directory, set during init, accessed by volume (and maybe 1 more)

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

void init(char* argv){
	/* Determine whether our machine is big endian or little endian */
	determineLocalEndian();

	/* Parse args and open our image file */
	fd = fopen(argv, "r"); //https://www.tutorialspoint.com/c_standard_library/c_function_fopen.htm
	printf("%s Opened\n", argv);
	//fd is our pointer to the file, as a reminder
	if(fd == NULL){
		printf("File does not exist\n");
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
	bytes_for_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	fat_bytes = BPB_BytesPerSec * BPB_FATSz32 * BPB_NumFATS; //multiply how many FATS by amount of fat sectors and bytes per each sector
	first_sector_of_cluster = ((BPB_RootCluster - 2) * BPB_SecPerClus) + bytes_for_reserved + fat_bytes;
	clusterSize = BPB_BytesPerSec * BPB_SecPerClus;

	//set rootDir:
	rootDir = dir[0];
	//set FAT table:
	fat = malloc(fat_bytes);
	fseek(fd, bytes_for_reserved, SEEK_SET);
	sizeTDummy = fread(fat, 4 /*bytes*/, fat_bytes/4, fd);

	//populate dir array
	refreshDir(BPB_RootCluster);
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
 *   Converts the given string into the appropriate short (internal) name
 * 	 BUG: This is not (yet) equipped to handle filenames
 * 	 that need to be truncated. EX:  reallylongname.txt -> REALLY~0.txt
 *   BUG: If called with no input, will segfault
 */

	char* convertToShortName(char* input) {
		//printf("\nInside of convertToShortName");
		if(input == NULL){
			return input;//return original string for '.' or '..', and do nothing for null
		}
		else if(input[0] == '.' && input[1] != '.') return ".          ";
		else if(input[0] == '.' && input[1] == '.' && input[2] != '.') return "..         ";
		char* baseName = calloc(sizeof(char), 12);//+1 for the null terminator
		char* extension = calloc(sizeof(char), 4);
		int i = 0;//position in input
		int j = 0;//position in output
		while(i < 8){
			if(iscntrl(input[i]) || input[i] == '.'){
				//the end of the string is reached before 8 characters
				while(j<8){
					baseName[j++] = ' ';
				}
				break;
			}
			else{
				baseName[i++] = toupper(input[j++]);
			}

		}
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
		strcat(baseName, extension);
		free(extension);
		//printf("\nexiting convertToShortName");

		return baseName; //and the extension added by strcat
	}

	/*
 * convertToPrettyName
 * ----------------------------
 *   Converts the given string into the appropriate external name
 * 	 EX: CONST   TXT -> const.txt
 * 	 BUG: If called with no input, will segfault
 * 	 that need to be truncated. EX:  reallylongname.txt -> REALLY~0.txt
 */

	char* convertToPrettyName(char* input) {
		//printf("\nInside of convertToShortName");
		char* name = calloc(sizeof(char), 12);//+1 for the null terminator
		int namePos = 0;
		int inputPos = 0;
		for(;namePos < 11;namePos++){
			if(input[inputPos] != ' ' && inputPos != 8){
				name[namePos]=tolower(input[inputPos++]);
			}
			else if(inputPos == 8){
				if(input[9] == ' '){
					inputPos=11;
					break;
				}
				name[namePos]='.';
				name[++namePos] = tolower(input[inputPos++]);
			}
			else {//white space
				inputPos++;
				namePos--;
			}
		}
		return name;
	}	

	int getCluster(struct directory thisDir){
		int cluster = (thisDir.DIR_FstClusLo) +(thisDir.DIR_FstClusHi << 16);
		if(cluster == 0) return 2;
		else{
			return (thisDir.DIR_FstClusLo) + (thisDir.DIR_FstClusHi << 16);
		}
	}
	uint32_t getOffset(struct directory thisDir){
		//((N - 2) * BPB_SecPerClus*BPB_BytesPerSec) + fat_bytes+bytes_for_reserved;
		//((getCluster(dir[i]) - 2) * BPB_BytesPerSec*BPB_SecPerClus) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);//see below for it fleshed out
		return ((getCluster(thisDir) - 2) * BPB_SecPerClus*BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);
	}
	uint32_t getOffsetInt(uint32_t currentCluster){
		//returns THIS cluster's offset
		if(currentCluster < 0) currentCluster = 2;
		return ((currentCluster - 2) * BPB_SecPerClus*BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);
	}

	uint32_t getNextClusterOffset(struct directory currentDir){
		//use this to get the next page address whenver accessing multiple clusters of memory
		//ex: large files or folders with getNextCluster subdirectories
		uint32_t cluster = getCluster(currentDir);
		if(cluster == EOC || cluster == FREE) return -1;
		return getOffsetInt(fat[cluster]);
	}
	uint32_t getNextClusterOffsetInt(int currentCluster){
		return getOffsetInt(fat[currentCluster]);
	}
	uint32_t getNextCluster(int currentCluster){
		if (currentCluster == 438){
			//nothing
		}
		return fat[currentCluster];
	}

	void refreshDir(uint32_t cluster){
		//zero out previous data:
		memset(dir,0,sizeof(dir));
		memset(stagingDir,0,sizeof(stagingDir));
		//add elements to stagingDir array:
		int dirsToSkip = 0;
		int dirsPerCluster = clusterSize/32;//32=size of dir entry
		while(cluster != FREE && cluster < EOC && cluster != BAD_CLUSTER){
			fseek(fd, getOffsetInt(cluster), SEEK_SET);//at the first sector of data
			sizeTDummy = fread(&stagingDir[dirsToSkip], 32, dirsPerCluster, fd);//shorthand for 512 bytes 32*16=512, read into the first dir struct, ie root, everything offset from here
			if(DEBUG) printf("Dec: %d\t Hex: %x\t EOC: %d\n",cluster, cluster, cluster==EOC);
			cluster = getNextCluster(cluster);
			dirsToSkip += dirsPerCluster;
		}
		
	
		//clean remove entries that do not exist in this directory by assuming
		//that a blank space means the end of the directory:

		for(int i = 0; i < MAX_DIR; i++){
			//determine if dir exists:
			//TODO: unclear if strncmp is working correctly
			if(strncmp(stagingDir[i].DIR_Name, "",11) == 0 &&
				stagingDir[i].DIR_Attr == 0 &&
				stagingDir[i].DIR_FileSize == 0 &&
				stagingDir[i].DIR_FstClusLo == 0 &&
				stagingDir[i].DIR_FstClusHi == 0) break;
			else if(stagingDir[i].DIR_Name[0]=='_') continue;//ignore mac artifacts
			dir[i]= stagingDir[i];
		}
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
 *  TODO: use the path parameter
 */
void ls(char* path){
	//calculate the place in file, called change
	//int bytes_in_reserved = BPB_BytesPerSec * BPB_RsvdSecCnt;
	//int fat_sector_bytes = BPB_FATSz32 * BPB_NumFATS * BPB_BytesPerSec;//see the setting up method, init for same procedure
	//int change = ((present_dir - 2) * BPB_BytesPerSec) + bytes_in_reserved + fat_sector_bytes;

	if(DEBUG) printf("Cluster Number: %d\tOffset: %d\n", getCluster(dir[0]), getOffset(dir[0]));
	for(int i = 0; i < MAX_DIR; i++){
		if((dir[i].DIR_Name[0] != (char)0xe5 || dir[i].DIR_Name[0] != ' ')&& !(dir[i].DIR_Attr & ATTR_HIDDEN
			|| dir[i].DIR_Attr & ATTR_SYSTEM || dir[i].DIR_Attr & ATTR_VOLUME_ID)){
				//snprintf(stdout, 12, "%s\t",dir[i].DIR_Name);
				char temp[12];
				strncpy(temp, convertToPrettyName(dir[i].DIR_Name), 11);
				if(!isalnum(temp[10])) temp[10] = '\0';//remove the question marks
				temp[11] = '\0';
				printf("%s\t",temp);//this seperates the directories by follow up tab
		}
	}
}
	/**
 * cd command 
*/
void cd(char *newDir){
	/*TODO: Check here to see if we should "go up" a dir, if the newDir is ".."
	reference the cluster_hi cluster_lo bytes to see how to "move up" in a file directory.
	We need this one to compare the first char*/
	
	//when we aren't "moving up" check if newDir exists
	for (int i = 0; i < MAX_DIR; i++){
		if (strncmp(dir[i].DIR_Name, newDir, 11) == 0){
			//we have a match. make sure it is a folder not file:
			if(!(dir[i].DIR_Attr & ATTR_DIRECTORY)){
				printf("Error: Tried to cd into a directory, but found file instead");
				return;
			}
			refreshDir(getCluster(dir[i]));
			if(DEBUG) printf("Going to offset: %d\n",getOffset(dir[i]));
			return;
		}
	}
	//no results:
	printf("Error: unable to find '%s'",newDir);	

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
		if(!strncmp(path, dir[i].DIR_Name, 11)/* using strncmp bc DIR_Name has a trailing space */){
			//found match!
			printf("Size is %d\n", dir[i].DIR_FileSize);

			if(!!(ATTR_READ_ONLY & dir[i].DIR_Attr))
				printf("Attribute: ATTR_READ_ONLY\n");

			if(!!(ATTR_HIDDEN & dir[i].DIR_Attr)){
				printf("Attribute: ATTR_HIDDEN\n");
			}
			if(!!(ATTR_SYSTEM & dir[i].DIR_Attr)){
				printf("Attribute: ATTR_SYSTEM\n");
			}
			if(!!(ATTR_VOLUME_ID & dir[i].DIR_Attr)){
				printf("Attribute: ATTR_VOLUME_ID\n");
			}
			if(!!(ATTR_DIRECTORY & dir[i].DIR_Attr)){
				printf("Attribute: ATTR_DIRECTORY\n");
			}
			if(!!(ATTR_ARCHIVE & dir[i].DIR_Attr)){
				printf("Attribute: ATTR_ARCHIVE\n");
			}
			//TODO: verify this is what we should print
			printf("First cluster number is 0x%x\n", getCluster(dir[i]));
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
*	shouldPrint: Whether we should print details to the console or just return the size
*	return: returns the size if file exists, otherwise 0

	shouldPrint: whether or not to print the size to the console
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
	return -1;
}

/*
* read
* ----------------------------
*	Description: reads from a file named FILE_NAME, starting at POSITION, and prints NUM_BYTES.
*	Return an error when trying to read an unopened file.
*
*	file: the file to examine. Determine if this is a file or directory and if it exists
*/
void fileread(char* file, int startPos, int numBytes){
	for(int i = 0; i < MAX_DIR; i++){
		if(!strncmp(dir[i].DIR_Name, file, 11)){
			//Name match!

			//check if directory
			if((dir[i].DIR_Attr & ATTR_DIRECTORY) || (dir[i].DIR_Attr & ATTR_VOLUME_ID)){
				//TODO: this does not work
				printf("ERROR: attempt to read directory");
				return;
			}

			//ensure what we want to read is within the length:
			int fsize = size(file, False);
			if(numBytes + startPos >= fsize || startPos < 0){
				printf("ERROR: Attempted to read outside of file bounds");
				return;
			}

			//find the position of the file to read:
			// uint32_t N;//starting cluster
			char buf[numBytes+1/*null terminator*/];
			//New plan: read in the entire cluster that has any relevant byte, then only print out
			//the parts that we need. Something like printf(buf[startingPosition]
			//While there is probably a cleaner way, this is what I thought of and is easy to implement
			int bytesToSkip = 0;//buffer offset
			int bytesPerCluster = clusterSize;
			int bytesToRead = clusterSize;//how many bytes to read for this iteration
			int bufInitialOffset = startPos % clusterSize;
			int totalRemainingBytes = numBytes; //the bytes left to read for the entire file
			uint32_t cluster = getCluster(dir[i]);
			while(cluster != FREE && cluster != BAD_CLUSTER && (totalRemainingBytes > 0)){
				if(totalRemainingBytes >= clusterSize){ bytesToRead = clusterSize;}
				else{ bytesToRead = totalRemainingBytes;}
				totalRemainingBytes -= clusterSize;
				fseek(fd, getOffsetInt(cluster), SEEK_SET);//at the first sector of data
				sizeTDummy = fread(&buf[bytesToSkip], sizeof(char), bytesToRead, fd);//shorthand for 512 bytes 32*16=512, read into the first dir struct, ie root, everything offset from here
				if(DEBUG) printf("\nDec: %d\t Hex: %x\t EOC: %d\n",cluster, cluster, cluster==EOC);

				//now that we finished, check if EOC and increment values:
				if(cluster >= EOC){
					break;
				}
				cluster = getNextCluster(cluster);
				bytesToSkip += bytesPerCluster;
			}

			buf[numBytes] = '\0';
			printf("\n%s", buf + bufInitialOffset);
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

void filemkdir(char* file){
	//read the fileread and cd commands to figure out how to get the location/offset.
	//Once we have the offset, we need to add a new dir entry into the dir table (optional)
	//and write it back to the underlying file.
}

void filermdir(char* file){
	//either set the flags to deleted and write back or manually set to 0s and remove FAT entry
	//this one should be easier than mkdir.

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
			printf("Going to volume!\n");
			volume();
		}

		else if(strncmp(cmd_line,"cd",2)==0) {
			printf("Going to cd into %s\n", &cmd_line[3]);//fix this line
			cd(convertToShortName(&cmd_line[3]));
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

