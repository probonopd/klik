/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2007 by Lionel Tricon <lionel.tricon AT gmail.com>
 *   Kde kfile meta-data for cmg files
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
 */

#ifndef __KFILE_CMG_H__
#define __KFILE_CMG_H__

#include <kfilemetainfo.h>
#include <qdom.h>
#include <iostream>

#include "cmg_utl.hh"

class QStringList;

class cmgPlugin: public KFilePlugin
{
    Q_OBJECT
    
    public:

        cmgPlugin(QObject *parent, const char *name, const QStringList& args);
        virtual bool readInfo( KFileMetaInfo& info, uint what);

    private:

        QMap<QString, QString> _debtags;
        QDomDocument _recipe;
        CmgUtl *_isoUtl;
        char *_buffer;
        mode_t _mode;
        long _size;
};

#endif // __KFILE_CMG_H__
