1. The source code is present in the file “scheduler.c” in the folder named “source”.
2. The compilation of the source code can be done by using the makefile in the same folder.
3. To build, we need to go to the folder “<path_to_jha_assign1>/source” and type make. 
4. The executable will be made in the folder “<path_to_jha_assign1>/bin”, which can be run by going to the folder and typing “./Scheduler”.
5. The “<path_to_jha_assign1>/test” folder contains the sample jobs used for testing, the corresponding trace files generated and their screenshots when viewed in kernelshark.
6. The path to the device file corresponding to the mouse may be different than the one used in the code. if the file is not “/dev/input/event3”, the code should be modified to open the correct file. Specifically, the definition of “MOUSEFILE1”, defined in the beginning of the code needs to be changed. 
