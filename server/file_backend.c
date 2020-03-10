/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *    file_backend.c
 *    This file is part of "Sauvegarde" project.
 *
 *    (C) Copyright 2015 - 2019 Olivier Delhomme
 *     e-mail : olivier.delhomme@free.fr
 *
 *    "Sauvegarde" is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    "Sauvegarde" is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with "Sauvegarde".  If not, see <http://www.gnu.org/licenses/>
 */
/**
 * @file server/file_backend.c
 *
 * This file contains all the functions for the file backend that saves
 * everything to some flat files somewhere into the filesystem.
 *
 * @note to translators: file_backend is the name of the backend please
 * do not translate this. Thanks.
 */

#include "server.h"


static gchar *build_filename_from_hash(gchar *path, gchar *hex_has, guint level);
static void make_all_subdirectories(file_backend_t *file_backend);
static buffer_t *init_buffer_structure(GFileInputStream *stream);
static void free_buffer_t(buffer_t *a_buffer);
static void read_one_buffer(buffer_t *a_buffer);
static gchar *extract_one_line_from_buffer(buffer_t *a_buffer);
static meta_data_t *extract_from_line(gchar *line, GRegex *a_regex, query_t *query);
static GList *get_file_list_from_regex_and_query(GFileInputStream *stream, GRegex *a_regex, query_t *query);
static gshort get_cmptype_from_file_meta(gchar *filename);
static gssize get_uncmplen_from_file_meta(gchar *filename);
static void set_metadata_to_file_meta(gchar *filename, gssize uncmplen, gshort cmptype);

/**
 * Stores meta data into a flat file. A file is created for each host that
 * sends meta data. This code is not thread safe (it means that this is
 * not safe to call it from different threads unless some mechanism
 * garantees that a write will never occur in the same file at the same
 * time.
 * @param server_struct is the server main structure where all
 *        informations needed by the program are stored.
 * @param smeta the server's structure for file meta data. It contains the
 *        hostname that sent it. This structure IS FREED by this
 *        function.
 * @todo prefix should be set as a configuration's option.
 */
void file_store_smeta(server_struct_t *server_struct, server_meta_data_t *smeta)
{
    GFile *meta_file = NULL;
    gchar *filename = NULL;
    GFileOutputStream *stream = NULL;
    GError *error = NULL;
    gsize count = 0;
    gssize written = 0;
    gchar *string_written = NULL;
    gchar *buffer = NULL;
    gchar *hash_list = NULL;
    meta_data_t *meta = NULL;
    gchar *prefix = NULL;
    file_backend_t *file_backend = NULL;
    gchar *name64 = NULL;
    gchar *link64 = NULL;


    if (server_struct != NULL && server_struct->backend != NULL && server_struct->backend->user_data != NULL && smeta != NULL)
        {
            meta = smeta->meta;
            file_backend = server_struct->backend->user_data;
            prefix = g_build_filename((gchar *) file_backend->prefix, "meta", NULL);

            if (smeta->hostname != NULL && meta != NULL)
                {
                    filename = g_build_filename(prefix, smeta->hostname, NULL);

                    meta_file = g_file_new_for_path(filename);

                    stream = g_file_append_to(meta_file, G_FILE_CREATE_NONE, NULL, &error);

                    if (stream != NULL)
                        {
                            hash_list = convert_hash_data_list_to_gchar(meta->hash_data_list);

                            name64 = encode_to_base64(meta->name);
                            link64 = encode_to_base64(meta->link);

                            if (name64 == NULL)
                                {
                                    name64 = g_strdup("");
                                }

                            if (link64 == NULL)
                                {
                                    link64 = g_strdup("");
                                }

                            if (hash_list != NULL)
                                {
                                    buffer = g_strdup_printf("%d, %" G_GUINT64_FORMAT ", %d, %" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", \"%s\", \"%s\", %d, %d, \"%s\", \"%s\", %s\n", meta->file_type, meta->inode, meta->mode, meta->atime, meta->ctime, meta->mtime, meta->size, meta->owner, meta->group, meta->uid, meta->gid, name64, link64, hash_list);
                                    free_variable(hash_list);
                                }
                            else
                                {
                                    buffer = g_strdup_printf("%d, %" G_GUINT64_FORMAT ", %d, %" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", %" G_GUINT64_FORMAT ", \"%s\", \"%s\", %d, %d, \"%s\", \"%s\"\n", meta->file_type, meta->inode, meta->mode, meta->atime, meta->ctime, meta->mtime, meta->size, meta->owner, meta->group, meta->uid, meta->gid, name64, link64);
                                }

                            free_variable(name64);
                            free_variable(link64);

                            count = strlen(buffer);
                            written = g_output_stream_write((GOutputStream *) stream, buffer, count, NULL, &error);

                            if (error != NULL)
                                {
                                    string_written = g_strdup_printf("%"G_GSSIZE_FORMAT, written);
                                    print_error(__FILE__, __LINE__, _("Error: unable to write to file %s (%s bytes written).\n"), filename, string_written);
                                }

                            g_output_stream_close((GOutputStream *) stream, NULL, &error);
                            free_variable(buffer);
                        }
                    else
                        {
                            print_error(__FILE__, __LINE__, _("Error: unable to open file %s to append meta-data in it.\n"), filename);
                        }

                    free_object(meta_file);
                    free_variable(filename);
                }
            else
                {
                    print_error(__FILE__, __LINE__, _("Error: no server_meta_data_t structure or missing hostname or missing meta_data_t * structure.\n"));
                }

            free_variable(prefix);
        }
}


