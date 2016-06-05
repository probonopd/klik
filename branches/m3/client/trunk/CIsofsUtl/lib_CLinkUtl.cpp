/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2007 by Lionel Tricon <lionel.tricon AT free.fr>
 *   Small class to merge a path and a symbolic link
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
 *  CLinkUtl::GetRealPath
 * -----------------------------------------------------------------------------
 */

#include "lib_CLinkUtl.hh"

string
CLinkUtl::GetRealPath(
string p_path,                                                        // symbolic link file
string p_link                                                         // contents of the symbolic link
)
{
    vector<string>::iterator cross;
    istringstream v_path(p_path);
    istringstream v_link(p_link);
    ostringstream v_result("");
    string word;

    // in case the symbolic link is absolute
    if (p_link!="" && p_link.c_str()[0]=='/') return p_link;

    // we explode the original path and save the chuncks into a vector
    while (getline(v_path, word, '/' )) _path.push_back(word);

    // we explode the symbolic link and save the chuncks into a vector
    while (getline(v_link, word, '/' )) _link.push_back(word);

    // we remove the last element of the path
    _path.pop_back();

    // we go through each element of the symbolic link
    for (cross=_link.begin();cross!=_link.end();cross++)
    {
        if (*cross == "..") _path.pop_back();
        else if (*cross=="." || *cross=="") {}
        else _path.push_back(*cross);
    }

    // we concat all elements into the final string
    for (cross=_path.begin();cross!=_path.end();cross++)
    {
        if (*cross == "") continue;
        v_result << "/" << *cross;
    }

    _path.clear();
    _link.clear();
    return v_result.str();
}

