Helium for 3ds Max

Helium is a Schematic Framework that allows to create Flow-chart
User Interfaces within 3dsmax, via maxscript. Please see the help file
and example scripts installed by the installer for further instructions
on how to use helium.

Repo and project setup by Josef 'spacefrog' Wienerroither

Building:
The project setup supports building for 3ds Max 2017 up to most recent 3ds Max SDK installed, all from a single solution.
The solution relies on correctly set-up 3ds Max SDK environment variables.
The environment variables should be named accordingly and point to the correct SDK path
eg. for the 3ds Max 2025 SDK it should be named ADSK_3DSMAX_SDK_2025 and point to the SDK rood path

To build all versions, use the Visualstudio internal batch build functionality, or msbuild to build from commandline

Original code copyright, © Kees Rijnen

Special Thanks go to Larry Minton, Rejean Poirier, www.maxplugins.de, Bobo, plus all beta testers.