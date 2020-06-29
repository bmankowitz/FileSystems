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
This was my first project working remotely, so communication could have been better. Despite the fact that we had git set up and each worked on a single issue, there was still some duplicated work. For example, there were two methods to convert a filename into the fat32 short name. Additionally, we didn't define a set style so we have a mix of camelCase and snake_case. Overall this was a learning experience and I hope to be able to collaborate more effectively in the future.

## Additional Sources
Outside sources were used. Any sources besides the textbook/slides are noted in the comments section

## Known Bugs
The program is currently unable to handle filenames longer than 11 characters.

When creating new files, it is possible to enter an invalid file name. That file will be created, but will not be deletable.

Not all allocated memory is freed.

There was an issue where mkdir would occasionally overwrite other data in the fat32 image. I can not reproduce the issue, so it may be resolved, but in case it's not user beware.
