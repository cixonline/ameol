Ameol Source Code

This is the source code for the Ameol CIX offline reader. It is being
released under the Apache license. See the LICENSE file for more details.

The code requires Microsoft Visual Studio 2008 or greater to build. The
solutions file provided is for Visual Studio 2008 which is the minimum
supported version.

Only the main Ameol executable and support DLLs are issued. At this point
we are not releasing the source code for the installer. You will need a
full Ameol installation for the remaining data files.


To Build
--------
Open ameol.sln in Visual Studio 2008 and do a full rebuild.

All built binaries are copied to the Build folder off the root along with
any program debug database files.

There should be NO warnings or errors. All projects have the warnings as
error flag set anyway.

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
