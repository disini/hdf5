/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * This file contains private information about the H5VL module
 */
#ifndef _H5VLiod_common_H
#define _H5VLiod_common_H

#include "H5VLpublic.h"
#include "H5VLiod.h"         /* Iod VOL plugin			*/

#define NA_UNDEFINED NULL

/* types for different ways that objects are located in an HDF5 container */
fs_id_t H5VL_EFF_INIT_ID;
fs_id_t H5VL_EFF_FINALIZE_ID;
fs_id_t H5VL_FILE_CREATE_ID;
fs_id_t H5VL_FILE_OPEN_ID;
fs_id_t H5VL_FILE_FLUSH_ID;
fs_id_t H5VL_FILE_CLOSE_ID;
fs_id_t H5VL_GROUP_CREATE_ID;
fs_id_t H5VL_GROUP_OPEN_ID;
fs_id_t H5VL_GROUP_CLOSE_ID;
fs_id_t H5VL_DSET_CREATE_ID;
fs_id_t H5VL_DSET_OPEN_ID;
fs_id_t H5VL_DSET_READ_ID;
fs_id_t H5VL_DSET_WRITE_ID;
fs_id_t H5VL_DSET_SET_EXTENT_ID;
fs_id_t H5VL_DSET_CLOSE_ID;

/* struct that contains the information about the IOD container */
typedef struct H5VL_iod_remote_file_t {
    iod_handle_t coh;
    iod_handle_t root_oh;
    iod_obj_id_t root_id;
    iod_handle_t scratch_oh;
    iod_obj_id_t scratch_id;
    hid_t fcpl_id;
    fs_handle_t fs_handle;
} H5VL_iod_remote_file_t;

/* struct that contains the information about the IOD group */
typedef struct H5VL_iod_remote_group_t {
    iod_handle_t iod_oh;
    iod_obj_id_t iod_id;
    iod_handle_t scratch_oh;
    iod_obj_id_t scratch_id;
    hid_t gcpl_id;
    fs_handle_t fs_handle;
} H5VL_iod_remote_group_t;

/* struct that contains the information about the IOD dset */
typedef struct H5VL_iod_remote_dset_t {
    iod_handle_t iod_oh;
    iod_obj_id_t iod_id;
    iod_handle_t scratch_oh;
    iod_obj_id_t scratch_id;
    hid_t dcpl_id;
    hid_t space_id;
    hid_t type_id;
    fs_handle_t fs_handle;
} H5VL_iod_remote_dset_t;

typedef struct H5VL_iod_file_create_input_t {
    const char *name;
    unsigned flags;
    hid_t fcpl_id;
    hid_t fapl_id;
    fs_handle_t fs_handle;
} H5VL_iod_file_create_input_t;

typedef struct H5VL_iod_file_open_input_t {
    const char *name;
    unsigned flags;
    hid_t fapl_id;
    fs_handle_t fs_handle;
} H5VL_iod_file_open_input_t;

typedef struct H5VL_iod_file_flush_input_t {
    iod_handle_t coh;
    H5F_scope_t scope;
    fs_handle_t fs_handle;
} H5VL_iod_file_flush_input_t;

typedef struct H5VL_iod_group_create_input_t {
    iod_handle_t coh;
    iod_handle_t loc_oh;
    iod_obj_id_t loc_id;
    const char *name;
    hid_t gcpl_id;
    hid_t gapl_id;
    hid_t lcpl_id;
    fs_handle_t fs_handle;
} H5VL_iod_group_create_input_t;

typedef struct H5VL_iod_group_open_input_t {
    iod_handle_t coh;
    iod_handle_t loc_oh;
    iod_obj_id_t loc_id;
    const char *name;
    hid_t gapl_id;
    fs_handle_t fs_handle;
} H5VL_iod_group_open_input_t;

typedef struct H5VL_iod_dset_create_input_t {
    iod_handle_t coh;
    iod_handle_t loc_oh;
    iod_obj_id_t loc_id;
    const char *name;
    hid_t dcpl_id;
    hid_t dapl_id;
    hid_t lcpl_id;
    hid_t type_id;
    hid_t space_id;
    fs_handle_t fs_handle;
} H5VL_iod_dset_create_input_t;

