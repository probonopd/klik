/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2005-2006 by Dmitry Morozhnikov <dmiceman AT mail.ru>
 *   Original version (http://ubiz.ru/dm/)
 *
 *   Copyright (C) 2006-2008 by Lionel Tricon <lionel.tricon AT free.fr>
 *   Klik2 support - union-fuse + sandbox feature
 *
 *   Copyright (C) 2007 by Ryan Thomas
 *   Protection against cornercase when read request is exactly equal to
 *   the uncompressed file size
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
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * -----------------------------------------------------------------------------
 */

/*
 * We keep inside this file all functions dedicated to read isofs image
 *
 * FUNCTIONS
 *  (static) dstr
 *  (static) isofs_check_rr
 *  (static) isofs_date
 *  (static) isofs_parse_zisofs_header
 *  isofs_real_preinit
 *  isofs_real_init
 *  isofs_read_raw_block
 *  isofs_direntry2stat
 *  isofs_fix_entry
 *  isofs_free_inode
 *  isofs_parse_sa
 *  isofs_real_read_zf
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <utime.h>
#include <iconv.h>
#include <pthread.h>
#include <zlib.h>

#include <linux/rock.h>

#include "fusioniso_debug.h"
#include "fusioniso.h"

static pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;

static char* dstr(char* str, const char* src, int len)
{
    int i;
    strncpy(str, src, len);
    str[len] = '\0';
    for (i=len-1; i>=0; --i)
    {
        if (str[i] == '\0' || str[i] == ' ' || str[i] == '\t' || str[i] == '\r' || str[i] == '\n')
        {
            str[i] = '\0';
        }
        else
        {
            return str;
        }
    }
    return str;
}


/**
 * Check for Rock Ridge header
 *
 * @return 1 if RR header is found,
 *         0 if not found,
 *         -1 on error
 */
static int isofs_check_rr(struct iso_directory_record *root_record, isofs_context *isofs)
{
    int extent = isonum_733(root_record->extent);
    char *buf = (char *) malloc(isofs->data_size);            // can we use "standard" 2048 there?
    if (!buf)
    {
        FSWARNING("can\'t malloc: %s",strerror(errno));
        return -1;
    }

    int rc = isofs_read_raw_block(extent, buf, isofs);
    if (rc < 0)
    {
        free(buf);
        return 0;
    }

    struct iso_directory_record *record = (struct iso_directory_record *) buf;
    size_t record_length = isonum_711((unsigned char*) record->length);
    size_t name_len = isonum_711(record->name_len);
    size_t pad_len = ((name_len & 1) ? 0 : 1);                        // padding byte if name_len is even
    size_t sa_len = record_length - name_len - sizeof(struct iso_directory_record) - pad_len;
    if (record_length < sizeof(struct iso_directory_record))
    {
        FSWARNING("directory record length too small: %d",record_length);
        free(buf);
        return -1;
    }
    if (name_len != 1)
    {
        FSWARNING("file name length too big for . record: %d",name_len);
        free(buf);
        return -1;
    }
    if (sa_len < 0)
    {
        // probably something wrong with name_len
        FSWARNING("wrong name_len in directory entry: %d, record_length %d",name_len, record_length);
        free(buf);
        return -1;
    }

    if (sa_len >= 7)
    {
        struct rock_ridge *sue = (struct rock_ridge *) (((char *) record) +
            sizeof(struct iso_directory_record) +
            name_len + pad_len);

        int sue_sig = SIG(sue->signature[0], sue->signature[1]);
        int sue_len = sue->len;
        int sue_version = sue->version;

        if (sue_sig == SIG('S', 'P'))
        {
            if (sue_len != 7 || sue_version != 1 || sue->u.SP.magic[0] != 0xbe || sue->u.SP.magic[1] != 0xef)
            {
                // incorrect SP entry
                free(buf);
                return 0;
            }
            else
            {
                // got it!
                free(buf);
                return 1;
            }
        }
        else
        {
            // not SP record
            free(buf);
            return 0;
        }
    }
    else
    {
        // no space for SP record
        free(buf);
        return 0;
    }

    // should not happen
    free(buf);
    return 0;
}


/**
 * Extract date info from a string
 *
 * @return >0 (timestamp) if string contains a date,
 *         0 otherwise
 */
static time_t isofs_date(char *stamp, int stamp_len)
{
    struct tm tm;
    memset(& tm, 0, sizeof(struct tm));

    if (stamp_len == 7)                                               // ISO9660:9.1.5
    {
        tm.tm_year = stamp[0];
        tm.tm_mon = stamp[1] - 1;
        tm.tm_mday = stamp[2];
        tm.tm_hour = stamp[3];
        tm.tm_min = stamp[4];
        tm.tm_sec = stamp[5];
        tm.tm_isdst = -1;
        tm.tm_gmtoff = stamp[6] * 15 * 60;
    }                                                                 // ISO9660:8.4.26.1
    else if (stamp_len == 17)
    {
        FSDEBUG("17 byte date isn`t supported for the moment, sorry");
        return 0;
    }
    else
    {
        FSDEBUG("unsupported date format, stamp_len %d",stamp_len);
        return 0;
    }

    if (tm.tm_gmtoff)
    {
        //         fprintf(stderr, "direntry2stat: non-zero timezone offset: %d\n", tm.tm_gmtoff);
    }
    time_t time = mktime(& tm);

    return time;
}

// borrowed from zisofs-tools
static const unsigned char zisofs_magic[8] =
{
    0x37, 0xE4, 0x53, 0x96, 0xC9, 0xDB, 0xD6, 0x07
};


/**
 * Parse inode and search for zisofs header
 *
 * @return 1 if zisofs header is found,
 *         0 if not found (or invalid),
 *         -1 on error
 */
static int isofs_parse_zisofs_header(isofs_inode *inode)
{
    isofs_context *isofs=isofs_get_context();
    char *buf = (char *) malloc(isofs->block_size);
    if (!buf)
    {
        FSWARNING("can\'t malloc: %s",strerror(errno));
        return -1;
    }

    int block_num = isonum_733(inode->record->extent);
    int len = isofs_read_raw_block(block_num, buf, isofs);
    if (len < 0)
    {
        free(buf);
        return -1;
    }
    if (len < (int)inode->zf_header_size)
    {
        FSWARNING("too small block len %d",len);
        free(buf);
        return -1;
    }

    zf_file_header *zf_header = (zf_file_header *) buf;

    if (memcmp(zf_header->magic, zisofs_magic, sizeof(zisofs_magic)))
    {
        // invalid compressed file header
        free(buf);
        return 0;
    }

    size_t block_size = 1 << inode->zf_block_shift;

    inode->zf_nblocks = ((inode->real_size + inode->zf_header_size - 1) / block_size) + 1;

    size_t block_table_size = (inode->zf_nblocks + 1) * 4;
    if (!inode->zf_blockptr)
    {
        inode->zf_blockptr = (int *) malloc(block_table_size);
        if (!inode->zf_blockptr)
        {
            FSWARNING("can\'t malloc: %s",strerror(errno));
            return -1;
        }
    }

    // copy offset table into memory buffer, maintaining iso9660 block boundaries

    int block_table_left = block_table_size;
    int block_table_total = 0;
    int block_table_shift = inode->zf_header_size;

    while (block_table_left)
    {
        size_t block_table_chunk =
            (block_table_left < (int)isofs->data_size - block_table_shift ?
            block_table_left : isofs->data_size - block_table_shift);

        /*        printf("isofs_parse_sa: block_table_size: %d, block_table_left: %d, block_table_total %d, block_table_shift %d, block_table_chunk %d\n",
                                    block_table_size, block_table_left, block_table_total, block_table_shift, block_table_chunk);*/

        memcpy(((char *) inode->zf_blockptr) + block_table_total, buf + block_table_shift, block_table_chunk);

        block_table_left -= block_table_chunk;
        block_table_total += block_table_chunk;
        block_table_shift = 0;

        // read next block
        block_num += 1;
        len = isofs_read_raw_block(block_num, buf, isofs);
        if (len < 0)
        {
            free(buf);
            return -1;
        }

        /*        printf("isofs_parse_sa: block_num: %d, len: %d\n",
                                    block_num, len);*/
    }

    /*    printf("isofs_parse_zisofs_header: real size %d, header size %d, nblocks %d, block size %d\n",
                    inode->real_size, inode->zf_header_size,
                    inode->zf_nblocks, block_size);
                int i;
                for (i = 0; i <= inode->zf_nblocks; i++) {
                    printf("zf block table entry %d have value %d\n", i, inode->zf_blockptr[i]);
                }*/

    // all information for compressed file we have already in ZF entry
    // so just signal what this is really compressed file
    free(buf);
    return 1;
}


/**
 *
 * @return 0 on success,
 *         -1 on error
 */
int isofs_init(char* imagefile, isofs_context **isofs)
{
    int i, rc, vd_type, vd_num=0;
    struct iso_volume_descriptor *vd;
    ssize_t size;
    char *vd_id, buf[129];
    isofs_context *new;

    new=(isofs_context *) malloc(sizeof(isofs_context));
    if (!new) {
        FSWARNING("can\'t malloc: %s",strerror(errno));
        return -1;
    }
    memset(new,0,sizeof(isofs_context));

    new->imagefile=imagefile;
    new->fd=open(imagefile,O_RDONLY);
    if (new->fd<0) {
	FSWARNING("cannot open image file \"%s\"",imagefile);
	return -1;
    }

    // trying to read all volume descriptors
    vd=(struct iso_volume_descriptor *) malloc(sizeof(struct iso_volume_descriptor));
    if (!vd)
    {
        FSWARNING("can\'t malloc: %s",strerror(errno));
        return -1;
    }

    // defaults for iso
    new->block_size = 2048;
    new->data_size = 2048;
    new->block_offset = 0;
    new->file_offset = 0;

    enum
    {
        IDOFF_ISO_2048 = 2048 * 16,
        IDOFF_MODE1_2352 = 2352 * 16 + 16,
        IDOFF_MODE2_2352_RAW = 2352 * 16,
        IDOFF_MODE2_2352 = 2352 * 16 + 24,
        IDOFF_MODE2_2336 = 2336 * 16 + 16,
        IDOFF_NRG = 2048 * 16 + 307200,
    };
    int iso_offsets[] = {IDOFF_ISO_2048, IDOFF_MODE2_2336, IDOFF_MODE2_2352_RAW, IDOFF_NRG};

    // try to find CD001 identifier
    for (i = 0; i < 4; i++)
    {
        if (lseek(new->fd, iso_offsets[i], SEEK_SET) == -1)
        {
            FSWARNING("can\'t lseek() to next possible data start position; is it really supported file? : %s",strerror(errno));
            return -1;
        }

        size = read(new->fd, vd, sizeof(struct iso_volume_descriptor));
        if (size != sizeof(struct iso_volume_descriptor))
        {
            FSWARNING("only %d bytes read from position %d, %d required; is it really supported file?",
                size, iso_offsets[i], sizeof(struct iso_volume_descriptor));
            return -1;
        }

	vd_id = (char *) vd->id;
        if (strncmp("CD001", vd_id, 5) == 0)
        {
            // found CD001!
            // fill context with information about block size and block offsets
            new->id_offset = iso_offsets[i];
            switch (iso_offsets[i])
            {
                case IDOFF_ISO_2048:
                    // normal iso file
                    // use defaults
                    break;
                case IDOFF_MODE2_2352_RAW:
                    new->block_size = 2352;
                    break;
                case IDOFF_MODE2_2336:
                    new->block_size = 2336;
                    new->block_offset = 16;
                    break;
                case IDOFF_NRG:
                    new->file_offset = 307200;
                    break;
                default:
                    break;
            }
            break;
        }
        else if (strncmp("CD001", vd_id+16, 5) == 0)
        {
            new->id_offset = iso_offsets[i] + 16;
            new->block_size = 2352;
            new->block_offset = 16;
            break;
        }
        else if (strncmp("CD001", vd_id+24, 5) == 0)
        {
            new->id_offset = iso_offsets[i] + 24;
            new->block_size = 2352;
            new->block_offset = 24;
            break;
        }
    }

    FSDEBUG("CD001 found at %d, block_size %d, block_offset %d, data_size %d",
                    new->id_offset, new->block_size, new->block_offset, new->data_size);
    while (1)
    {
        if (lseek(new->fd, new->block_size * (16 + vd_num) + 
	            new->block_offset + new->file_offset, SEEK_SET) == -1)
        {
            FSWARNING("can\'t lseek() to next volume descriptor: %s",strerror(errno));
            return -1;
        }

        size = read(new->fd, vd, sizeof(struct iso_volume_descriptor));
        if (size != sizeof(struct iso_volume_descriptor))
        {
            FSWARNING("only %d bytes read from volume descriptor %d, %d required",
                size, vd_num, sizeof(struct iso_volume_descriptor));
            return -1;
        }

        vd_type = isonum_711((unsigned char *)vd->type);
        FSDEBUG("found volume descriptor type %d, vd_num %d",vd_type,vd_num);

        if (strncmp("CD001", vd->id, 5) != 0)
        {
            if (vd_num > 16)
            {
                // no more trying
                FSWARNING("wrong standard identifier in volume descriptor %d, exiting..",vd_num);
                return -1;
            }
            else
            {
                // try to continue
                FSDEBUG("wrong standard identifier in volume descriptor %d, skipping..",vd_num);
            }
        }
        else
        {
            switch (vd_type)
            {
                case ISO_VD_PRIMARY:
                    // check if this is only primary descriptor found
                    if (new->pd.type[0])
                    {
                        FSDEBUG("init: primary volume descriptor already found, skipping..");
                    }
                    else
                    {
                        memcpy(&new->pd, vd, sizeof(struct iso_volume_descriptor));
                        new->root = (struct iso_directory_record *) &new->pd.root_directory_record;
                        new->data_size = isonum_723(new->pd.logical_block_size);

                        if (!new->block_size)
                        {
                            FSDEBUG("wrong block data size %d, using default 2048",new->data_size);
                            new->data_size = 2048;
                        }
#if 0
                        if (new->block_size != 2048)
                        {
                            // report unusual data block size
                            // later
                            // FSDEBUG("Data block size: %d",new->block_size);
                        }
#endif
                        rc = isofs_check_rr(new->root,new);
			if (rc) {
                            new->pd_have_rr = 1;
			} else if (rc<0) {
			    return -1;
                        }
                    }
                    break;

                case ISO_VD_SUPPLEMENTARY:
                {
                    struct iso_supplementary_descriptor *sd = (struct iso_supplementary_descriptor *) vd;
		    int have_rr, joliet_level=0;

                    if (!new->pd.type[0])
                    {
                        FSWARNING("supplementary volume descriptor found, but no primary descriptor!");
                        return -1;
                    }
                    else
                    {
                        if (sd->escape[0] == 0x25 && sd->escape[1] == 0x2f)
                        {
                            switch (sd->escape[2])
                            {
                                case 0x40:
                                    joliet_level = 1;
                                    break;
                                case 0x43:
                                    joliet_level = 2;
                                    break;
                                case 0x45:
                                    joliet_level = 3;
                                    break;
                            }
                        }

                        have_rr = isofs_check_rr((struct iso_directory_record *) sd->root_directory_record,new);
			if (have_rr<0)
				return -1;

                        // switch to SVD only if it contain RRIP or if PVD have no RRIP
                        // in other words, prefer VD with RRIP
                        if ((joliet_level && have_rr) ||
                            (have_rr && !new->pd_have_rr) ||
                            (joliet_level && !new->pd_have_rr))
                        {

                            new->joliet_level = joliet_level;
                            memcpy(&new->sd, vd, sizeof(struct iso_volume_descriptor));
                            new->supplementary = 1;

                            new->root = (struct iso_directory_record *) new->sd.root_directory_record;

                            FSDEBUG("switching to supplementary descriptor %d, joliet_level %d, have_rr %d",
                                   vd_num, joliet_level, have_rr);
                        }
                        else
                        {
                            new->joliet_level = 0;
                            FSDEBUG("found supplementary descriptor %d, flags %d",
                                   vd_num, isonum_711((unsigned char *)sd->flags));
                        }
                    }
                }
                break;

                case 0:
                    // boot record, not interesting..
                    break;

                case ISO_VD_END:
                    free(vd);
                    goto out;
                    break;

                default:
                    FSDEBUG("unsupported volume descriptor type %d, vd_num %d",vd_type,vd_num);
                    break;
            }
        }

        vd_num += 1;
    }

out:
    if (!new->pd.type[0])
    {
        FSWARNING("primary volume descriptor not found! exiting..");
        return -1;
    }

    new->susp = 0;
    new->susp_skip = 0;


    if (new->file_offset == 307200)
    {
        FSDEBUG("NRG image found");
    }
    else if (new->block_size == 2048)
    {
        FSDEBUG("ISO9660 image found");
    }
    else if (new->block_size == 2352 && new->block_offset == 0)
    {
        FSDEBUG("MODE2 RAW BIN image found");
    }
    else if (new->block_size == 2352 && new->block_offset == 16)
    {
        FSDEBUG("MODE1 BIN image found (or CCD MODE1 image, or MDF image)");
    }
    else if (new->block_size == 2352 && new->block_offset == 24)
    {
        FSDEBUG("MODE2 BIN image found (or CCD MODE2 image)");
    }
    else if (new->block_size == 2336 && new->block_offset == 16)
    {
        FSDEBUG("MODE2/2336 BIN image found");
    }
    else
    {
        FSDEBUG("UNKNOWN image found; probably will not work");
    }

    if (new->block_size != 2048)
    {
        // report unusual data block size
        FSDEBUG("Data block size: %d",new->block_size);
    }

    FSDEBUG("System Identifier                 : %s",     dstr(buf, new->pd.system_id,             32));
    FSDEBUG("Volume Identifier                 : %.32s",  dstr(buf, new->pd.volume_id,             32));
    FSDEBUG("Volume Set Identifier             : %.128s", dstr(buf, new->pd.volume_set_id,         128));
    FSDEBUG("Publisher Identifier              : %.128s", dstr(buf, new->pd.publisher_id,          128));
    FSDEBUG("Data Preparer Identifier          : %.128s", dstr(buf, new->pd.preparer_id,           128));
    FSDEBUG("Application Identifier            : %.128s", dstr(buf, new->pd.application_id,        128));
    FSDEBUG("Copyright File Identifier         : %.37s",  dstr(buf, new->pd.copyright_file_id,     37));
    FSDEBUG("Abstract File Identifier          : %.37s",  dstr(buf, new->pd.abstract_file_id,      37));
    FSDEBUG("Bibliographic File Identifier     : %.37s",  dstr(buf, new->pd.bibliographic_file_id, 37));
    FSDEBUG("Volume Creation Date and Time     : %.17s",  dstr(buf, new->pd.creation_date,         17));
    FSDEBUG("Volume Modification Date and Time : %.17s",  dstr(buf, new->pd.modification_date,     17));
    FSDEBUG("Volume Expiration Date and Time   : %.17s",  dstr(buf, new->pd.expiration_date,       17));
    FSDEBUG("Volume Effective Date and Time    : %.17s",  dstr(buf, new->pd.effective_date,        17));

    *isofs=new;
    return 0;
}


/**
 * Read data at 'block' offset and put them into buf
 *
 * @return >=0 on read success (number of bytes read)
 *         -1 on error
 */
int isofs_read_raw_block(int block, char *buf, isofs_context *isofs)
{
    off_t off = block * isofs->block_size + isofs->block_offset + isofs->file_offset;
    if (pthread_mutex_lock(& fd_mutex))
    {
        //int err = errno;
        FSWARNING("can\'t lock fd_mutex: %s",strerror(errno));
        return -1;
    }
    if (lseek(isofs->fd, off, SEEK_SET) == -1)
    {
        FSWARNING("can\'t lseek: %s",strerror(errno));
        pthread_mutex_unlock(& fd_mutex);
        return -1;
    }
    ssize_t len = read(isofs->fd, buf, isofs->data_size);
    if (len != isofs->data_size)
    {
        FSDEBUG("can\'t read full block, (%d on %d bytes from offset %d): %s",
            len, isofs->data_size, (int)off, strerror(errno));
        // fprintf(stderr, "isofs_read_raw_block: huh? reading zeros beyond file end? someone want to save a penny?\n");
        memset(buf + len, 0, isofs->data_size - len);
        // pthread_mutex_unlock(& fd_mutex);
        // return -EIO;
    }
    pthread_mutex_unlock(& fd_mutex);
    // printf("block %d, offset %d, read %d\n", block, (int) off, len);
    return len;
}


/**
 * Parse inode and fill struct stat
 *
 * @return 0 anyhow
 */
int isofs_direntry2stat(struct stat *st, isofs_inode *inode)
{
    isofs_context *isofs=isofs_get_context();
    struct iso_directory_record *record = inode->record;

    // fill st_ino from block number where file start
    // since there is no files begin in the same block
    // st->st_ino = isonum_733(record->extent);
    // but some iso images save space by sharing content between several files
    // so it is better to save it unique
    st->st_ino = inode->st_ino;

    if (inode->ZF)                                                    // compressed file, report real (uncompressed) size
    {
        st->st_size = inode->real_size;
    }                                                                 // no zisofs compression
    else
    {
        st->st_size = isonum_733(record->size);
    }

    st->st_blocks = st->st_size / isofs->data_size;           // should not be to meaningful even for zisofs compression
    st->st_blksize = isofs->data_size;
    st->st_nlink = 1;                                                 // always, even if rrip PX entry found

    if (inode->PX)                                                    // rrip PX entry found and in effect
    {
        st->st_mode = inode->st.st_mode;
        st->st_uid = inode->st.st_uid;
        st->st_gid = inode->st.st_gid;
    }
    else
    {
        /// TODO use hidden flag?
        if (ISO_FLAGS_DIR(record->flags))
        {
            st->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH |
                S_IXUSR | S_IXGRP | S_IXOTH;                          // dir, readabale and browsable by everyone
        }
        else
        {
            st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;      // regular, read by everyone
        }
    }

    if (inode->TF)                                                    // rrip TF entry found and in effect
    {
        st->st_atime = inode->st.st_atime;
        st->st_ctime = inode->st.st_ctime;
        st->st_mtime = inode->st.st_mtime;
    }
    else
    {
        if (!inode->ctime)
        {
            inode->ctime = isofs_date(record->date, 7);
        }

        st->st_atime = inode->ctime;
        st->st_ctime = inode->ctime;
        st->st_mtime = inode->ctime;
    }

    return 0;
}

/**
 * Fix string 'entry' (lowercase, charset, ...)
 *
 * @return 0 on success ('entry' is fixed),
 *         -1 on error
 */
int isofs_fix_entry(char **entry, size_t len, int joliet_level, char *iocharset)
{
    if (!joliet_level)                      // iso9660 names
    {
        char *sep1, *sep2, *str;

	str=*entry;

        sep1=index(str, ';');               // find first occurrence of ';'
        if (sep1)                           // remove remaining part
            *sep1 = '\0';

        sep2=rindex(str, '.');              // find last occurrence of '.'
        if (sep2 && sep2[1] == '\0')        // check if '.' is a last symbol in string
            *sep2 = '\0';                   // remove it


        str=*entry;
        // this part is borrowed from linux kernel code
        // convert remaining ';' and '/' characters to dots
        // and lowercase characters in range A-Z
        while (*str)
        {
            if (*str == ';' || *str == '/')
                *str = '.';
            else if (*str >= 'A' && *str <= 'Z')
                *str |= 0x20;

            str++;
        }

        return 0;
    }
    else
    {
        iconv_t cd;
	char *inbuf, *outbuf, *outstr;
	size_t inbytesleft, outbytesleft, outlen;
	int rc;

        // initialize iconv descriptor
        cd=iconv_open(iocharset, "UCS-2BE");
        if (cd < 0)
        {
            FSWARNING("iconv_open error: %s",strerror(errno));
            return -1;
        }

        inbuf=*entry;
        inbytesleft=len;

        outbuf=(char *) malloc(NAME_MAX);   // this should be sufficient for our purposes
        if (!outbuf)
        {
            FSWARNING("can\'t malloc: %s",strerror(errno));
            return -1;
        }
        outbytesleft=NAME_MAX;

        outstr=outbuf;

        rc=iconv(cd,&inbuf,&inbytesleft,&outbuf,&outbytesleft);
        outlen=NAME_MAX-outbytesleft;
	if (rc<0)
        {
            // incorrect multibyte char or other iconv error -- return as much as possible anyway
            FSWARNING("iconv error on '%s': %s",outstr,strerror(errno));
            if (outlen == 0)
            {
                iconv_close(cd);
                free(entry);
                free(outstr);
                return -1;
            }
            // try to recover
        }
        outstr[outlen]='\0';

        //FSDEBUG("outlen %d, outbytesleft %d, rc %d, outbuf %s", outlen, outbytesleft, rc, outstr);

        // borrowed from linux kernel isofs joliet code
        if (outlen > 2 && outstr[outlen - 2] == ';' && outstr[outlen - 1] == '1')
            outstr[outlen - 2] = '\0';
        if (outlen >= 2 && outstr[outlen - 1] == '.')
            outstr[outlen - 1] = '\0';

        iconv_close(cd);
        free(*entry);
	*entry=outstr;
        return 0;
    }
}


/**
 * Parse inode and extract info based on its signature
 *
 * @return 0 on success,
 *         -1 on error
 */
int isofs_parse_sa(isofs_inode *inode, char *sa, size_t sa_len)
{
    int cont_block = 0;
    int cont_offset = 0;
    int cont_size = 0;
    int remaining = sa_len;
    isofs_context *isofs=isofs_get_context();

    while (remaining > 4)                                             // susp 4.
    {
        //FSDEBUG("sa offset %d, remaining %d",sa_len-remaining,remaining);

        struct rock_ridge *sue = (struct rock_ridge *) (sa + sa_len - remaining);
        int sue_sig = SIG(sue->signature[0], sue->signature[1]);
        int sue_len = sue->len;
        int sue_version = sue->version;

	//FSDEBUG("signature %c%c, sue_len %d, sue_version %d",
        //    sue->signature[0], sue->signature[1], sue_len, sue_version);

        int known_sue = 1;

        switch (sue_sig)
        {
            case SIG('S', 'P'):                                       // susp 5.3
                if (sue_len != 7 || sue_version != 1 || sue->u.SP.magic[0] != 0xbe || sue->u.SP.magic[1] != 0xef)
                {
                    // incorrect SP entry
                    FSWARNING("incorrect SP entry: sue_len %d, sue_version %d, magic %c%c",
                        sue_len, sue_version, sue->u.SP.magic[0], sue->u.SP.magic[1]);
                    isofs->susp = 0;
                    return -1;
                }
                else
                {
                    isofs->susp = 1;
                    isofs->susp_skip = sue->u.SP.skip;
                }
                //FSDEBUG("SP entry, skip %d",sue->u.SP.skip);
                break;
            case SIG('C', 'E'):                                       // susp 5.1
                if (sue_len != 28 || sue_version != 1)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect CE entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }                                                     // CE entry already found
                else if (cont_block != 0)
                {
                    FSDEBUG("duplicate CE entry, skipping, sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    cont_block = isonum_733(sue->u.CE.extent);
                    cont_offset = isonum_733(sue->u.CE.offset);
                    cont_size = isonum_733(sue->u.CE.size);

                    if (cont_block < 16)
                    {
                        // continuation area can`t be there
                        FSDEBUG("wrong continuation area extent: %d, cont_offset %d, cont_size %d",
                            cont_block, cont_offset, cont_size);
                        cont_block = 0;                               // do not process it
                    }
                    else if (cont_offset + cont_size > (int)isofs->data_size)
                    {
                        // something wrong with continuation area offset and/or size
                        FSDEBUG("wrong continuation area: extent %d, cont_offset %d, cont_size %d",
                            cont_block, cont_offset, cont_size);
                        cont_block = 0;                               // do not process it
                    }
                    else
                    {
                        //FSDEBUG("CE entry, extent %d, offset %d, size %d",
                        //    cont_block, cont_offset, cont_size);
                    }
                }
                break;
            case SIG('E', 'R'):                                       // susp 5.5
                if (sue_version != 1)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect ER entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    int len_id = sue->u.ER.len_id;
                    int len_des = sue->u.ER.len_des;
                    int len_src = sue->u.ER.len_src;
                    int ext_ver = sue->u.ER.ext_ver;
                    if (len_id + len_des + len_src + 8 > sue_len)
                    {
                        FSDEBUG("incorrect ER entry: sue_len %d, sue_version %d, len_id %d, len_des %d, len_src %d, ext_ver %d",
                            sue_len, sue_version, len_id, len_des, len_src, ext_ver);
                    }
                    else
                    {
                        char id[256];
                        char des[256];
                        char src[256];

                        strncpy(id, sue->u.ER.data, len_id);
                        id[len_id] = '\0';
                        strncpy(des, sue->u.ER.data + len_id, len_des);
                        des[len_des] = '\0';
                        strncpy(src, sue->u.ER.data + len_id + len_des, len_src);
                        src[len_src] = '\0';

                        //FSDEBUG("ER entry: id: %s des: %s src: %s ver: %d",
                        //    id, des, src, ext_ver);
                    }
                }
                break;
            case SIG('R', 'R'):
                {
                // unused
                //isonum_711((unsigned char *) sue->u.RR.flags);
                //FSDEBUG("RR entry, sue_version %d, sue_len %d, flags %d",
                //    sue_version, sue_len, (int)sue->u.RR.flags);
                }
                break;
            case SIG('P', 'X'):                                       // rrip 4.1.1
                // according to rrip 4.1.1, length of PX record should be exactly 44
                // but linux kernel and mkisofs seems to be happy with length 36,
                // where 'serial number' are not presented
                // (or i`m looking at outdated draft.. :-)
                if ((sue_len != 44 && sue_len != 36) || sue_version != 1)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect PX entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    mode_t mode = isonum_733(sue->u.PX.mode);
                    nlink_t nlink = isonum_733(sue->u.PX.n_links);
                    uid_t uid = isonum_733(sue->u.PX.uid);
                    gid_t gid = isonum_733(sue->u.PX.gid);

                    //FSDEBUG("PX entry, sue_version %d, sue_len %d, mode %d, nlinks %d, uid %d, gid %d",
                    //    sue_version, sue_len, mode, nlink, uid, gid);

		    inode->st.st_mode = mode;
                    inode->st.st_nlink = nlink;
                    inode->st.st_uid = uid;
                    inode->st.st_gid = gid;
                    inode->PX = 1;
                    /// TODO check if entry length == 44 and set st_ino field from 'file serial number'?
                }
                break;
            case SIG('S', 'L'):                                       // rrip 4.1.3
                if (sue_version != 1)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect SL entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else if (inode->SL)
                {
                    FSDEBUG("SL entry already in effect, sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    int sl_flags = sue->u.SL.flags;
                    int max_components = (sue_len - 5) / sizeof(struct SL_component);

                    if (max_components < 1)
                    {
                        FSDEBUG("SL entry found, but no components: sue_len %d, sue_version %d",
                            sue_len, sue_version);
                    }
                    else
                    {
                        int c;
                        int c_offset = 0;
                        int c_errors = 0;
                        for (c = 0; c < max_components; c++)
                        {
                            struct SL_component *comp =
                                (struct SL_component *)
                                (((char *) & sue->u.SL.link) + c_offset);

                            int sl_c_flags = comp->flags;
                            int sl_c_len = comp->len;

                            if (c_offset + 5 >= sue_len)
                            {
                                // according to rrip, we must stop if CONTINUE flag isn`t set
                                // according to linux kernel isofs code and images produced witj mkisofs
                                // we need to continue while there is space in SL entry for next component
                                break;

                                // strict rrip code:
                                // fprintf(stderr,
                                //     "parse_sa: invalid SL component: sue_len %d, sue_version %d, sl_flags %d, sl_c_flags %d, sl_c_len %d\n",
                                //     sue_len, sue_version, sl_flags, sl_c_flags, sl_c_len);
                                // c_errors++;
                                // break;

                                /// TODO find _working_ rrip specification
                            }

                            int c_len = 0;
                            char c_separator = 0;
                            if (!inode->sl_len)                       // nothing found previoustly
                            {
                            }                                         // previous SL component was ROOT
                            else if (inode->sl_len == 1 && inode->sl[0] == '/')
                            {
                                // no need for separator after ROOT component
                            }
                            else
                            {
                                c_len += 1;                           // place for '/' separator
                                c_separator = '/';
                            }
                            if (sl_c_flags & (1 << 1))                // CURRENT component
                            {
                                c_len += 1;                           // place for '.' character
                            }                                         // PARENT component
                            else if (sl_c_flags & (1 << 2))
                            {
                                c_len += 2;                           // place for ".." characters
                            }
                            else
                            {
                                c_len += sl_c_len;
                            }

                            if (c_len + inode->sl_len + 1 > PATH_MAX)
                            {
                                FSDEBUG("too long symlink found: sue_len %d, sue_version %d, sl_flags %d, sl_c_flags %d, sl_c_len %d, c_len %d",
                                    sue_len, sue_version, sl_flags, sl_c_flags, sl_c_len, c_len);
                                c_errors++;
                                break;
                            }

                            if (!inode->sl)
                            {
                                inode->sl = (char *) malloc(PATH_MAX);
                                if (!inode->sl)
                                {
                                    FSWARNING("can\'t malloc: %s",strerror(errno));
                                    return -1;
                                }
                            }

                            if (c_separator)
                            {
                                inode->sl[inode->sl_len] = c_separator;
                                inode->sl_len += 1;
                            }

                            if (sl_c_flags & (1 << 1))                // CURRENT component
                            {
                                inode->sl[inode->sl_len] = '.';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '\0';
                                //FSDEBUG("SL CURRENT component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s",
                                //    sl_c_flags, sl_c_len, inode->sl_len, inode->sl);
                            }                                         // PARENT component
                            else if (sl_c_flags & (1 << 2))
                            {
                                inode->sl[inode->sl_len] = '.';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '.';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '\0';
                                //FSDEBUG("SL PARENT component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s",
                                //    sl_c_flags, sl_c_len, inode->sl_len, inode->sl);
                            }                                         // ROOT component (?! does not documented at all)
                            else if (sl_c_flags & (1 << 3))
                            {
                                inode->sl[inode->sl_len] = '/';
                                inode->sl_len += 1;
                                inode->sl[inode->sl_len] = '\0';
                                //FSDEBUG("SL ROOT component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s",
                                //    sl_c_flags, sl_c_len, inode->sl_len, inode->sl);
                            }
                            else
                            {
                                strncpy(inode->sl + inode->sl_len, comp->text, sl_c_len);
                                inode->sl_len += sl_c_len;
                                inode->sl[inode->sl_len] = '\0';
                                //FSDEBUG("SL component, sl_c_flags %d, sl_c_len %d, sl_len %d, sl %s",
                                //    sl_c_flags, sl_c_len, inode->sl_len, inode->sl);
                            }

                            // according to rrip, we must stop if CONTINUE flag isn`t set
                            // according to linux kernel isofs code and images produced witj mkisofs
                            // we need to continue while there is space in SL entry for next component
                            c_offset += (2 + sl_c_len);

                            // strict rrip code:
                            // if (sl_c_flags & 1) { // CONTINUE
                            //     c_offset += 2 + sl_c_len;
                            // } else {
                            //     printf("parse_sa: SL final component, sl_c_len %d, sl_c_flags %d, sl_len %d, sl %s\n",
                            //         sl_c_len, sl_c_flags, inode->sl_len, inode->sl);
                            //     break;
                            // }
                        }

                        if (c_errors)
                        {
                            //FSDEBUG("errors found while processing SL components, cleaning");
                            if (inode->sl)
                            {
                                free(inode->sl);
                            }
                            inode->sl_len = 0;
                            inode->SL = 0;
                        }
                        else if (!(sl_flags & 1) && inode->sl)
                        {
                            //FSDEBUG("SL entry (final), sue_len %d, sue_version %d, sl_len %d, sl %s",
                            //    sue_len, sue_version, inode->sl_len, inode->sl);
                            inode->SL = 1;
                        }
                        else if (!(sl_flags & 1) && !inode->sl)
                        {
                            //FSDEBUG("final SL entry found, but no SL components, cleaning");
                            if (inode->sl)
                            {
                                free(inode->sl);
                            }
                            inode->sl_len = 0;
                            inode->SL = 0;
                        }
                        else if (inode->sl)
                        {
                            //FSDEBUG("SL entry, sue_len %d, sue_version %d, sl_len %d, sl %s",
                            //    sue_len, sue_version, inode->sl_len, inode->sl);
                        }
                        else
                        {
                            //FSDEBUG("empty SL entry?");
                        }
                    }
                }
                break;
            case SIG('N', 'M'):                                       // rrip 4.1.4
                if (sue_version != 1)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect NM entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    int nm_flags = sue->u.NM.flags;
                    if (nm_flags & (1 << 1))                          // CURRENT bit
                    {
                        //FSDEBUG("NM CURRENT entry, sue_version %d, sue_len %d, flags %d",
                        //    sue_version, sue_len, nm_flags);
                    }                                                 // PARENT bit
                    else if (nm_flags & (1 << 2))
                    {
                        //FSDEBUG("NM PARENT entry, sue_version %d, sue_len %d, flags %d",
                        //    sue_version, sue_len, nm_flags);
                    }
                    else
                    {
                        if (sue_len - 5 + inode->nm_len > NAME_MAX - 1)
                        {
                            FSDEBUG("too long NM entry: %d",
                                sue_len - 5 + inode->nm_len);
                        }
                        else if (inode->NM)
                        {
                            FSDEBUG("NM entry already in effect, sue_len %d, sue_version %d",
                                sue_len, sue_version);
                        }
                        else
                        {
                            if (!inode->nm)
                            {
                                inode->nm = (char *) malloc(NAME_MAX);
                                if (!inode->nm)
                                {
                                    FSWARNING("can\'t malloc: %s",strerror(errno));
                                    return -1;
                                }
                            }

                            strncpy(inode->nm + inode->nm_len, sue->u.NM.name, sue_len - 5);
                            inode->nm_len += sue_len - 5;
                            inode->nm[inode->nm_len] = '\0';

                            if (!nm_flags & 1)                        // CONTINUE bit
                            {
                                inode->NM = 1;
                                //FSDEBUG("NM entry (final), flags %d, len %d, name %s",
                                //    nm_flags, sue_len - 5, inode->nm);
                            }
                            else
                            {
                                //FSDEBUG("NM entry (part), flags %d, len %d, name %s",
                                //    nm_flags, sue_len - 5, inode->nm);
                            }
                        }
                    }
                }
                break;
            case SIG('C', 'L'):                                       // rrip 4.1.5.1
                if (sue_version != 1 || sue_len != 12)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect CL entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    int cl_block = isonum_733(sue->u.CL.location);

		    //FSDEBUG("CL entry, block %d",cl_block);

		    inode->CL = 1;
                    inode->cl_block = cl_block;

                    // read block pointed by CL record and process first directory entry
                    char *buf = (char *) malloc(isofs->data_size);
                    if (!buf)
                    {
                        FSWARNING("can\'t malloc: %s",strerror(errno));
                        return -1;
                    }
                    int rc = isofs_read_raw_block(inode->cl_block, buf, isofs);
                    if (rc < 0)
                    {
                        free(buf);
                        return -1;
                    }

                    struct iso_directory_record *record = (struct iso_directory_record *) buf;
                    size_t record_length = isonum_711((unsigned char *) record->length);
                    size_t name_len = isonum_711(record->name_len);
                    size_t pad_len = ((name_len & 1) ? 0 : 1);        // padding byte if name_len is even
                    size_t sa_len = record_length - name_len - sizeof(struct iso_directory_record) - pad_len;

                    //FSDEBUG("record length %d, name_len %d, pad_len %d, sa_len %d",
                    //    record_length, name_len, pad_len, sa_len);

		    if (record_length < sizeof(struct iso_directory_record))
                    {
                        FSWARNING("CL entry: directory record length too small: %d",record_length);
                        free(buf);
                        return -1;
                    }
                    if (name_len != 1)
                    {
                        FSWARNING("file name length too big for . record: %d",name_len);
                        free(buf);
                        return -1;
                    }
                    if (sa_len < 0)
                    {
                        // probably something wrong with name_len
                        FSWARNING("CL record: wrong name_len in directory entry: %d, record_length %d",
                            name_len, record_length);
                        free(buf);
                        return -1;
                    }

                    // ignoring anything from original record
                    struct iso_directory_record *cl_record =
                        (struct iso_directory_record *) malloc(sizeof(struct iso_directory_record));
                    if (!cl_record)
                    {
                        FSWARNING("can\'t malloc: %s",strerror(errno));
                        return -1;
                    }
                    memcpy(cl_record, record, sizeof(struct iso_directory_record));

                    // drop existing record
                    if (inode->record)
                    {
                        free(inode->record);
                    }

                    // replace record with new one
                    inode->record = cl_record;

                    // parse sa entries from relocated directory . record
                    rc = isofs_parse_sa(inode,
                        ((char *) record) +
                        sizeof(struct iso_directory_record) +
                        name_len + pad_len + isofs->susp_skip,
                        sa_len);
                    if (rc<0)
                    {
                        free(buf);
                        return -1;
                    }

                    free(buf);
                }
                break;
            case SIG('P', 'L'):                                       // rrip 4.1.5.2
                if (sue_version != 1 || sue_len != 12)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect PL entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    int pl_block = isonum_733(sue->u.PL.location);
                    /*                    printf("parse_sa: PL entry, block %d\n",
                                                                                                pl_block);*/
                    // probably we don`t need process PL record with FUSE
                    inode->PL = 1;
                    inode->pl_block = pl_block;
                }
                break;
            case SIG('R', 'E'):                                       // rrip 4.1.5.3
                if (sue_version != 1 || sue_len != 4)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect RE entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    //                     printf("parse_sa: RE entry\n");
                    inode->RE = 1;
                }
                break;
            case SIG('T', 'F'):                                       // rrip 4.1.6
                if (sue_version != 1)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect TF entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    int tf_flags = sue->u.TF.flags;
                    int stamp_length;
                    if (tf_flags & TF_LONG_FORM)
                    {
                        // iso9660:8.4.26.1 format
                        stamp_length = 17;
                    }
                    else
                    {
                        // iso9660:9.1.5 format
                        stamp_length = 7;
                    }

                    time_t ctime = 0;
                    time_t mtime = 0;
                    time_t atime = 0;

                    int stamp_no = 0;
                    // ctime can be stored as CREATION time
                    if (tf_flags & TF_CREATE)
                    {
                        ctime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    }
                    if (tf_flags & TF_MODIFY)
                    {
                        mtime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    }
                    if (tf_flags & TF_ACCESS)
                    {
                        atime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    }
                    // ctime should be stored in ATTRIBUTES stamp according to rrip 4.1.6
                    if (tf_flags & TF_ATTRIBUTES)
                    {
                        ctime = isofs_date(((char *) sue) + 5 + stamp_length * stamp_no, stamp_length);
                        stamp_no++;
                    }
                    // other fields have no meaning for us

                    //FSDEBUG("TF entry, sue_version %d, sue_len %d, ctime %d, mtime %d, atime %d",
                    //    sue_version, sue_len, (int)ctime, (int)mtime, (int)atime);
                    inode->st.st_ctime = ctime;
                    inode->st.st_mtime = mtime;
                    inode->st.st_atime = atime;
                    inode->TF = 1;
                }
                break;
            case SIG('S', 'F'):                                       // rrip 4.1.7
                if (sue_version != 1 || sue_len != 21)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect SF entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else
                {
                    /// TODO does anyone support SF entries? linux isofs code does not..
                    //FSDEBUG("SF entries (sparse files) are unsupported in this version, sorry..");
                }
                break;
            case SIG('Z', 'F'):                                       // non-standard linux extension (zisofs)
                if (sue_version != 1 || sue_len != 16)
                {
                    // incorrect entry, skip
                    FSDEBUG("incorrect ZF entry: sue_len %d, sue_version %d",
                        sue_len, sue_version);
                }
                else if (SIG(sue->u.ZF.algorithm[0], sue->u.ZF.algorithm[1]) == SIG('p', 'z'))
                {
                    inode->zf_algorithm[0] = sue->u.ZF.algorithm[0];
                    inode->zf_algorithm[1] = sue->u.ZF.algorithm[1];
                    inode->zf_header_size = ((unsigned char) sue->u.ZF.parms[0]) << 2;
                    inode->zf_block_shift = (unsigned char) sue->u.ZF.parms[1];
                    inode->zf_size = (inode->st.st_size ? inode->st.st_size : isonum_733(inode->record->size));
                    inode->real_size = isonum_733(sue->u.ZF.real_size);
                    // check if file is really compressed, ignore ZF entry otherwise
                    int rc = isofs_parse_zisofs_header(inode);
                    if (rc)
                    {
                        //FSDEBUG("ZF entry found, algorithm %02x%02x, header size %d, block shift %d, compressed size %d, real size %d",
                        //    inode->zf_algorithm[0], inode->zf_algorithm[1],
                        //    inode->zf_header_size, inode->zf_block_shift,
                        //    inode->zf_size, inode->real_size);
                        inode->ZF = 1;
                    }
                    else if (rc==0)
                    {
                        FSDEBUG("ZF entry found, but file is not really compressed");
                    }                                                 // some error occur
                    else
                    {
                        return -1;
                    }
                }
                else
                {
                    FSDEBUG("unknown ZF compression algorithm %c%c, sorry..",
                        sue->u.ZF.algorithm[0], sue->u.ZF.algorithm[1]);
                }
                break;
            default:
                known_sue = 0;
                FSDEBUG("unknown entry '%c%c', sue_sig %d, sue_version %d, sue_len %d",
                    sue->signature[0], sue->signature[1], sue_sig, sue_version, sue_len);
                break;
        }

        if (sue_len >= 4 && known_sue)
        {
            remaining -= sue_len;
        }
        else if (known_sue)
        {
            FSWARNING("inappropriate susp entry length: %d, signature %c%c",
                sue_len, sue->signature[0], sue->signature[1]);
            return -1;
        }                                                             // probably there are no more susp entries
        else
        {
            return 0;
        }
    }

    // process continuation area if found
    if (cont_block)
    {
        char *buf = (char *) malloc(isofs->data_size);
        if (!buf)
        {
            FSWARNING("can\'t malloc: %s",strerror(errno));
            return -1;
        }
        int rc = isofs_read_raw_block(cont_block, buf, isofs);
        if (rc < 0)
        {
            free(buf);
            return -1;
        }
        //FSDEBUG("deep into CE, extent %d, offset %d, size %d",
        //    cont_block, cont_offset, cont_size);
        rc = isofs_parse_sa(inode, buf + cont_offset, cont_size);
        if (rc<0)
        {
            free(buf);
            return -1;
        }

        free(buf);
    }

    return 0;
}