/**
 * Builds the filename of a block represented by a hash
 * @param path is the whole path.
 * @param hex_hash the hash to be stored / retrieved.
 * @param cmptype is the compression type (COMPRESS_NONE_TYPE, COMPRESS_ZLIB_TYPE).
 * @param level is the level chosen to store hashs.
 */
static gchar *build_filename_from_hash(gchar *path, gchar *hex_hash, guint level)
{
    gchar *hash_cmp_filename = NULL;
    gchar *filename = NULL;

    /* Stores compression type in the hash filename's separated by a , */
    hash_cmp_filename = g_strdup_printf("%s", hex_hash + level*2);
    filename = g_build_filename(path, hash_cmp_filename, NULL);

    free_variable(hash_cmp_filename);

    return filename;
}


/**
 * Gets cmptype from meta hash file.
 * @param filename is the filename of the hash. The meta file has the
 *        same name but ends with .meta
 * @returns cmptype if possible or COMPRESS_NONE_TYPE.
 */
static gshort get_cmptype_from_file_meta(gchar *filename)
{
    gchar *filename_meta = NULL;
    GKeyFile *keyfile = NULL;
    GError *error = NULL;
    gshort cmptype = COMPRESS_NONE_TYPE;

    filename_meta = g_strdup_printf("%s.meta", filename);
    keyfile = g_key_file_new();

    if (g_key_file_load_from_file(keyfile, filename_meta, G_KEY_FILE_KEEP_COMMENTS, &error))
        {
            cmptype = (gshort) read_int_from_file(keyfile, filename_meta, GN_META, KN_CMPTYPE, _("Error while reading cmptype value"), COMPRESS_NONE_TYPE);
        }

    g_key_file_free(keyfile);

    if (is_compress_type_allowed(cmptype) == FALSE)
        {
            cmptype = COMPRESS_NONE_TYPE;
        }

    return cmptype;
}


/**
 * Gets uncmplen: The uncompressed len of hash file from meta hash file.
 * @param filename is the filename of the hash. The meta file has the
 *        same name but ends with .meta
 * @returns the len of the uncompressed hash file or 0.
 */
static gssize get_uncmplen_from_file_meta(gchar *filename)
{
    gchar *filename_meta = NULL;
    GKeyFile *keyfile = NULL;
    GError *error = NULL;
    gssize uncmplen = 0;

    filename_meta = g_strdup_printf("%s.meta", filename);
    keyfile = g_key_file_new();

    if (g_key_file_load_from_file(keyfile, filename_meta, G_KEY_FILE_KEEP_COMMENTS, &error))
        {
            uncmplen = read_int64_from_file(keyfile, filename_meta, GN_META, KN_UNCMPLEN, _("Error while reading uncmplen value"), 0);
        }

    g_key_file_free(keyfile);

    return uncmplen;
}

/**
 * Sets cmptype and uncmplen in meta hash file.
 * @param filename is the filename of the hash. The meta file has the
 *        same name but ends with .meta
 * @param uncmplen the len of the uncompressed hash file.
 * @param cmptype the compression type used to store this hash file.
 */
static void set_metadata_to_file_meta(gchar *filename, gssize uncmplen, gshort cmptype)
{
    gchar *filename_meta = NULL;
    GKeyFile *keyfile = NULL;
    GError *error = NULL;

    filename_meta = g_strdup_printf("%s.meta", filename);
    keyfile = g_key_file_new();

    if (is_compress_type_allowed(cmptype) == FALSE)
        {
            cmptype = COMPRESS_NONE_TYPE;
        }

    g_key_file_set_int64(keyfile, GN_META, KN_UNCMPLEN, uncmplen);
    g_key_file_set_integer(keyfile, GN_META, KN_CMPTYPE, cmptype);

    g_key_file_save_to_file(keyfile, filename_meta, &error);

    g_key_file_free(keyfile);
    free_variable(filename_meta);
}