typedef struct H5VL_iod_dset_open_input_t {
    iod_handle_t coh;
    iod_handle_t loc_oh;
    iod_obj_id_t loc_id;
    const char *name;
    hid_t dapl_id;
    fs_handle_t fs_handle;
} H5VL_iod_dset_open_input_t;

typedef struct H5VL_iod_dset_io_input_t {
    iod_handle_t iod_oh;
    iod_handle_t scratch_oh;
    hid_t space_id;
    hid_t dxpl_id;
    uint32_t checksum;
    bds_handle_t bds_handle;
    fs_handle_t fs_handle;
} H5VL_iod_dset_io_input_t;

typedef struct H5VL_iod_dset_set_extent_input_t {
    iod_handle_t iod_oh;
    int rank;
    hsize_t *size;
    fs_handle_t fs_handle;
} H5VL_iod_dset_set_extent_input_t;

typedef struct H5VL_iod_read_status_t {
    int ret;
    uint32_t cs;
} H5VL_iod_read_status_t;

#if 0
/* Define fs_proc_iod_handle_t */
static inline int fs_proc_iod_handle_t(fs_proc_t proc, void *data)
{
    int ret = S_SUCCESS;
    iod_handle_t *struct_data = (iod_handle_t *)data;

    ret = fs_proc_uint64_t(proc, &struct_data->cookie);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    return ret;
}

/* Define fs_proc_iod_obj_id_t */
static inline int fs_proc_iod_obj_id_t(fs_proc_t proc, void *data)
{
    int ret = S_SUCCESS;
    iod_obj_id_t *struct_data = (iod_obj_id_t *)data;

    ret = fs_proc_uint64_t(proc, &struct_data->oid_hi);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }

    ret = fs_proc_uint64_t(proc, &struct_data->oid_lo);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }

    return ret;
}

/* Define fs_proc_iod_handle_t */
static inline int fs_proc_plist_t(fs_proc_t proc, void *data)
{
    int ret = S_SUCCESS;
    size_t plist_size = 0;
    H5P_genplist_t *plist = NULL;
    void *buf = NULL;
    hid_t plist_id = *((hid_t *)data);

    if(NULL == (plist = (H5P_genplist_t *)H5I_object_verify(plist_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
    if((ret = H5P__encode(plist, FALSE, NULL, &plist_size)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "unable to encode property list");
    buf = H5MM_malloc(plist_size);
    if((ret = H5P__encode(plist, FALSE, buf, &plist_size)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "unable to encode property list");


    ret = fs_proc_memcpy(proc, buf, plist_size);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }

    H5MM_xfree(buf);
    return ret;
}

static inline int fs_proc_remote_file_t(fs_proc_t proc, void *data)
{
    int ret = S_SUCCESS;
    H5VL_iod_remote_file_t *struct_data = (H5VL_iod_remote_file_t *) data;

    ret = fs_proc_iod_handle_t(proc, &struct_data->coh);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_handle_t(proc, &struct_data->root_oh);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_obj_id_t(proc, &struct_data->root_id);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_handle_t(proc, &struct_data->scratch_oh);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_obj_id_t(proc, &struct_data->scratch_id);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    return ret;
}

static inline int fs_proc_remote_object_t(fs_proc_t proc, void *data)
{
    int ret = S_SUCCESS;
    H5VL_iod_remote_group_t *struct_data = (H5VL_iod_remote_group_t *) data;

    ret = fs_proc_iod_handle_t(proc, &struct_data->iod_oh);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_handle_t(proc, &struct_data->iod_id);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_handle_t(proc, &struct_data->scratch_oh);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    ret = fs_proc_iod_obj_id_t(proc, &struct_data->scratch_id);
    if (ret != S_SUCCESS) {
        S_ERROR_DEFAULT("Proc error");
        ret = S_FAIL;
        return ret;
    }
    return ret;
}
#endif

#endif /* _H5VLiod_common_H */
