/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *    communique.c
 *    This file is part of "Sauvegarde" project.
 *
 *    (C) Copyright 2014 - 2015 Olivier Delhomme
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
 * @file communique.c
 * This file contains every call to the zmq library in order to make a
 * wrapper to this library
 */

#include "libsauvegarde.h"

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);
static void send_datas_from_hash_list(comm_t *comm, hashs_t *hashs, GSList *hash_list);


/**
 * Gets the version for the communication library
 * @returns a newly allocated string that contains the version and that
 *          may be freed with free_variable() when no longer needed.
 */
gchar *get_communication_library_version(void)
{
    curl_version_info_data *data = NULL;

    data = curl_version_info(CURLVERSION_NOW);

    return g_strdup_printf(_("\t. LIBCURL version : %s\n"), data->version);
}


/**
 * Used by libcurl to retrieve informations
 * @param buffer is the buffer where received data are written by libcurl
 * @param size is the size of an element in buffer
 * @param nmemb is the number of elements in buffer
 * @param userp is a user pointer and MUST be a pointer to comm_t *
 *        structure
 * @returns should return the size of the data taken into account.
 *          Everything different from the size passed to this function is
 *          considered as an error by libcurl.
 */
static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    comm_t *comm = (comm_t *) userp;
    gchar *buf1 = NULL;
    gchar *concat = NULL;

    if (comm->seq == 0)
        {
            comm->buffer = g_strndup(buffer, size * nmemb);
        }
    else
        {
            buf1 = g_strndup(buffer, size * nmemb);
            concat = g_strdup_printf("%s%s", comm->buffer, buf1);
            free_variable(buf1);
            free_variable(comm->buffer);
            comm->buffer = concat;
        }

    comm->seq = comm->seq + 1;

    return (size * nmemb);
}


/**
 * Uses curl to send a GET command to the http url
 * @param comm a comm_t * structure that must contain an initialized
 *        curl_handle (must not be NULL)
 * @param url a gchar * url where to send the command to. It must NOT
 *        contain the http://ip:port string. And must contain the first '/'
 *        ie to get 'http://127.0.0.1:5468/Version' url must be '/Version'.
 * @returns a CURLcode (http://curl.haxx.se/libcurl/c/libcurl-errors.html)
 *          CURLE_OK upon success, any other error code in any other
 *          situation. When CURLE_OK is returned, the datas that the server
 *          sent is in the comm->buffer gchar * string.
 */