/**
 * Stores data into a flat file. The file is named by its hash in hex
 * representation (one should easily check that the sha256sum of such a
 * file gives its name !).
 * @param server_struct is the server's main structure where all
 *        informations needed by the program are stored.
 * @param hash_data is a hash_data_t * structure that contains the hash and
 *        the corresponding data in a binary form and a 'read' field that
 *        contains the number of bytes in 'data' field.
 * @todo return errors when they occurs
 */
void file_store_data(server_struct_t *server_struct, hash_data_t *hash_data)
{
    GFile *data_file = NULL;
    gchar *filename = NULL;
    GFileOutputStream *stream = NULL;
    GError *error = NULL;
    gssize written = 0;
    gchar *string_written = NULL;
    gchar *hex_hash = NULL;
    gchar *path = NULL;
    gchar *prefix = NULL;
    file_backend_t *file_backend = NULL;

    if (server_struct != NULL && server_struct->backend != NULL && server_struct->backend->user_data != NULL)
        {
            file_backend = server_struct->backend->user_data;
            prefix = g_build_filename((gchar *) file_backend->prefix, "data", NULL);

            if (hash_data != NULL && hash_data->hash != NULL && hash_data->data != NULL)
                {
                    path = make_path_from_hash(prefix, hash_data->hash, file_backend->level);
                    hex_hash = hash_to_string(hash_data->hash);

                    filename = build_filename_from_hash(path, hex_hash, file_backend->level);
                    set_metadata_to_file_meta(filename, hash_data->uncmplen, hash_data->cmptype);

                    data_file = g_file_new_for_path(filename);
                    stream = g_file_replace(data_file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);

                    if (stream != NULL)
                        {
                            written = g_output_stream_write((GOutputStream *) stream, hash_data->data, hash_data->read, NULL, &error);

                            if (error != NULL)
                                {
                                    string_written = g_strdup_printf("%"G_GSSIZE_FORMAT, written);
                                    print_error(__FILE__, __LINE__, _("Error: unable to write to file %s (%s bytes written).\n"), filename, string_written);
                                    free_variable(string_written);
                                }

                            g_output_stream_close((GOutputStream *) stream, NULL, &error);

                            g_object_unref(stream);
                            free_variable(hash_data->data);
                            free_variable(hash_data->hash);
                            free_variable(hash_data);
                        }
                    else
                        {
                            print_error(__FILE__, __LINE__, _("Error: unable to open file %s to write data in it.\n"), filename);
                        }

                    free_object(data_file);
                    free_variable(filename);
                    free_variable(hex_hash);
                    free_variable(path);
                }
            else
                {
                    print_error(__FILE__, __LINE__, _("Error: no hash_data_t structure or hash in it or missing data in it.\n"));
                }

            free_variable(prefix);
        }
}


/**
 * Builds a list of hashs that cdpfglerver's server needs.
 * @param server_struct is the server's main structure where all
 *        informations needed by the program are stored.
 * @param hash_list is the list of hashs that we have to check for.
 * @returns to the client a list of hashs in no specific order for which
 *          the server needs the data.
 */
GList *file_build_needed_hash_list(server_struct_t *server_struct, GList *hash_data_list)
{
    GFile *data_file = NULL;
    GList *head = hash_data_list;
    GList *needed = NULL;
    gchar *hex_hash = NULL;
    gchar *filename = NULL;
    gchar *path = NULL;
    gchar *prefix = NULL;
    file_backend_t *file_backend = NULL;
    hash_data_t *hash_data = NULL;
    hash_data_t *needed_hash_data = NULL;


    if (server_struct != NULL && server_struct->backend != NULL && server_struct->backend->user_data != NULL)
        {
            file_backend = server_struct->backend->user_data;

            prefix = g_build_filename((gchar *) file_backend->prefix, "data", NULL);

            while (head != NULL)
                {
                    hash_data = head->data;
                    path = make_path_from_hash(prefix, hash_data->hash, file_backend->level);
                    hex_hash = hash_to_string(hash_data->hash);
                    filename = build_filename_from_hash(path, hex_hash, file_backend->level);
                    data_file = g_file_new_for_path(filename);

                    /* @todo : do we need to request compressed hash if we have an uncompressed version ?
                     * Also : how can the program thy to answer this without knowing that the hash will be compressed or not ? */

                    if (g_file_query_exists(data_file, NULL) == FALSE && hash_data_is_in_list(hash_data, needed) == FALSE)
                        {
                            /* file does not exists and is not in the needed list so we need it!
                             * thus putting it it the needed list
                             */
                            needed_hash_data = copy_only_hash(hash_data, NULL);
                            needed = g_list_prepend(needed, needed_hash_data);
                        }

                    free_object(data_file);
                    free_variable(filename);
                    free_variable(hex_hash);
                    free_variable(path);

                    head = g_list_next(head);
                }

            needed = g_list_reverse(needed);
            free_variable(prefix);
        }

    return needed;
}


