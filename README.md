Ameol Source Code
-----------------

This is the source code for the Ameol CIX offline reader. It is being
released under the Apache license. See the LICENSE file for more details.

The code requires Microsoft Visual Studio 2008  to build. There are issues
with building under newer versions of Visual Studio due to changes in the
CRT causing conflict with some pre-built binaries. Thus only VS2008 is
officially supported.

Only the main Ameol executable and support DLLs are issued, plus the
associated data files and installer. The source code for the Moderate
addon is in the Moderate project on github.


To Build
--------
First install the Inno Setup program from the Tools folder.

    isetup-5.5.4.exe

This is required to build the installer. Version 5.5.4 is the current
supported version. You only need to do this once.

Open a "Visual Studio 2008 Command Prompt" window. This will be in your
Start menu under Microsoft Visual Studio 2008. Alternatively you can find
it at:

    %PROGRAMFILES(x86)%\Microsoft Visual Studio 9.0\VC\vcvarsall.bat

Navigate to the root of the folder where you cloned the Ameol source code
and run:

    build release

All built binaries are copied to the Build folder off the root along with
any program debug database files.

At the end of the setup build, the packages will be copied to the Drops
folder. There are two packages - an integrated installer (the filename
begins with 'a') and a ZIP file (the filename begins with 'z').

There should be NO warnings or errors. All projects have the warnings as
error flag set anyway.

The resulting installer is NOT code signed by default. If you have access
to the code signing certificate then set two environment variables before
running the build:

set _PFXPATH=<path to the code signing PFX file>
set _AM2SIGNPWD=<the PFX password>

If the above are not set the the build skips code signing. You can run the
resulting packages without code signing but you'll need to bypass Windows
warnings. Do not release unsigned packages.

To Run
------
You can run Ameol by copying the binaries from the Build folder to your
Ameol installation, replacing the existing binaries there.

To Debug
--------
You will need to set up your own debug environment. You will need to copy
files from your Ameol setup to the Build folder so that the Build folder
is the root of your Ameol installation. A very *minimal* way to get Ameol
to run is to copy the files from the External folder into the Build folder.

Documentation
-------------
The source code is the only real documentation. Feel free to ask questions
in the cix.developer/ameol topic though but CIX staff cannot guarantee much beyond
basic support.

Contributions
-------------
You are welcome to contribute changes back to the official Ameol source
tree. All changes will be reviewed before being accepted and it is assumed
that you've done your own, exhaustive, testing. Feel free to make private
binaries available for testing by other people.

Official Release
----------------
The open source code IS the main Ameol source now. All official builds
will use this, and any changes made by CIX staff will go into the open
source tree by default. We will make official builds available when we deem
that there are sufficient changes to warrant an upgrade.