gint get_url(comm_t *comm, gchar *url)
{
    gint success = CURLE_FAILED_INIT;
    gchar *real_url = NULL;

    if (comm != NULL && url != NULL && comm->curl_handle != NULL && comm->conn != NULL)
        {
            comm->seq = 0;
            real_url = g_strdup_printf("%s%s", comm->conn, url);

            curl_easy_setopt(comm->curl_handle, CURLOPT_URL, real_url);
            curl_easy_setopt(comm->curl_handle, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(comm->curl_handle, CURLOPT_WRITEDATA, comm);

            success = curl_easy_perform(comm->curl_handle);

            real_url = free_variable(real_url);

            if (success == CURLE_OK && comm->buffer != NULL)
                {
                    print_debug("Answer is: \"%s\"\n", comm->buffer);
                }
            else
                {
                    if (comm->buffer != NULL)
                        {
                            comm->buffer = free_variable(comm->buffer);
                        }

                    print_error(__FILE__, __LINE__, _("Error while sending GET command and receiving datas\n"));
                }
        }

    return success;
}


/**
 * Uses curl to send a POST command to the http server url
 * @param comm a comm_t * structure that must contain an initialized
 *        curl_handle (must not be NULL). buffer field of this structure
 *        is sent as data in the POST command.
 * @param url a gchar * url where to send the command to. It must NOT
 *        contain the http://ip:port string. And must contain the first '/'
 *        ie to get 'http://127.0.0.1:5468/Version' url must be '/Version'.
 * @returns a CURLcode (http://curl.haxx.se/libcurl/c/libcurl-errors.html)
 *          CURLE_OK upon success, any other error code in any other
 *          situation. When CURLE_OK is returned, the datas that the server
 *          sent is in the comm->buffer gchar * string.
 * @todo . free some memory where needed
 *       . manage errors codes
 */
gint post_url(comm_t *comm, gchar *url)
{
    gint success = CURLE_FAILED_INIT;
    gchar *real_url = NULL;
    gchar *buffer = NULL;

    if (comm != NULL && url != NULL && comm->curl_handle != NULL && comm->conn != NULL && comm->buffer != NULL)
        {
            comm->seq = 0;

            real_url = g_strdup_printf("%s%s", comm->conn, url);
            buffer = g_strdup(comm->buffer);

            curl_easy_setopt(comm->curl_handle, CURLOPT_POSTFIELDS, buffer);
            curl_easy_setopt(comm->curl_handle, CURLOPT_URL, real_url);
            curl_easy_setopt(comm->curl_handle, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(comm->curl_handle, CURLOPT_WRITEDATA, comm);

            success = curl_easy_perform(comm->curl_handle);

            if (success != CURLE_OK)
                {
                    print_error(__FILE__, __LINE__, _("Error while sending POST command (to \"%s\") with datas\n"), real_url);
                }
            else if (comm->buffer != NULL)
                {
                    print_debug("Answer is: \"%s\"\n", comm->buffer); /** @todo  Not sure that we will need this information later */
                }

            real_url = free_variable(real_url);
        }

    return success;
}


/**
 * This functions iters over a GSList of hashs searching into the binary
 * tree if we have the datas and then sends them to serveur server.
 * @param
 */
static void send_datas_from_hash_list(comm_t *comm, hashs_t *hashs, GSList *hash_list)
{
    data_t *a_data = NULL;
    GSList *head = hash_list;
    gint success = CURLE_FAILED_INIT;

    while (hash_list != NULL)
        {
            a_data = g_tree_lookup(hashs->tree_hash, hash_list->data);

            if (a_data != NULL)
                {

                    print_debug(_("Sending datas for hash: \"%s\"\n"), g_base64_encode(hash_list->data, HASH_LEN));
                    comm->buffer = convert_data_to_json(a_data, hash_list->data);
                    success = post_url(comm, "/Data.json");

                    if (success == CURLE_OK)
                        {
                            a_data->buffer = free_variable(a_data->buffer);
                            a_data->read = 0;
                            a_data->into_cache = FALSE;
                        }
                }
            else
                {
                    print_error(__FILE__, __LINE__, "Error, some data may be missing : unable to find datas for hash: \"%s\"\n",  g_base64_encode(hash_list->data, HASH_LEN));
                }

            free_variable(hash_list->data);

            hash_list = g_slist_next(hash_list);
        }

    g_slist_free(head);
}


/**
 * This function sends the datas that corresponds to the hashs in the json
 * formatted string's answer.
 * @param comm a comm_t * structure that must contain an initialized
 *             curl_handle (must not be NULL). Buffer field of this
 *             structure is sent as data in the POST command.
 * @param hashs is the hash structure that contains the binary tree.
 * @param answer is the answer of the serveur containing a json formatted
 *        hash list.
 */
void send_datas_to_server(comm_t *comm, hashs_t *hashs, gchar *answer)
{
    json_t *root = NULL;
    GSList *hash_list = NULL;

    if (hashs != NULL && answer != NULL)
        {
            root = load_json(answer);

            if (root != NULL)
                {

                    hash_list = extract_gslist_from_array(root, "hash_list");

                    send_datas_from_hash_list(comm, hashs, hash_list);

                    json_decref(root);
                }
        }
}


/**
 * Creates a new communication comm_t * structure.
 * @param conn a gchar * connection string that should be some url like
 *        string : http://ip:port or http://servername:port
 * @returns a newly allocated comm_t * structure where sender and receiver
 *          are set to NULL.
 */
comm_t *init_comm_struct(gchar *conn)
{
    comm_t *comm = NULL;

    comm = (comm_t *) g_malloc0(sizeof(comm_t));

    comm->curl_handle = curl_easy_init();
    comm->buffer = NULL;
    comm->conn = conn;

    return comm;
}