/**
 * Makes all subdirectories into the "data" directory.
 * @note creating subdirectories for a level of 2 will take some time and
 *       the empty directories will consume at least 256 Mb of space (ext4
 *       filesystem). A level of 3 will take a long time and will consume
 *       at least like 64 Gb of space (ext4 filesystem). Expect 16 Tb with
 *       level 4 and a very very long time to complete.
 * @param file_backend the structure that contains the prefix path and the
 *        level in which we want to create the subdirectories.
 */
static void make_all_subdirectories(file_backend_t *file_backend)
{
    gchar *path = NULL;
    gchar *path2 = NULL;
    gchar *octet = NULL;
    guint number = 0;
    double i = 0;
    double p = 0;
    double total = 0;

    if (file_backend != NULL && file_backend->level < 5 && file_backend->level > 1)
        {
            total = pow(256, file_backend->level);

            for (i = 0; i < total; i++)
                {
                    path = g_strdup("");

                    for (p = file_backend->level-1; p >= 0; p--)
                        {
                            number = i / (pow(256, p));


                            if (number > 255)
                                {
                                    number = fmod(number, 256);
                                }


                            octet = g_strdup_printf("%02x", number);
                            path2 = g_strconcat(path, octet, "/", NULL);
                            free_variable(path);
                            path = path2;
                            free_variable(octet);
                        }

                    path[strlen(path)-1] = '\0';
                    path2 = g_build_filename(file_backend->prefix, "data", path, NULL);

                    create_directory(path2);

                    free_variable(path);
                    free_variable(path2);

                }

            /* Creates a directory named .done in prefix/data in order
             * to tell that we already have created all direcrories an subdirectories
             */
            path =  g_build_filename(file_backend->prefix, "data", ".done", NULL);
            create_directory(path);
            free_variable(path);
        }
    else if (file_backend != NULL)
        {
            print_error(__FILE__, __LINE__, _("dir-level (%d) should be > 1 and < 5\n"), file_backend->level);
        }
}


/**
 * Reads keys in keyfile if groupname is in that keyfile and fills
 * file_backend structure accordingly.
 * @param[in,out] file_backend: file_backend_t * structure to store
 *                options read from the configuration file "filename".
 * @param filename : the filename of the configuration file to read from
 */
static void read_from_group_file_backend(file_backend_t *file_backend, gchar *filename)
{
    GKeyFile *keyfile = NULL;      /** Configuration file parser */
    GError *error = NULL;          /** Glib error handling       */
    gchar *prefix = NULL;
    guint level = 0;

    keyfile = g_key_file_new();

    if (g_key_file_load_from_file(keyfile, filename, G_KEY_FILE_KEEP_COMMENTS, &error))
        {
            if (keyfile != NULL && filename != NULL && g_key_file_has_group(keyfile, GN_FILE_BACKEND) == TRUE)
                {
                    prefix = read_string_from_file(keyfile, filename, GN_FILE_BACKEND, KN_FILE_DIRECTORY, _("Could not load [file_backend] file-directory from file."));
                    level = read_int_from_file(keyfile, filename, GN_FILE_BACKEND, KN_DIR_LEVEL, _("Could not load [file_backend] dir-level from file."), FILE_BACKEND_LEVEL);
                }
        }
    else if (error != NULL)
        {
            print_error(__FILE__, __LINE__,  _("Failed to open %s configuration file: %s\n"), filename, error->message);
            free_error(error);
        }

    if (prefix != NULL && file_backend != NULL)
        {
            free_variable(file_backend->prefix);
            file_backend->prefix = normalize_directory(prefix);
        }

    free_variable(prefix);

    if (level > 0 && level < 6)
        { /* Will anyone need more than 1 099 511 627 776 directories to
           * store data? with 16k blocs and 256 block files per leafs
           * it represents 4 exabytes !
           */
            file_backend->level = level;
        }

    g_key_file_free(keyfile);
}


