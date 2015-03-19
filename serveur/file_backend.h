/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *    file_backend.h
 *    This file is part of "Sauvegarde" project.
 *
 *    (C) Copyright 2015 Olivier Delhomme
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
 * @file serveur/file_backend.h
 *
 * This file contains all definition for the functions for the file backend
 * system.
 */

/**
 * Stores meta data into a flat file. A file is created for each host that
 * sends meta datas. This code is not thread safe (it means that this is
 * not safe to call it from different threads unless some mechanism
 * garantees that a write will never occur in the same file at the same
 * time.
 * @param serveur_struct is the serveur main structure where all
 *        informations needed by the program are stored.
 * @param smeta the serveur structure for file meta data. It contains the
 *        hostname that sent it. This structure MUST be freed by this
 *        function.
 */
extern void file_store_smeta(serveur_struct_t *serveur_struct, serveur_meta_data_t *smeta);


/**
 * Inits the backend : takes care of the directories we want to write to.
 * @param serveur_struct is the serveur main structure where all
 *        informations needed by the program are stored.
 */
extern void file_init_backend(serveur_struct_t *serveur_struct);


/**
 * Stores data into a flat file. The file is named by its hash in hex
 * representation (one should easily check that the sha256sum of such a
 * file gives its name !).
 * @param serveur_struct is the serveur main structure where all
 *        informations needed by the program are stored.
 * @param hash_data is a hash_data_t * structure that contains the hash and
 *        the corresponding data in a binary form and a 'read' field that
 *        contains the number of bytes in 'data' field.
 */
extern void file_store_data(serveur_struct_t *serveur_struct, hash_data_t *hash_data);


/**
 * Builds a list of hashs that serveur's server need.
 * @param serveur_struct is the serveur main structure where all
 *        informations needed by the program are stored.
 * @param hash_list is the list of hashs that we have to check for.
 */
extern GSList *build_needed_hash_list(serveur_struct_t *serveur_struct, GSList *hash_list);