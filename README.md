# fat-gundel

## Graphics Undelete for FAT-Filesystems 

Fat-gundel restores deleted JPEG images from your digicam's storage chip.
It usually even succeeds after the chip was formatted.
(Most cameras do not really format the chip when you choose format.)

Fat-gundel runs on a block device without actually mounting the filesystem.
Fat-gundel reads the bootblock for basic device information, but ignores all 
FAT-tables and directory information. 

JPEG Images have a strict data format, thus our task is much easier than
finding deleted files in general. We search for JPEG headers and copy
the corresponding files to your computers harddisk.

If you frequently format your chips or remove all files regularly, fat-gundel 
has a good chance to restore *all* images later. Repeatedly adding and deleting 
individual images may cause fragmentation, which needs implementation.

Neverthelss, fat-gundel has -- compared to similar tools -- a tendency to err on 
the inclusive side. That is, it is very likely that you will see multiple copies 
or partially mixed or scrambled versions  of some possible image candidates, where
other tools would have discarded an invalid image. Also, it is almost certain 
that fat-gundel will bring back additional images, that have been deleted intentionally or 
had existed before the media was formatted.

Support for other filesystems is to be done. If ever needed -- 
SD-Cards and USB-Sticks are almost always FAT Filesystems.

It is a command line tool. No GUI.
See also:
 foremost.sf.net (the authors favorite these days),
 PhotoRec from http://www.cgsecurity.org/
 http://freecode.com/projects/magicrescue
 sf.net/projects/ocfa, Scalpel   
