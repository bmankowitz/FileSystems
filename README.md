# Fat32 Reader 
The fat32 reader by Benjamin Mankowitz and Ari Roffe is a simple C program that reads in a given fat32 image file.

## Table of Contents
Requirements - The folder containing all the known requirements
Slides - The folder containing helpful slides from class
fat32.img - A sample fat32 image
fat32y.img - A second fat32 image

## Compiling the program
Just type in ```make``` to compile, and start by ```./fat32_reader <FILENAME>```. For example:
```bash
./fat32_reader fat32y.img
```

## Running the program

## Challenges

## Additional Sources

## Known Bugs
The program is currently unable to handle filenames longer than 11 characters

With certain file sizes, the ls command may include "junk" characters with the file names

Not all allocated memory is ```free```d

read might not work for nonconsecutive sectors.
