/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *    query.h
 *    This file is part of "Sauvegarde" project.
 *
 *    (C) Copyright 2015 - 2017 Olivier Delhomme
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
 * @file query.h
 *
 *  This file contains all the definitions needed by queries
 */
#ifndef _QUERY_H_
#define _QUERY_H_

/**
 * @struct query_t
 * @brief This structure is used to pass parameters to query functions.
 * Fields of this structure may be searched into the selected backend.
 */
typedef struct
{
    gchar *hostname;
    gchar *uid;
    gchar *gid;
    gchar *owner;
    gchar *group;
    gchar *filename;
    gchar *date;
    gchar *afterdate;
    gchar *beforedate;
    gboolean latest;    /**< latest: True if we only want to get the latest entries found */
    gboolean reduced;
} query_t;


/**
 * Creates a query_t * structure filled with the corresponding data
 * @param hostname hostname (where to look for)
 * @param uid uid for the file(s)
 * @param gid gid for the file(s)
 * @param owner owner for the file(s) hopefully corresponding to uid
 * @param group group for the file(s) hopefully corresponding to gid
 * @param filename is the filename that we are looking for.
 * @param date is the date at which we want the file's version back.
 * @param afterdate is the date after which we want the files.
 * @param beforedate is the date before which we want the files.
 * @returns a newly allocated query_t * structure filled  with the
 *          corresponding data that may be freed when no longer needed.
 */
extern query_t *init_query_t(gchar *hostname, gchar *uid, gchar *gid, gchar *owner, gchar *group, gchar *filename, gchar *date, gchar *afterdate, gchar *beforedate, gboolean latest, gboolean reduced);


/**
 * Frees a query
 * @param query is the qery to be freed
 * @returns NULL;
 */
extern void free_query_t(query_t *query);

#endif /* #ifndef _QUERY_H_ */