/**
 * Inits the backend : takes care of the directories we want to write to.
 * user_data of the backend structure is a file_backend_t structure that
 * contains the prefix path where to store data and the level of
 * indirections
 * @param server_struct is the server's main structure where all
 *        informations needed by the program are stored.
 */
void file_init_backend(server_struct_t *server_struct)
{
    file_backend_t *file_backend = NULL;
    gchar *path = NULL;

    if (server_struct != NULL && server_struct->backend != NULL)
        {
            file_backend = (file_backend_t *) g_malloc0(sizeof(file_backend_t));

            /* default values */
            file_backend->prefix = g_strdup("/var/tmp/cdpfgl/server");
            file_backend->level = FILE_BACKEND_LEVEL;

            if (server_struct->opt != NULL && server_struct->opt->configfile != NULL)
                {
                    /* Values from the config file */
                    read_from_group_file_backend(file_backend, server_struct->opt->configfile);
                }

            server_struct->backend->user_data = file_backend;

            file_create_directory(file_backend->prefix, "meta");
            file_create_directory(file_backend->prefix, "data");

            path =  g_build_filename(file_backend->prefix, "data", ".done", NULL);
            if (file_exists(path) == FALSE)
                {
                    fprintf(stdout, _("Please wait while creating directories\n"));
                    make_all_subdirectories(file_backend);
                    fprintf(stdout, _("Finished !\n"));
                }
            free_variable(path);

        }
    else
        {
            print_error(__FILE__, __LINE__, _("Error: no server structure or no backend structure.\n"));
        }
}


/**
 * Allocates a newly buffer_t structure and fills it with the corresponding
 * values.
 * @param stream is the GFileInputStream stream from which we want to read things
 * @returns a newly allocated buffer_t structure with buf, size and pos
 *          filled accordingly. It may be freed when no longer needed.
 */
static buffer_t *init_buffer_structure(GFileInputStream *stream)
{
    buffer_t *a_buffer = NULL;
    gchar *buf = NULL;

    a_buffer = (buffer_t *) g_malloc0(sizeof(buffer_t));
    buf = (gchar *) g_malloc0(FILE_BACKEND_BUFFER_SIZE + 1); /* to store the \0 at the end ! */

    a_buffer->buf = buf;
    a_buffer->size = 0;
    a_buffer->pos = 0;
    a_buffer->stream = stream;

    return a_buffer;
}


/**
 * Frees the buffer structure
 * @param a_buffer is the buffer structure to be freed
 */
static void free_buffer_t(buffer_t *a_buffer)
{
    free_variable(a_buffer->buf);
    free_variable(a_buffer);
}


/**
 * Reads one entire buffer
 * @param[in,out] a_buffer is a buffer_t * structure containing all that is
 *                needed to read a buffer and to know where we are in it
 *                when parsing it. It fills the structure with the bytes
 *                read, the number of bytes read and puts pos at 0.
 */
static void read_one_buffer(buffer_t *a_buffer)
{
    GError *error = NULL;

    if (a_buffer != NULL && a_buffer->stream != NULL)
        {
            a_buffer->size = g_input_stream_read((GInputStream *) a_buffer->stream, a_buffer->buf, FILE_BACKEND_BUFFER_SIZE, NULL, &error);
            a_buffer->pos = 0;

            if (error != NULL)
                {
                    print_error(__FILE__, __LINE__, _("Error while reading the file: %s\n"), error->message);
                    free_error(error);
                }
        }
}


/**
 * This function extracts one line from the buffer by searching the end of
 * line (assuming unix style '\n' end of lines).
 * @param[in,out] a_buffer contains the buffer the total number of bytes read
 *                and the actual position
 * @returns a gchar * representing a whole line that may be freed when no
 *          longer needed.
 * @warning this function does not work properly (it will not handle ", \n
 *          and , charaters in file names or link names: @todo rewrite
 *          something new that may handle this correctly.
 */
