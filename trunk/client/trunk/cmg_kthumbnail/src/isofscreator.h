/*
 *  Copyright (C) 2007 by Lionel Tricon <lionel.tricon AT gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef _ISOFSCREATOR_H_
#define _ISOFSCREATOR_H_

#include <kio/thumbcreator.h>
#include <kimageio.h>

#include "cmg_utl.hh"
#include "link_utl.hh"

class IsoFsCreator : public ThumbCreator, CLinkUtl
{
    public:

        IsoFsCreator() { _buffer=NULL; };
        virtual bool create(const QString&, int, int, QImage&);
        virtual Flags flags() const;

    private:

        bool readRecipe(QString&);
        bool readImage(const QString&);

        CmgUtl *_isoUtl;
        char *_buffer;
        mode_t _mode;
        long _size;
};

#endif	/* _ISOFSCREATOR_H_ */
