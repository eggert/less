/*
 * Copyright (c) 1984,1985,1989,1994  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Routines to mess around with filenames (and files).
 * Much of this is very OS dependent.
 */

#include "less.h"
#if MSOFTC
#include <dos.h>
#endif

extern int force_open;
extern IFILE curr_ifile;
extern IFILE old_ifile;

/*
 * Return the full pathname of the given file in the "home directory".
 */
	public char *
homefile(filename)
	char *filename;
{
	register char *pathname;
	register char *homedir;

	homedir = getenv("HOME");
#if MSOFTC
	/*
	 * If $HOME is not defined, look for the file anywhere on search path.
	 */
	pathname = NULL;
	if (homedir != NULL)
	{
		/*
		 * Found $HOME.
		 */
		pathname = (char *) calloc(strlen(homedir)+strlen(filename)+2, 
					sizeof(char));
		if (pathname == NULL)
			return (NULL);
		sprintf(pathname, "%s\\%s", homedir, filename);
		if (access(pathname, 0) < 0)
		{
			free(pathname);
			pathname = NULL;
		}
	}
	if (pathname == NULL)
	{
			
		pathname = (char *) calloc(_MAX_PATH, sizeof(char));
		_searchenv(filename, "PATH", pathname);
		if (*pathname == '\0')
			
		{
			free(pathname);
			pathname = NULL;
		}
	}
	
#else
	if (homedir == NULL)
		return (NULL);
	pathname = (char *) calloc(strlen(homedir)+strlen(filename)+2,
				sizeof(char));
	if (pathname == NULL)
		return (NULL);
	sprintf(pathname, "%s/%s", homedir, filename);
#endif
	return (pathname);
}

/*
 * Find out where the help file is.
 */
	public char *
find_helpfile()
{
	register char *helpfile;
	
	if ((helpfile = getenv("LESSHELP")) != NULL)
		return (save(helpfile));
#if MSOFTC
	return (homefile(HELPFILE));
#else
	return (save(HELPFILE));
#endif
}

/*
 * Expand a string, substituting any "%" with the current filename,
 * and any "#" with the previous filename.
 * {{ This is a lot of work just to support % and #. }}
 */
	public char *
fexpand(s)
	char *s;
{
	register char *fr, *to;
	register int n;
	register char *e;

	/*
	 * Make one pass to see how big a buffer we 
	 * need to allocate for the expanded string.
	 */
	n = 0;
	for (fr = s;  *fr != '\0';  fr++)
	{
		switch (*fr)
		{
		case '%':
			n += strlen(get_filename(curr_ifile));
			break;
		case '#':
			if (old_ifile == NULL_IFILE)
			{
				error("No previous file", NULL_PARG);
				return (NULL);
			}
			n += strlen(get_filename(old_ifile));
			break;
		default:
			n++;
			break;
		}
	}

	e = (char *) ecalloc(n+1, sizeof(char));

	/*
	 * Now copy the string, expanding any "%" or "#".
	 */
	to = e;
	for (fr = s;  *fr != '\0';  fr++)
	{
		switch (*fr)
		{
		case '%':
			strcpy(to, get_filename(curr_ifile));
			to += strlen(to);
			break;
		case '#':
			strcpy(to, get_filename(old_ifile));
			to += strlen(to);
			break;
		default:
			*to++ = *fr;
			break;
		}
	}
	*to = '\0';
	return (e);
}

#if TAB_COMPLETE_FILENAME

/*
 * Return a blank-separated list of filenames which "complete"
 * the given string.
 */
	public char *