/**
 * Read compressed data from inode
 *
 * @return >=0 on read success (number of bytes read)
 *         -1 on error
 */
int isofs_real_read_zf(isofs_inode *inode, char *out_buf, size_t size, off_t offset)
{
    int zf_block_size = 1 << inode->zf_block_shift;
    int zf_start = offset / zf_block_size;
    int zf_end = (offset + size) / zf_block_size;
    // Protection against cornercase when read request is exactly equal to the uncompressed file size.
    // PATCH #1 -- Ryan Thomas 2007-06-13
    if ((offset+size ) % zf_block_size==0) zf_end--;
    int shift = offset % zf_block_size;
    isofs_context *isofs=isofs_get_context();

    //     printf("zf_start %d, zf_end %d, size %d, offset %d, shift %d\n",
    //         zf_start, zf_end, size, (int) offset, shift);

    // protection against some ununderstandable errors
    if (zf_start >= inode->zf_nblocks) zf_start = inode->zf_nblocks-1;
    if (zf_end >= inode->zf_nblocks) zf_end = inode->zf_nblocks-1;
    if (zf_end < 0 || zf_start < 0) return -1;

    unsigned char *cbuf = (unsigned char *) malloc(zf_block_size * 2);
    if (!cbuf)
    {
        FSWARNING("can\'t malloc: %s",strerror(errno));
        return -1;
    }
    unsigned char *ubuf = (unsigned char *) malloc(zf_block_size);
    if (!ubuf)
    {
        FSWARNING("can\'t malloc: %s",strerror(errno));
        return -1;
    }

    size_t total_size = 0;
    size_t size_left = size;

    int base_offset = isonum_733(inode->record->extent) * isofs->data_size;

    int i;
    for (i = zf_start; i <= zf_end; i++)
    {
        int block_offset = isonum_731((char *) (& inode->zf_blockptr[i]));
        int block_end = isonum_731((char *) (& inode->zf_blockptr[i + 1]));
        int block_size = block_end - block_offset;

        if (block_size == 0)                                          // sort of sparce file block
        {
            size_t payload_size = (zf_block_size - shift < (int)size_left?(int)(zf_block_size-shift):(int)size_left);
            memset(out_buf + total_size, 0, payload_size);

            total_size += payload_size;
            size_left -= payload_size;
        }
        else if (block_size > zf_block_size * 2)
        {
            FSWARNING("compressed block size bigger than uncompressed block size * 2, something is wrong there.."
                "block size %d, uncompressed block size %d",block_size,zf_block_size);
            free(cbuf);
            free(ubuf);
            return -1;
        }
        else
        {
            // we do not use isofs_read_raw_block() there because it is simpler to access
            // compressed blocks directly

            int image_off = base_offset + block_offset;

            if (pthread_mutex_lock(& fd_mutex))
            {
                //int err = errno;
                FSWARNING("can\'t lock fd_mutex: %s",strerror(errno));
                free(cbuf);
                free(ubuf);
                return -1;
            }
            if (lseek(isofs->fd, image_off, SEEK_SET) == -1)
            {
                FSWARNING("can\'t lseek: %s",strerror(errno));
                pthread_mutex_unlock(& fd_mutex);
                free(cbuf);
                free(ubuf);
                return -1;
            }
            ssize_t len = read(isofs->fd, cbuf, block_size);
            if ((int)len != block_size)
            {
                FSWARNING("can\'t read full block, errno %d, message %s",
                    errno, strerror(errno));
                pthread_mutex_unlock(& fd_mutex);
                free(cbuf);
                free(ubuf);
                return -1;
            }
            pthread_mutex_unlock(& fd_mutex);

            // compressed block is read from disk, now uncompress() it

            uLongf usize = zf_block_size;
            int rc = uncompress(ubuf, &usize, cbuf, block_size);
            if (rc != Z_OK)
            {
                free(cbuf);
                free(ubuf);
                FSWARNING("uncompress() error %i: %s",rc,strerror(rc));
                return -1;
            }

            // ubuf contain uncompressed data, usize contain uncompressed block size

            size_t payload_size = (usize - shift < size_left ? usize - shift : size_left);
            memcpy(out_buf + total_size, ubuf + shift, payload_size);

            total_size += payload_size;
            size_left -= payload_size;
        }

        // shift is only meaningful for first block
        shift = 0;
    }

    free(cbuf);
    free(ubuf);

    //     printf("total size %d\n", total_size);

    return total_size;
}