static gchar *extract_one_line_from_buffer(buffer_t *a_buffer)
{
    gchar *line = NULL;
    gchar *a_line = NULL;
    gchar *whole_line = NULL;
    gssize i = 0;
    gint comma = 0;             /* Counts the fields */
    gboolean end = FALSE;       /* end of line detected */
    gboolean in_string = FALSE; /* True when beetween " " and False otherwise */

    if (a_buffer != NULL && a_buffer->buf != NULL)
        {
            i = a_buffer->pos;

            while (end != TRUE && a_buffer->size != 0)
                {
                    if (i < a_buffer->size)
                        {
                            if (a_buffer->buf[i] == '"')
                                {
                                    in_string = !in_string;
                                }
                            else if (a_buffer->buf[i] == ',' && in_string == FALSE)
                                {
                                    comma++;
                                }

                            if (a_buffer->buf[i] == '\n' && in_string == FALSE && comma >= 12)
                                {
                                    end = TRUE;
                                }
                            else
                                {
                                    i++;
                                }
                        }
                    else
                        {
                            /* The line is stored in more than one buffer */
                            line = g_strndup(a_buffer->buf + a_buffer->pos, i - a_buffer->pos);

                            if (whole_line != NULL)
                                {
                                    /* the line is stored in more than 2 buffers */
                                    a_line = g_strconcat(whole_line, line, NULL);
                                    free_variable(whole_line);
                                    free_variable(line);
                                    whole_line = a_line;
                                }
                            else
                                {
                                    /* second buffer */
                                    whole_line = line;
                                }

                            read_one_buffer(a_buffer);
                            i = 0;
                        }
                }

            line = g_strndup(a_buffer->buf + a_buffer->pos, i - a_buffer->pos);
            a_buffer->pos = i + 1; /* the new position is right next '\n' ! */

            if (whole_line != NULL)
                {
                    a_line = g_strconcat(whole_line, line, NULL);
                    free_variable(whole_line);
                    free_variable(line);
                    whole_line = a_line;
                }
            else
                {
                    whole_line = line;
                }
        }

    return whole_line;
}


/**
 * Extracts all meta data from one line.
 * @param line is the line that has been read.
 * @param a_regex is the regular expression to filter upon the filename
 * @param query is the structure that contains everything about the
 *        requested filename.
 * @returns a newly allocated gchar * string containing the filename that
 *          may be freed when no longer needed
 */
static meta_data_t *extract_from_line(gchar *line, GRegex *a_regex, query_t *query)
{
    gchar **params = NULL;
    gchar *filename = NULL;
    meta_data_t *meta = NULL;
    guint32 q_uid = 0;
    guint32 q_gid = 0;
    gboolean res = FALSE;

    if (line != NULL && strlen(line) > 16)
        {
            /**
             * line example : 1, 1049893, 33261, 1432131763, 1432129404, 1425592185, 38680, "root", "root", 0, 0, "/bin/locale", "eAAAdPN/AAAQFgB0838AAFNKQtsAlNrU4QHmJlkxiKA=",
             * "IBUAdPN/AADgCQB0838AACuk6dHfqsfcXvECD/HXSbU=", "4AwAdPN/AAAQFgB0838AAPJ18vuZ+mHsaFOztwu6IWw="
             */

            params = g_strsplit(line, ",", 14);

            filename = get_substring_from_string(params[11], TRUE);

            if (g_regex_match(a_regex, filename, 0, NULL) && (query->reduced != TRUE))
                {
                    meta = new_meta_data_t();

                    meta->name = filename;

                    meta->file_type = get_uint_from_string(params[0]);
                    meta->inode = get_guint64_from_string(params[1]);

                    meta->mode = get_uint_from_string(params[2]);

                    meta->atime = get_guint64_from_string(params[3]);
                    meta->ctime = get_guint64_from_string(params[4]);
                    meta->mtime = get_guint64_from_string(params[5]);

                    res = compare_mtime_to_date(meta->mtime, query->date);

                    /** @todo gain speed in comparison by transforming query->afterdate and query->beforedate only once
                     *        in the calling function
                     */
                    if (query->afterdate != NULL)
                        {
                            res = res && compare_after_before_date(meta->mtime, query->afterdate, TRUE);
                        }

                    if (query->beforedate != NULL)
                        {
                            res = res && compare_after_before_date(meta->mtime, query->beforedate, FALSE);
                        }

                    if (res == TRUE)
                        {
                            meta->size = get_guint64_from_string(params[6]);

                            meta->owner = get_substring_from_string(params[7], FALSE);
                            meta->group = get_substring_from_string(params[8], FALSE);
                            meta->link =  get_substring_from_string(params[12], TRUE);

                            meta->uid = get_uint_from_string(params[9]);
                            meta->gid = get_uint_from_string(params[10]);
                            //q_uid = get_uint_from_string(query->uid);
                            //q_gid = get_uint_from_string(query->gid);

                            //if (strcmp(meta->owner, query->owner) == 0 && strcmp(meta->group, query->group) == 0 && (meta->uid == q_uid) && (meta->gid == q_gid))

                            meta->hash_data_list = make_hash_data_list_from_string(params[13]);

                            /* This debug message has no text to be translated */
                            print_debug("file_backend: --> type %d, inode: %"G_GUINT64_FORMAT", mode: %d, atime: %"G_GUINT64_FORMAT", ctime: %"G_GUINT64_FORMAT", mtime: %"G_GUINT64_FORMAT", size: %"G_GUINT64_FORMAT", filename: %s, owner: %s, group: %s, uid: %d, gid: %d, link: %s\n", meta->file_type, meta->inode, meta->mode, meta->atime, meta->ctime, meta->mtime, meta->size, meta->name, meta->owner, meta->group, meta->uid, meta->gid, meta->link);

                        }
                    else
                        {
                             free_meta_data_t(meta, TRUE);
                             meta = NULL;
                        }
                }
                else if(g_regex_match(a_regex, filename, 0, NULL) && (query->reduced == TRUE))
                {
                                    meta = new_meta_data_t();

                                    meta->name = filename;
                                    meta->file_type = get_uint_from_string(params[0]);
                                    meta->mtime = get_guint64_from_string(params[5]);

                                    res = compare_mtime_to_date(meta->mtime, query->date);

                                    /** @todo gain speed in comparison by transforming query->afterdate and query->beforedate only once
                                     *        in the calling function
                                     */
                                    if (query->afterdate != NULL)
                                        {
                                            res = res && compare_after_before_date(meta->mtime, query->afterdate, TRUE);
                                        }

                                    if (query->beforedate != NULL)
                                        {
                                            res = res && compare_after_before_date(meta->mtime, query->beforedate, FALSE);
                                        }

                                    if (res == TRUE)
                                        {
                                        meta->size = get_guint64_from_string(params[6]);
                                        /* This debug message has no text to be translated */
                                        print_debug("file_backend: --> type %d, inode: %"G_GUINT64_FORMAT", mode: %d, atime: %"G_GUINT64_FORMAT", ctime: %"G_GUINT64_FORMAT", mtime: %"G_GUINT64_FORMAT", size: %"G_GUINT64_FORMAT", filename: %s, owner: %s, group: %s, uid: %d, gid: %d, link: %s\n", meta->file_type, meta->inode, meta->mode, meta->atime, meta->ctime, meta->mtime, meta->size, meta->name, meta->owner, meta->group, meta->uid, meta->gid, meta->link);

                                        }
                                    else
                                        {
                                             free_meta_data_t(meta, TRUE);
                                             meta = NULL;
                                        }
                }
            else
                {
                    free_variable(filename);
                }

            g_strfreev(params);
        }

    return meta;
}


