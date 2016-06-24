# CH-SS16-Project
Racing game with the Hapkit as input device with haptic feedback.

The game StuntCarRacer, we used to implement the haptic controller, was originally
released at 1989 for the Amiga and some other platforms.

* Ported to C/C++ by Daniel Vernon and Andrew Copland
* Available under BSD license at https://sourceforge.net/projects/stuntcarremake

* The Hapkit is open hardware. All resources are available here: http://hapkit.stanford.edu/

Remark: The goal of this project is the inclusion of the haptic game controller, 
so if you want to develop the game and you don't need the haptic paddle, 
then it is recommended to use the repository mentioned above, or alternatively
one of those:
* Cloned from SourceForge with some improvements and an experimental mod for 
higher frame rates. Available at https://github.com/fluffyfreak/stuntcarracer
* Another clone from SourceForge. It is a port to linux which is still under development. Available at https://github.com/ptitSeb/stuntcarremake

## Dependencies
The repository contains the project files for Eclipse. Unfortunately Eclipse 
is not required, but recommended.

The project is developed on a system with windows 7, on this platform you will
need the following development packages, which are to install in the stated order:

1) Microsoft Windows 7.1 SDK
https://www.microsoft.com/en-us/download/details.aspx?id=8279

2) Microsoft Windows Driver Kit 7.1
https://www.microsoft.com/en-us/download/details.aspx?id=11800

3) Microsoft Visual C++ 2010 SDK
https://www.microsoft.com/en-us/download/details.aspx?id=2680

4) Microsoft DirectX 10 SDK (Jun 2010)
https://www.microsoft.com/en-us/download/details.aspx?id=6812

5) Microsoft Visual C++ 2010 SDK SP1 Update
https://www.microsoft.com/en-us/download/details.aspx?id=4422

The points 3) and 5) are optional - it depends on the system setup weather 
they are required or not. Sometimes some of the packages are already installed,
then it may be necessary to uninstall them if the installation of a prior step 
fails and reinstall them afterwards.

## Remark to the License
We chose the BSD license, because the original code is licensed under the same terms.
