OS/2 Metafile plugin
by Vladimir Kondratiev
mailto:vladkond@sgg.ru

Installation:

copy npmeta.dll to your netscape\plugins directory.

Usage:

1) full screen - now you can view .met files.
For example, you can rename OS/2 spool file
(OS/2 print files are actually metafiles) to *.met and view.

2) Embedded
use <EMBED> tag as
<EMBED type="image/metafile" width=400 height=300 src="graphic.met"> </EMBED>
If you use type= you can use any name for scr=. If you not use type=
you have to use .met files in src= because there is no way to determine
mime type.

Description:

This plugin display and print OS/2 metafiles. Original ratio not
preserved, metafile scaled to your dimensions.

Problems:

Sometime while resizing navigator window, plugin is not drawn correctly.
It seems to be Netscape bug because I so the same with bitmap images.
To work around - overlap navigator window with some other and then
activate navigator window again.

While printing, there are some erroneous data in print file. I really
don't know what is wrong. If somebody could help, you are welcome.

Warranties:

No.

License:

Feel free to use binaries as well as source code.

Contents:

makefile         make for VAC++
npmeta.cpp       C++ source
npmeta.def       definition
npmeta.dll
npmeta.rc        recource
PPRINTF.CPP      pipe printf - for debug
PPRINTF.H
PPR_SRV.CPP      pipe printf server
read.me          this file