/**
 * @param stream is an opened file stream of a plain meta data flat file.
 * @param a_regex is a GRegex * regular expression.
 * @param query is the structure that contains everything about the user
 *        requested query.
 * @returns the list of files from the flat file containing all meta data.
 */
static GList *get_file_list_from_regex_and_query(GFileInputStream *stream, GRegex *a_regex, query_t *query)
{
    buffer_t *a_buffer = NULL;
    gchar *line = NULL;
    meta_data_t *meta = NULL;
    GList *file_list = NULL;

    a_buffer = init_buffer_structure(stream);
    read_one_buffer(a_buffer);

    do
        {
            line = extract_one_line_from_buffer(a_buffer);

            if (a_buffer->size != 0)
                {
                    meta = extract_from_line(line, a_regex, query);

                    if (meta != NULL)
                        {
                            file_list = g_list_prepend(file_list, meta);
                        }
                }

            free_variable(line);
        }
    while (a_buffer->size != 0);

    free_buffer_t(a_buffer);

    return file_list;
}


/**
 * Gets the list of all saved files.
 * @param server_struct is the structure that contains all data for the
 *        server.
 * @param query is the structure that contains everything about the
 *        requested query.
 * @returns a JSON string containing all filenames requested
 * @note g_str_match_string is only available since glib 2.40 and
 *       travis-ci.org has an older version of it!
 */