fcomplete(s)
	char *s;
{
	char *fpat;
	/*
	 * Complete the filename "s" by globbing "s*".
	 */
#if MSOFTC
	/*
	 * But in DOS, we have to glob "s*.*".
	 * But if the final component of the filename already has
	 * a dot in it, just do "s*".  
	 * (Thus, "FILE" is globbed as "FILE*.*", 
	 *  but "FILE.A" is globbed as "FILE.A*").
	 */
	char *slash;
	for (slash = s+strlen(s)-1;  slash > s;  slash--)
		if (*slash == '/' || *slash == '\\')
			break;
	fpat = (char *) ecalloc(strlen(s)+4, sizeof(char));
	if (strchr(slash, '.') == NULL)
		sprintf(fpat, "%s*.*", s);
	else
		sprintf(fpat, "%s*", s);
#else
	fpat = (char *) ecalloc(strlen(s)+2, sizeof(char));
	sprintf(fpat, "%s*", s);
#endif
	s = glob(fpat);
	if (s != NULL && strcmp(s,fpat) == 0)
	{
		free(s);
		s = NULL;
	}
	free(fpat);
	return (s);
}
#endif

/*
 * Try to determine if a file is "binary".
 * This is just a guess, and we need not try too hard to make it accurate.
 */
	public int
bin_file(f)
	int f;
{
	int i;
	int n;
	unsigned char data[64];

	n = read(f, data, sizeof(data));
	for (i = 0;  i < n;  i++)
		if (binary_char(data[i]))
			return (1);
	return (0);
}

/*
 * Try to determine the size of a file by seeking to the end.
 */
	static POSITION
seek_filesize(f)
	int f;
{
	off_t spos;

	spos = lseek(f, (off_t)0, 2);
	if (spos == BAD_LSEEK)
		return (NULL_POSITION);
	return ((POSITION) spos);
}

/*
 * Expand a filename, substituting any environment variables, etc.
 */
#if GLOB

FILE *popen();

	static char *
readfd(fd)
	FILE *fd;
{
	int len;
	int ch;
	char *buf;
	char *p;
	
	len = 100;
	buf = (char *) ecalloc(len, sizeof(char));
	for (p = buf;  ;  p++)
	{
		if ((ch = getc(fd)) == '\n' || ch == EOF)
			break;
		if (p - buf >= len-1)
		{
			len *= 2;
			*p = '\0';
			p = (char *) ecalloc(len, sizeof(char));
			strcpy(p, buf);
			free(buf);
			buf = p;
			p = buf + strlen(buf);
		}
		*p = ch;
	}
	*p = '\0';
	return (buf);
}

	static FILE *
shellcmd(cmd, s1, s2)
	char *cmd;
	char *s1;
	char *s2;
{
	char *scmd;
	char *scmd2;
	char *shell;
	FILE *fd;
	int len;
	
	len = strlen(cmd) + 
		(s1 == NULL ? 0 : strlen(s1)) + 
		(s2 == NULL ? 0 : strlen(s2)) + 1;
	scmd = (char *) ecalloc(len, sizeof(char));
	sprintf(scmd, cmd, s1, s2);
	shell = getenv("SHELL");
	if (shell != NULL && *shell != '\0')
	{
		/*
		 * Read the output of <$SHELL -c "cmd">.
		 */
		scmd2 = (char *) ecalloc(strlen(shell) + strlen(scmd) + 7,
					sizeof(char));
		sprintf(scmd2, "%s -c \"%s\"", shell, scmd);
		free(scmd);
		scmd = scmd2;
	}

	fd = popen(scmd, "r");
	free(scmd);
	return (fd);
}

	public char *
glob(filename)
	char *filename;
{
	FILE *fd;
	char *gfilename;

	filename = fexpand(filename);
	if (filename == NULL)
		return (NULL);

	/*
	 * We get the shell to expand the filename for us by passing
	 * an "echo" command to the shell and reading its output.
	 */
	fd = shellcmd("echo %s", filename, (char*)NULL);
	if (fd == NULL)
	{
		/*
		 * Cannot create the pipe.
		 * Just return the original (fexpanded) filename.
		 */
		return (filename);
	}
	free(filename);
	gfilename = readfd(fd);
	pclose(fd);
	if (*gfilename == '\0')
		return (NULL);
	return (gfilename);
}

	public char *
