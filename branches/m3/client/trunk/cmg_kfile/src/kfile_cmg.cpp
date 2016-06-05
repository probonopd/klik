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
 * -----------------------------------------------------------------------------
 *  cmgPlugin::cmgPlugin
 *  cmgPlugin::readInfo
 * -----------------------------------------------------------------------------
 */

/* see also http://packages.debian.org/about/debtags */

#define RECIPE "/recipe.xml"

#include <config.h>
#include "kfile_cmg.h"

#include <kgenericfactory.h>

typedef KGenericFactory<cmgPlugin> cmgFactory;

K_EXPORT_COMPONENT_FACTORY(kfile_cmg, cmgFactory("kfile_cmg"))

/*
 * How we want to introduce data
 *
 */
cmgPlugin::cmgPlugin(
QObject *parent,
const char *name,
const QStringList &args
) : KFilePlugin(parent, name, args)
{
    // add the mimetype here
    KFileMimeTypeInfo* info = addMimeTypeInfo("application/x-extension-cmg");

    // create all groups
    KFileMimeTypeInfo::GroupInfo *group=0L;
    group = addGroupInfo(info, "cmgInfo", i18n("CMG Information"));

    // our new items in the group
    KFileMimeTypeInfo::ItemInfo* item;
    item = addItemInfo(group, "Application", i18n("Application"), QVariant::String);
    item = addItemInfo(group, "Version", i18n("Version"), QVariant::String);
    item = addItemInfo(group, "Homepage", i18n("Homepage"), QVariant::String);
    item = addItemInfo(group, "Category", i18n("Category"), QVariant::String);
    item = addItemInfo(group, "Summary", i18n("Summary"), QVariant::String);

    // mapping of debtags
    _debtags["suite::gnome"] = "GNOME";
    _debtags["suite::gnu"] = "GNU";
    _debtags["suite::gnustep"] = "GNUStep";
    _debtags["suite::kde"] = "KDE";
    _debtags["suite::xfce"] = "XFce";
    _debtags["field::arts"] = i18n("Arts");
    _debtags["field::astronomy"] = i18n("Astronomy");
    _debtags["field::aviation"] = i18n("Aviation");
    _debtags["field::biology"] = i18n("Biology");
    _debtags["field::biology:bioinformatics"] = i18n("Bioinformatics");
    _debtags["field::biology:molecular"] = i18n("Molecular biology");
    _debtags["field::biology:structural"] = i18n("Structural biology");
    _debtags["field::chemistry"] = i18n("Chemistry");
    _debtags["field::electronics"] = i18n("Electronics");
    _debtags["field::finance"] = i18n("Financial");
    _debtags["field::genealogy"] = i18n("Genealogy");
    _debtags["field::geography"] = i18n("Geography");
    _debtags["field::geology"] = i18n("Geology");
    _debtags["field::linguistics"] = i18n("Linguistics");
    _debtags["field::mathematics"] = i18n("Mathematics");
    _debtags["field::medicine"] = i18n("Medicine");
    _debtags["field::medicine:imaging"] = i18n("Medical Imaging");
    _debtags["field::physics"] = i18n("Physics");
    _debtags["field::religion"] = i18n("Religion");
    _debtags["field::statistics"] = i18n("Statistics");
    _debtags["use::analysing"] = i18n("Analysing");
    _debtags["use::browsing"] = i18n("Browsing");
    _debtags["use::chatting"] = i18n("Chatting");
    _debtags["use::compressing"] = i18n("Compressing");
    _debtags["use::configuring"] = i18n("Configuration");
    _debtags["use::converting"] = i18n("Data Conversion");
    _debtags["use::downloading"] = i18n("Downloading");
    _debtags["use::editing"] = i18n("Editing");
    _debtags["use::entertaining"] = i18n("Entertaining");
    _debtags["use::learning"] = i18n("Learning");
    _debtags["use::monitor"] = i18n("Monitoring");
    _debtags["use::organizing"] = i18n("Data Organisation");
    _debtags["use::playing"] = i18n("Playing Media");
    _debtags["use::printing"] = i18n("Printing");
    _debtags["use::proxying"] = i18n("Proxying");
    _debtags["use::searching"] = i18n("Searching");
    _debtags["use::synchronizing"] = i18n("Synchronisation");
    _debtags["use::text-formatting"] = i18n("Text Formatting");
    _debtags["use::timekeeping"] = i18n("Time and Clock");
    _debtags["use::typesetting"] = i18n("Typesetting");
    _debtags["use::viewing"] = i18n("Data Visualization");
    _debtags["game::adventure"] = i18n("Adventure");
    _debtags["game::arcade"] = i18n("Action and Arcade");
    _debtags["game::board"] = i18n("Board");
    _debtags["game::board:chess"] = i18n("Chess");
    _debtags["game::card"] = i18n("Card");
    _debtags["game::demos"] = i18n("Demo");
    _debtags["game::fps"] = i18n("First person shooter");
    _debtags["game::mud"] = i18n("Multiplayer RPG");
    _debtags["game::platform"] = i18n("Platform");
    _debtags["game::puzzle"] = i18n("Puzzle");
    _debtags["game::rpg"] = i18n("Role-playing");
    _debtags["game::rpg:rogue"] = i18n("Rogue-Like RPG");
    _debtags["game::simulation"] = i18n("Simulation");
    _debtags["game::sport"] = i18n("Sport games");
    _debtags["game::sport:racing"] = i18n("Racing");
    _debtags["game::strategy"] = i18n("Strategy");
    _debtags["game::tetris"] = i18n("Tetris-like");
    _debtags["game::toys"] = i18n("Toy or Gimmick");
    _debtags["game::typing"] = i18n("Typing Tutor");
    _debtags["junior::arcade"] = i18n("Arcade games");
    _debtags["junior::games-gl"] = i18n("3D games");
    _debtags["junior::meta"] = i18n("Metapackages");
    _debtags["office::finance"] = i18n("Finance");
    _debtags["office::groupware"] = i18n("Groupware");
    _debtags["office::presentation"] = i18n("Presentation");
    _debtags["office::project-management"] = i18n("Project management");
    _debtags["office::spreadsheet"] = i18n("Spreadsheet");
    _debtags["web::blog"] = i18n("Blog Software");
    _debtags["web::browser"] = i18n("Browser");
    _debtags["sound::compression"] = i18n("Sound compression");
    _debtags["sound::midi"] = i18n("MIDI Software");
    _debtags["sound::mixer"] = i18n("Sound mixing");
    _debtags["sound::player"] = i18n("Sound playback");
    _debtags["sound::recorder"] = i18n("Sound recording");
    _debtags["sound::sequencer"] = i18n("MIDI Sequencing");
    _debtags["sound::speech"] = i18n("Speech Synthesis");
}

