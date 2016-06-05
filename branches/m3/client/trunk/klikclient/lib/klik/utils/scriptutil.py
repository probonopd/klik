#!/usr/bin/env python
# -*- coding: UTF-8 -*-

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""
Module providing functions commonly used in shell scripting:

  - ffind()    : finds files in a directory tree
  - ffindgrep(): finds files in a directory tree and matches their
                 content to regular expressions
  - freplace() : in-place search/replace of files in a directory tree
                 with regular expressions
  - printr()   : prints the results of the ffind()/ffindgrep() functions

Please see the documentation strings of the particular functions for
detailed information.
"""

# Copyright: (c) 2007 Muharem Hrnjadovic
# created: 15/04/2007 09:31:25

__version__ = "$Id:$"
# $HeadURL $

import os, sys, types, re, fnmatch

class ScriptError(Exception): pass

def ffind(path, shellglobs=None, namefs=None, relative=True):
    """
    Finds files in the directory tree starting at 'path' (filtered by
    Unix shell-style wildcards ('shellglobs') and/or the functions in
    the 'namefs' sequence).

    The parameters are as follows:

    - path: starting path of the directory tree to be searched
    - shellglobs: an optional sequence of Unix shell-style wildcards
      that are to be applied to the file *names* found
    - namefs: an optional sequence of functions to be applied to the
      file *paths* found
    - relative: a boolean flag that determines whether absolute or
      relative paths should be returned

    Please not that the shell wildcards work in a cumulative fashion
    i.e. each of them is applied to the full set of file *names* found.

    Conversely, all the functions in 'namefs'
        * only get to see the output of their respective predecessor
          function in the sequence (with the obvious exception of the
          first function)
        * are applied to the full file *path* (whereas the shell-style
          wildcards are only applied to the file *names*)

    Returns a sequence of paths for files found.
    """
    if not os.access(path, os.R_OK):
        raise ScriptError("cannot access path: '%s'" % path)

    fileList = [] # result list
    try:
        for dir, subdirs, files in os.walk(path.encode('ascii')):
            if shellglobs:
                matched = []
                for pattern in shellglobs:
                    filterf = lambda s: fnmatch.fnmatchcase(s, pattern)
                    matched.extend(filter(filterf, files))
                fileList.extend(['%s%s%s' % (dir, os.sep, f) for f in matched])
            else:
                fileList.extend(['%s%s%s' % (dir, os.sep, f) for f in files])
        if not relative: fileList = map(os.path.abspath, fileList)
        if namefs: 
            for ff in namefs: fileList = filter(ff, fileList)
    except Exception, e: raise ScriptError(str(e))
    return(fileList)

def ffindgrep(path, regexl, shellglobs=None, namefs=None, relative=True):
    """
    Finds files in the directory tree starting at 'path' (filtered by
    Unix shell-style wildcards ('shellglobs') and/or the functions in
    the 'namefs' sequence) and searches inside these.

    The parameters are as follows:

    - path: starting path of the directory tree to be searched
    - shellglobs: an optional sequence of Unix shell-style wildcards
      that are to be applied to the file *names* found
    - namefs: an optional sequence of functions to be applied to the
      file *paths* found
    - relative: a boolean flag that determines whether absolute or
      relative paths should be returned

    Additionaly, the file content will be filtered by the regular
    expressions in the 'regexl' sequence. Each entry in the latter
    is a
    
      - either a string (with the regex definition)
      - or a tuple with arguments accepted by re.compile() (the
        re.M and re.S flags will have no effect though)

    For all the files that pass the file name/content tests the function
    returns a dictionary where the

      - key is the file name and the
      - value is a string with lines filtered by 'regexl'
    """
    fileList = ffind(path, shellglobs=shellglobs, 
                     namefs=namefs, relative=relative)
    if not fileList: return dict()

    result = dict()

    try:
        # first compile the regular expressions
        ffuncs = []
        for redata in regexl:
            if type(redata) == types.StringType:
                ffuncs.append(re.compile(redata).search)
            elif type(redata) == types.TupleType:
                ffuncs.append(re.compile(*redata).search)
        # now grep in the files found
        for file in fileList:
            # read file content
            fhandle = open(file, 'r')
            fcontent = fhandle.read()
            fhandle.close()
            # split file content in lines
            lines = fcontent.splitlines()
            for ff in ffuncs:
                lines = filter(ff, lines)
                # there's no point in applying the remaining regular
                # expressions if we don't have any matching lines any more
                if not lines: break
            else:
                # the loop terminated normally; add this file to the
                # result set if there are any lines that matched
                if lines: result[file] = '\n'.join(map(str, lines))
    except Exception, e: raise ScriptError(str(e))
    return(result)

def freplace(path, regexl, shellglobs=None, namefs=None, bext='.bak'):
    """
    Finds files in the directory tree starting at 'path' (filtered by
    Unix shell-style wildcards ('shellglobs') and/or the functions in
    the 'namefs' sequence) and performs an in-place search/replace
    operation on these.

    The parameters are as follows:

    - path: starting path of the directory tree to be searched
    - shellglobs: an optional sequence of Unix shell-style wildcards
      that are to be applied to the file *names* found
    - namefs: an optional sequence of functions to be applied to the
      file *paths* found
    - relative: a boolean flag that determines whether absolute or
      relative paths should be returned

    Additionally, an in-place search/replace operation is performed
    on the content of all the files (whose names passed the tests)
    using the regular expressions in 'regexl'.

    Please note: 'regexl' is a sequence of 3-tuples, each having the
    following elements:

      - search string (Python regex syntax)
      - replace string (Python regex syntax)
      - regex flags or 'None' (re.compile syntax)

    Copies of the modified files are saved in backup files using the
    extension specified in 'bext'.

    The function returns the total number of files modified.
    """
    fileList = ffind(path, shellglobs=shellglobs, namefs=namefs)

    # return if no files found
    if not fileList: return 0

    filesChanged = 0

    try:
        cffl = []
        for searchs, replaces, reflags in regexl:
            # prepare the required regex objects, check whether we need
            # to pass any regex compilation flags
            if reflags is not None: regex = re.compile(searchs, reflags)
            else: regex = re.compile(searchs)
            cffl.append((regex.subn, replaces))
        for file in fileList:
            # read file content
            fhandle = open(file, 'r')
            text = fhandle.read()
            fhandle.close()
            substitutions = 0
            # unpack the subn() function and the replace string
            for subnfunc, replaces in cffl:
                text, numOfChanges = subnfunc(replaces, text)
                substitutions += numOfChanges
            if substitutions:
                # first move away the original file
                bakFileName = '%s%s' % (file, bext)
                if os.path.exists(bakFileName): os.unlink(bakFileName)
                os.rename(file, bakFileName)
                # now write the new file content
                fhandle = open(file, 'w')
                fhandle.write(text)
                fhandle.close()
                filesChanged += 1
    except Exception, e: raise ScriptError(str(e))

    # return the number of files that had some of their content changed
    return(filesChanged)

def printr(results):
    """
    prints the results of ffind()/ffindgrep() in a manner similar to
    the UNIX find utility
    """
    if type(results) == types.DictType:
        for f in sorted(results.keys()):
            sys.stdout.write("%s\n%s\n" % (results[f],f))
    else:
        for f in sorted(results):
            sys.stdout.write("%s\n" % f)

if __name__ == '__main__':
    pass

def copytree(src, dst, symlinks=0):
        """Same as shutil.copytree, just doesn't attempt to create the
        destination directory; this makes it work correctly with a temp dir
        created using tempfile.mkdtemp()"""
			
		
        names = os.listdir(src)

        for name in names:
            srcname = os.path.join(src, name)
            dstname = os.path.join(dst, name)
            try:
                if symlinks and os.path.islink(srcname):
                    linkto = os.readlink(srcname)
                    os.symlink(linkto, dstname)
                elif os.path.isdir(srcname):
                    copytree(srcname, dstname, symlinks)
                else:
                    shutil.copy2(srcname, dstname)
            except (IOError, os.error), why:
                print "Can't copy %s to %s: %s" % (`srcname`, `dstname`,
                                                   str(why))
