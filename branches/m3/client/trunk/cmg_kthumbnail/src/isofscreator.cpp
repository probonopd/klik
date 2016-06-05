/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2007 by Lionel Tricon <lionel.tricon AT gmail.com>
 *   Kde thumbnail creator for cmg files (iso images)
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
 *  IsoFsCreator::readRecipe
 *  IsoFsCreator::readImage
 *  IsoFsCreator::create
 *  IsoFsCreator::flags
 * -----------------------------------------------------------------------------
 */

/* see also http://api.kde.org/4.0-api/kdelibs-apidocs/kio/html/classThumbCreator.html */

#define RECIPE "/recipe.xml"

#include <qimage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <qdom.h>
#include <iostream>

#include "isofscreator.h"

/*
 * Read the xml recipe file and return an icon
 *
 */
bool
IsoFsCreator::readRecipe(
QString &p_image
)
{
    QDomDocument v_recipe;
    string v_searchPath = RECIPE;
    int v_retval;

    // try to get the size of the recipe file
    if (!_isoUtl->GetFileInfo(v_searchPath, this->_mode, this->_size)) return false;

    // data allocation for the recipe
    _buffer = (char*)malloc(_size+1);
    if (_buffer == NULL) return false;
    memset(_buffer, '\0', _size+1);

    // extract the recipe from the iso image
    v_retval = _isoUtl->ReadFile(v_searchPath, _buffer, _size, 0);

    // read the DOM tree recipe file
    if ( v_retval!=_size || !v_recipe.setContent((QString)_buffer))
    {
        free(_buffer);
        _buffer = NULL;
        return false;
    }

    // now, we look after all icons tags
    QDomElement v_root = v_recipe.documentElement();
    QDomNodeList v_icons = v_root.elementsByTagName("icon");
    for (int i=0; i<(int)v_icons.length(); i++)
    {
        if (v_icons.item(i).toElement().hasAttribute("href"))
        {
            p_image = v_icons.item(i).toElement().attribute("href");
            free(_buffer);
            _buffer = NULL;
            return true;
        }
    }

    free(_buffer);
    _buffer = NULL;
    return false;
}

/*
 * Read an image and return data in the private _buffer variable
 *
 */
bool
IsoFsCreator::readImage(
const QString &p_path
)
{
    int v_retval;
    string v_searchPath = (char*)p_path.latin1();

    // try to get the size of the image
    if (!_isoUtl->GetFileInfo(v_searchPath, _mode, _size)) return false;

    // data allocation for the image
    _buffer = (char*)malloc(_size+1);
    if (_buffer == NULL) return false;
    memset(_buffer, '\0', _size+1);

    // extract the image from the iso image
    v_retval = _isoUtl->ReadFile(v_searchPath, _buffer, _size, 0);
    if (v_retval != _size)
    {
        free(_buffer);
        _buffer = NULL;
        return false;
    }

    return true;
}

/*
 * Creates a thumbnail
 *
 * It seems that only plugins that create an image "from scratch", like
 * the TextCreator should directly use the specified size.
 *
 */
bool
IsoFsCreator::create(
const QString &p_path,                                                // path of the cmg file to open
int p_width,                                                          // maximum width for the preview
int p_height,                                                         // maximum height for the preview
QImage &p_img                                                         // location to fill with an image
)
{
    QString v_image = "/.DirIcon";

    // initialize the CIsoUtl class with the cmg file
    _isoUtl = new CIsoUtl(p_path.latin1());

    // first, we try to load the /.DirIcon file,
    if (!this->readImage(v_image))
    {
        // in case of failure, we check into the recipe file
        if (this->readRecipe(v_image))
        {
            (void)this->readImage(v_image);
        }
    }

    // close the CIsoUtl class
    delete _isoUtl;

    // load the image into the QImage buffer
    if (_buffer == NULL) return false;
    if (!p_img.loadFromData((const uchar *)_buffer,_size))
    {
        free(_buffer);
        _buffer = NULL;
        return false;
    }
    if (p_img.depth() != 32) p_img=p_img.convertDepth(32);
    if (p_img.width()>p_width || p_img.height()>p_height)
    {
        p_img = p_img.smoothScale(p_width, p_height, QImage::ScaleMin);
    }

    free(_buffer);
    return true;
}

/*
 * Flags for this plugin
 *
 */
ThumbCreator::Flags
IsoFsCreator::flags() const
{
    return None;
}