/*
 * Extract informations from the cmg file
 *
 */
bool
cmgPlugin::readInfo(
KFileMetaInfo& info,
uint /*what*/
)
{
    QString v_list_category("");
    string v_searchFile = RECIPE;
    QDomNode v_node;
    int v_retval;

    // initialize the CIsoUtl class with the cmg file
    _isoUtl = new CIsoUtl(info.path().ascii());

    // try to get the size of the recipe file
    if (!_isoUtl->GetFileInfo(v_searchFile, this->_mode, this->_size)) return false;

    // data allocation for the recipe
    _buffer = (char*)malloc(_size+1);
    if (_buffer == NULL) return false;
    memset(_buffer, '\0', _size+1);

    // extract the recipe from the iso image
    v_retval = _isoUtl->ReadFile(v_searchFile, _buffer, _size, 0);

    // read the DOM tree recipe file
    if (v_retval!=_size || !_recipe.setContent((QString)_buffer))
    {
        free(_buffer);
        _buffer = NULL;
        return false;
    }

    // we add the main group
    KFileMetaInfoGroup group = appendGroup(info, "cmgInfo");

    // default values
    appendItem(group, "Application", i18n("Not available"));
    appendItem(group, "Version",     i18n("Not available"));
    appendItem(group, "Summary",     i18n("Not available"));
    appendItem(group, "Homepage",    i18n("Not available"));
    appendItem(group, "Category",    i18n("Not available"));

    // now, we look after all the embedded informations
    // for the name, summary and homepage tag names
    QDomElement root = _recipe.documentElement();
    v_node = root.firstChild();
    while (!v_node.isNull())
    {
        QString tempo;

        if (v_node.isElement())
        {
            tempo = v_node.toElement().text();
            if (tempo.isEmpty())
            {
                v_node = v_node.nextSibling();
                continue;
            }

            // case for the different entries
            if (v_node.nodeName() == "name") appendItem(group, "Application", tempo);
            else if (v_node.nodeName() == "summary") appendItem(group, "Summary", tempo);
            else if (v_node.nodeName() == "homepage") appendItem(group, "Homepage", tempo);
            else if (v_node.nodeName() == "group")
            {
                QDomElement v_node2 = v_node.firstChild().toElement();
	        while (!v_node2.isNull())
	        {
                    if (v_node2.tagName() == "implementation")
                    {
                        tempo = v_node2.attribute("version", "");
                        if (!tempo.isEmpty()) appendItem(group, "Version", tempo);
                        break;
                    }
                    v_node2 = v_node2.nextSibling().toElement();
                }
            }
        }

        v_node = v_node.nextSibling();
    }

    // exploit all debtags 
    QDomNodeList v_debtags = root.elementsByTagName("tag");
    for (int i=0; i<(int)v_debtags.length(); i++)
    {
        QMap<QString, QString>::Iterator v_iter;

        v_iter = _debtags.find(v_debtags.item(i).toElement().text());
        if (v_iter != _debtags.end())
        {
            if (!v_list_category.isEmpty()) v_list_category.append(", ");
            v_list_category.append(v_iter.data());
        }
    }

    if (!v_list_category.isEmpty()) appendItem(group, "Category", v_list_category);
    free(_buffer);
    _buffer = NULL;

    // close the CIsoUtl class before closing
    delete _isoUtl;

    return true;
}

#include "kfile_cmg.moc"
