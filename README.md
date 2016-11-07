# Scoville: The escaping overlay file system

Scoville is a [FUSE] overlay file system which works around the limited
character set allowed in FAT file names.  FAT does not permit non-printable
characters in file names, nor does it allow the characters ‘*’, ‘?’, ‘<’, ‘>’,
‘|’, ‘"’, ‘:’, or ‘\\’.  Many applications support applying a file name
translation map when placing files on FAT-formatted volumes; however, some –
notably [git-annex](https://git-annex.branchable.com/) – do not.  For those
applications, Scoville may be the only way to work around some very unpleasant
ad-hoc schemes.

Usage is trivial: Run Scoville on an existing mounted file system.  For example,
if you have a FAT-formatted SD card mounted on /media/sd, you can run `scoville
/media/sd`.  When you’re done, unmount it with `fusermount -u`.  While mounted,
Scoville will translate all forbidden characters to their URL-escaped versions.
For example, a file called ‘What Else Is There?.flac’ will be stored on disk as
‘What Else Is There%3f.flac’.

Beyond escaping, Scoville is exactly as capable as the file system it overlays.
If you want long file names, use vfat; if you want POSIX permissions, use
umsdos.  (On the other hand, if you use umsdos, you don’t need to use Scoville,
since umsdos provides Scoville’s special character handling, albeit using a
different scheme.)

[FUSE]: https://github.com/libfuse/libfuse

## License

Scoville is licensed under the [Apache License, version 2.0][Apache-2].

[Apache-2]: https://www.apache.org/licenses/LICENSE-2.0