open_altfile(filename)
	char *filename;
{
	char *lessopen;
	char *gfilename;
	FILE *fd;
	
	if ((lessopen = getenv("LESSOPEN")) == NULL)
		return (NULL);
	filename = fexpand(filename);
	if (filename == NULL)
		return (NULL);
	fd = shellcmd(lessopen, filename, (char*)NULL);
	if (fd == NULL)
	{
		/*
		 * Cannot create the pipe.
		 */
		return (NULL);
	}
	free(filename);
	gfilename = readfd(fd);
	pclose(fd);
	if (*gfilename == '\0')
		return (NULL);
	return (gfilename);
}

	public void
close_altfile(altfilename, filename)
	char *altfilename;
	char *filename;
{
	char *lessclose;
	FILE *fd;
	
	if ((lessclose = getenv("LESSCLOSE")) == NULL)
	     	return;
	fd = shellcmd(lessclose, filename, altfilename);
	pclose(fd);
}
		
#else
#if MSOFTC

	public char *
glob(filename)
	char *filename;
{
	register char *gfilename;
	register char *p;
	register int len;
	register int n;
	struct find_t fnd;
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	
	filename = fexpand(filename);
	if (filename == NULL)
		return (NULL);
	if (_dos_findfirst(filename, ~0, &fnd) != 0)
		return (filename);
		
	_splitpath(filename, drive, dir, fname, ext);
	len = 100;
	gfilename = (char *) ecalloc(len, sizeof(char));
	p = gfilename;
	do {
		n = strlen(drive) + strlen(dir) + strlen(fnd.name);
		while (p - gfilename + n+2 >= len)
		{
			len *= 2;
			*p = '\0';
			p = (char *) ecalloc(len, sizeof(char));
			strcpy(p, gfilename);
			free(gfilename);
			gfilename = p;
			p = gfilename + strlen(gfilename);
		}
		sprintf(p, "%s%s%s", drive, dir, fnd.name);
		p += n;
		*p++ = ' ';
	} while (_dos_findnext(&fnd) == 0);
	
	*--p = '\0';
	return (gfilename);
}
	
	public char *
open_altfile(filename)
	char *filename;
{
	return (NULL);
}

	public void
close_altfile(altfilename, filename)
	char *altfilename;
	char *filename;
{
}
		
#else

	public char *
glob(filename)
	char *filename;
{
	return (fexpand(filename));
}

	
	public char *
open_altfile(filename)
	char *filename;
{
     	return (NULL);
}

	public void
close_altfile(altfilename, filename)
	char *altfilename;
	char *filename;
{
}
		
#endif
#endif


#if HAVE_STAT

#include <sys/stat.h>

/*
 * Returns NULL if the file can be opened and
 * is an ordinary file, otherwise an error message
 * (if it cannot be opened or is a directory, etc.)
 */
	public char *
bad_file(filename)
	char *filename;
{
	register char *m;
	struct stat statbuf;

	if (stat(filename, &statbuf) < 0)
		return (errno_message(filename));

	if (force_open)
		return (NULL);

	if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
	{
		static char is_dir[] = " is a directory";
		m = (char *) ecalloc(strlen(filename) + sizeof(is_dir), 
			sizeof(char));
		strcpy(m, filename);
		strcat(m, is_dir);
		return (m);
	}
	if ((statbuf.st_mode & S_IFMT) != S_IFREG)
	{
		static char not_reg[] = " is not a regular file";
		m = (char *) ecalloc(strlen(filename) + sizeof(not_reg), 
			sizeof(char));
		strcpy(m, filename);
		strcat(m, not_reg);
		return (m);
	}

	return (NULL);
}

/*
 * Return the size of a file, as cheaply as possible.
 * In Unix, we can stat the file.
 */
	public POSITION
filesize(f)
	int f;
{
	struct stat statbuf;

	if (fstat(f, &statbuf) < 0)
		/*
		 * Can't stat; try seeking to the end.
		 */
		return (seek_filesize(f));

	return ((POSITION) statbuf.st_size);
}

#else

/*
 * If we have no way to find out, just say the file is good.
 */
	public char *
bad_file(filename)
	char *filename;
{
	return (NULL);
}

/*
 * We can find the file size by seeking.
 */
	public POSITION
filesize(f)
	int f;
{
	return (seek_filesize(f));
}

#endif