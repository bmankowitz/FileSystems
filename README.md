# Fat32 Reader 
The fat32 reader by Benjamin Mankowitz and Ari Roffe is a simple C program that reads in a given fat32 image file.

## Table of Contents
Requirements - The folder containing all the known requirements
Slides - The folder containing helpful slides from class
fat32.img - A sample fat32 image
fat32y.img - A second fat32 image

## Compiling the program
Just type in ```make``` to compile

## Running the program
Start the program with ```./fat32_reader <FILENAME>```. For example:
```bash
./fat32_reader fat32y.img
```
## Challenges
This was my first project with far less communication that previously. Specifically, although we had git set up and assigned different issues to each person, there was still some duplicated work. For example, there are two methods to convert a filename into the short name standard. Additionally, the different coding styles occasoinally clash with different method naming styles and a mix of camelCase and snake_case. Overall this was a learning experience and I hope to be able to collaborate more effectively in the future.

This is all separate from the general issues(less time, more responsibilities at home, etc) arising from the Covid-19 pandemic and the crunch of finals.

## Additional Sources
Outside sources were used. Any sources besides the textbook/slides are noted in the comments section

## Known Bugs
The program is currently unable to handle filenames longer than 11 characters

With certain file sizes, the ls command may include "junk" characters with the file names

Not all allocated memory is freed

Read might not work for nonconsecutive sectors, and will segfault if called incorrectly.