gchar *file_get_list_of_files(server_struct_t *server_struct, query_t *query)
{
    gchar *filename = NULL;
    file_backend_t *file_backend = NULL;
    GFile *the_file = NULL;
    GFileInputStream *stream = NULL;
    GError *error = NULL;
    json_t *array = NULL;
    json_t *root = NULL;
    gchar *json_string = NULL;
    GRegex *a_regex = NULL;
    GList *file_list = NULL;


    if (server_struct != NULL && server_struct->backend != NULL &&  server_struct->backend->user_data != NULL && query != NULL)
        {
            print_debug(_("file_backend: filter is: %s && %s && %s && %s\n"), query->filename, query->date, query->afterdate, query->beforedate);

            a_regex = g_regex_new(query->filename, G_REGEX_CASELESS, 0, &error);

            file_backend = server_struct->backend->user_data;
            filename =  g_build_filename(file_backend->prefix, "meta", query->hostname, NULL);
            the_file = g_file_new_for_path(filename);

            print_debug(_("file_backend: Reading in %s\n"), filename);

            stream = g_file_read(the_file, NULL, &error);

            if (stream != NULL)
                {
                    /* Requesting a list of files corresponding to a_regex and query */
                    file_list = get_file_list_from_regex_and_query(stream, a_regex, query);

                    /* Closing file input stream */
                    g_input_stream_close((GInputStream *) stream, NULL, &error);

                    /* Sorting the list. As explained in Glib doc, it may be
                     * quicker to add elements to the list by prepending them
                     * and then sorting the list once. */
                    file_list = g_list_sort(file_list, compare_meta_data_t);

                    /* Filtering  */
                    if (query->latest == TRUE)
                        {
                            file_list = keep_latests_meta_data_t_in_list(file_list);
                        }

                    /* Converting list into JSON array */
                    array = convert_meta_data_list_to_json_array(file_list, query->hostname, FALSE);

                    /* Freeing memory */
                    g_list_free_full(file_list, free_glist_meta_data_t);
                }
            else
                {
                     print_error(__FILE__, __LINE__, _("Error: unable to open file %s to read data from it.\n"), filename);
                }

            free_variable(filename);
            g_regex_unref(a_regex);
        }
    else
        {
            print_debug(_("file_backend: Something is wrong with backend initialization!\n"));
        }

    root = json_object();
    insert_json_value_into_json_root(root, "file_list", array);
    json_string = json_dumps(root, 0);

    json_decref(array);
    json_decref(root);

    return json_string;
}


/**
 * Retrieves data from a flat file. The file is named by its hash in hex
 * representation (one should easily check that the sha256sum of such a
 * file gives its name !).
 * @param server_struct is the server's main structure where all
 *        informations needed by the program are stored.
 * @param hex_hash is a gchar * hash in hexadecimal format as retrieved
 *        from the url.
 */
hash_data_t *file_retrieve_data(server_struct_t *server_struct, gchar *hex_hash)
{
    GFile *data_file = NULL;
    gchar *filename = NULL;
    GFileInputStream *stream = NULL;
    GError *error = NULL;
    gssize size_read = 0;
    gchar *string_read = NULL;
    gchar *path = NULL;
    gchar *prefix = NULL;
    file_backend_t *file_backend = NULL;
    hash_data_t *hash_data = NULL;
    guchar *data = NULL;
    guint8 *hash = NULL;
    guint64 filesize = 0;
    gshort cmptype = 0;
    gssize uncmplen = 0;


    if (server_struct != NULL && server_struct->backend != NULL && server_struct->backend->user_data != NULL)
        {
            file_backend = server_struct->backend->user_data;
            prefix = g_build_filename((gchar *) file_backend->prefix, "data", NULL);
            hash = string_to_hash(hex_hash);
            path = make_path_from_hash(prefix, hash, file_backend->level);
            filename = build_filename_from_hash(path, hex_hash, file_backend->level);
            cmptype = get_cmptype_from_file_meta(filename);
            data_file = g_file_new_for_path(filename);
            stream = g_file_read(data_file, NULL, &error);

            if (stream != NULL)
                {
                    filesize = get_file_size(data_file);
                    /* we can do this because files here are blocks and
                     * may not be too big: as large as the biggest CLIENT_BUFFER_SIZE. */
                    data = (guchar *) g_malloc(filesize + 1);   /* No need to do g_malloc0  because data is binary data */

                    size_read = g_input_stream_read((GInputStream *) stream, data, filesize, NULL, &error);

                    if (error != NULL)
                        {
                            string_read = g_strdup_printf("%"G_GSSIZE_FORMAT, size_read);
                            print_error(__FILE__, __LINE__, _("Error: unable to read from file %s (%s bytes read): %s.\n"), filename, string_read, error->message);
                            free_variable(string_read);
                            free_error(error);
                        }
                    else
                        {
                            /* We need to know the compression type directly from the stored filename */
                            uncmplen = get_uncmplen_from_file_meta(filename);

                            /* see retreive_data() in server.c */
                            hash_data = new_hash_data_t_as_is(data, size_read, hash, cmptype, uncmplen);
                        }

                    g_input_stream_close((GInputStream *) stream, NULL, &error);
                    g_object_unref(stream);
                }
            else
                {
                     print_error(__FILE__, __LINE__, _("Error: unable to open file %s to read data from it.\n"), filename);
                }

            free_object(data_file);
            free_variable(filename);
            free_variable(path);
            free_variable(prefix);
        }

    return hash_data;
}
