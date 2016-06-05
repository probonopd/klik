/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2007-2008 by Lionel Tricon <lionel.tricon@gmail.com>
 *   Small utility to deal with cmg files thanks to the CmgUtl class
 *
 * -----------------------------------------------------------------------------
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * -----------------------------------------------------------------------------
 */

#include<stdio.h>
#include<stdlib.h>
#include<libgen.h>
#include<iostream>

#include <config.h>
#include<cmg_utl.hh>

#define BUFFER_SIZE 2048

CmgUtl *cmgUtl;
bool Helpflag = false;
bool Verboseflag = false;
bool Recursiveflag = false;

void
recurs_display(
string pathdir
)
{
    vector<string>::iterator v_iter;
    vector<string> v_list;

    // if the path contains a final slash, remove it
    if (pathdir.length()>1 && pathdir.substr(pathdir.length()-1,1)=="/")
    {
        pathdir = pathdir.substr(0, pathdir.length()-1);
    }

    // in case the path is a link
    if (cmgUtl->IsLink(pathdir))
    {
        string followLink = cmgUtl->GetRealPath(pathdir,cmgUtl->GetLinkInfo(pathdir));
        if (pathdir == followLink) return;
        if (!cmgUtl->IsDir(followLink)) return;
        pathdir = followLink;
    }

    // Read the directory
    bool ret = cmgUtl->ReadDir(pathdir,v_list);
    if (!ret) return;

    for (v_iter=v_list.begin(); v_iter!=v_list.end(); v_iter++)
    {
        bool isDir = cmgUtl->IsDir(*v_iter);
        bool isLink = cmgUtl->IsLink(*v_iter);

        // we avoid to display . and .. directories
        if ((string)basename((char*)(*v_iter).c_str()) == ".") continue;
        if ((string)basename((char*)(*v_iter).c_str()) == "..") continue;

        if (Verboseflag)
        {
            if (isDir) cout << "d " << *v_iter << endl;
            else if (isLink) cout << "l " << *v_iter << " -> " << cmgUtl->GetLinkInfo(*v_iter) << endl;
            else cout << "f " << *v_iter << endl;
        }
        else
        {
            if (!isDir && !isLink) cout << *v_iter << endl;
        }

        // go through the directory
        if (Recursiveflag && isDir)
        {
            (void)recurs_display(*v_iter);
        }
    }
}

int
main(int argc, char *argv[])
{
    char *isofile = NULL;
    char *listdir = NULL;
    char *readfile = NULL;
    char *outfile = NULL;
    string newpath;
    mode_t mode;
    long size;
    int c;

    while ((c = getopt (argc, argv, "hrvf:rl:e:o:")) != -1)
    {
        switch (c)
        {
           case 'f': isofile = optarg; break;
           case 'l': listdir = optarg; break;
           case 'e': readfile = optarg; break;
           case 'h': Helpflag = true; break;
           case 'v': Verboseflag = true; break;
           case 'r': Recursiveflag = true; break;
           case 'o': outfile = optarg; break;

           default: exit(EXIT_FAILURE);
        }
    }

    if (Helpflag || (argc!=2 && (isofile==NULL || (!listdir && !readfile))))
    {
        fprintf(stderr, "cmginfo %s\n", VERSION);
        fprintf(stderr, "list a directory : %s [-h] -f cmgfile [-r] -l directory [-v]\n", argv[0]);
        fprintf(stderr, "extract a file : %s [-h] -f cmgfile -e file_to_extract [-o output file]\n", argv[0]);
        fprintf(stderr, "listing of all files and directories : %s cmgfile (assume \"-r -l / -f cmgfile\")\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    // If no arguments other than an existing file are given,
    // we assume that file is a cmg and then we try to give a
    // listing of all files and directories contained in that cmg
    if (argc == 2)
    {
        isofile = argv[1];
        Recursiveflag = true;
        listdir = (char*)malloc(2);
        strcpy(listdir, "/");
    }

    // Open the cmg file
    cmgUtl = new CmgUtl(isofile);

    // We want to list all files inside
    if (listdir)
    {
        recurs_display(listdir);
        exit(EXIT_SUCCESS);
    }

    // We want to extract a file
    if (readfile)
    {
        FILE *newfile;
        newpath = readfile;
        if (!cmgUtl->GetFileInfo(newpath,mode,size)) exit(EXIT_FAILURE);
        if (outfile == NULL) newfile=stdout;
        else newfile = fopen(outfile, "wb");
        (void)cmgUtl->ExtractFile(newpath, newfile);
        if (outfile!=NULL && newfile!=stdout) fclose(newfile);
    }

    // Close the cmg file
    delete cmgUtl;

    exit(EXIT_SUCCESS);
}

