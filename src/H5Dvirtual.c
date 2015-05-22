/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
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
 * Programmer:  Neil Fortner <nfortne2@hdfgroup.org>
 *              Wednesday, January 28, 2015
 *
 * Purpose:
 *      VDSINC
 */

/****************/
/* Module Setup */
/****************/

#define H5D_PACKAGE             /* Suppress error about including H5Dpkg */


/***********/
/* Headers */
/***********/
#include "H5private.h"          /* Generic Functions                    */
#include "H5Dpkg.h"             /* Dataset functions                    */
#include "H5Eprivate.h"         /* Error handling                       */
#include "H5Fprivate.h"         /* Files                                */
#include "H5FLprivate.h"        /* Free Lists                           */
#include "H5Gprivate.h"         /* Groups                               */
#include "H5HGprivate.h"        /* Global Heaps                         */
#include "H5Iprivate.h"         /* IDs                                  */
#include "H5MMprivate.h"        /* Memory management                    */
#include "H5Oprivate.h"         /* Object headers                       */
#include "H5Sprivate.h"         /* Dataspaces                           */


/****************/
/* Local Macros */
/****************/

/* Default size for sub_dset array */
#define H5D_VIRTUAL_DEF_SUB_DSET_SIZE 128


/******************/
/* Local Typedefs */
/******************/


/********************/
/* Local Prototypes */
/********************/

/* Layout operation callbacks */
static herr_t H5D__virtual_io_init(const H5D_io_info_t *io_info,
    const H5D_type_info_t *type_info, hsize_t nelmts, const H5S_t *file_space,
    const H5S_t *mem_space, H5D_chunk_map_t *cm);
static herr_t H5D__virtual_read(H5D_io_info_t *io_info, const H5D_type_info_t
    *type_info, hsize_t nelmts, const H5S_t *file_space, const H5S_t *mem_space,
    H5D_chunk_map_t *fm);
static herr_t H5D__virtual_write(H5D_io_info_t *io_info,
    const H5D_type_info_t *type_info, hsize_t nelmts, const H5S_t *file_space,
    const H5S_t *mem_space, H5D_chunk_map_t *fm);
static herr_t H5D__virtual_flush(H5D_t *dset, hid_t dxpl_id);

/* Other functions */
static herr_t H5D__virtual_open_source_dset(const H5D_t *vdset,
    H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset, hid_t dxpl_id);
static herr_t H5D__virtual_reset_source_dset(
    H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset);
static herr_t H5D__virtual_str_append(const char *src, size_t src_len, char **p,
    char **buf, size_t *buf_size);
static herr_t H5D__virtual_copy_parsed_name(
    H5O_storage_virtual_name_seg_t **dst, H5O_storage_virtual_name_seg_t *src);
static herr_t H5D__virtual_build_source_name(char *source_name,
    const H5O_storage_virtual_name_seg_t *parsed_name, size_t static_strlen,
    size_t nsubs, hsize_t blockno, char **built_name);
static herr_t H5D__virtual_read_one(H5D_io_info_t *io_info,
    const H5D_type_info_t *type_info, const H5S_t *file_space,
    const H5S_t *mem_space, H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset);
static herr_t H5D__virtual_write_one(H5D_io_info_t *io_info,
    const H5D_type_info_t *type_info, const H5S_t *file_space,
    const H5S_t *mem_space, H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset);


/*********************/
/* Package Variables */
/*********************/

/* Contiguous storage layout I/O ops */
const H5D_layout_ops_t H5D_LOPS_VIRTUAL[1] = {{
    NULL,
    H5D__virtual_init,
    H5D__virtual_is_space_alloc,
    H5D__virtual_io_init,
    H5D__virtual_read,
    H5D__virtual_write,
#ifdef H5_HAVE_PARALLEL
    H5D__virtual_collective_read, //VDSINC
    H5D__virtual_collective_write, //VDSINC
#endif /* H5_HAVE_PARALLEL */
    NULL,
    NULL,
    H5D__virtual_flush,
    NULL
}};


/*******************/
/* Local Variables */
/*******************/

/* Declare a free list to manage the H5O_storage_virtual_name_seg_t struct */
H5FL_DEFINE(H5O_storage_virtual_name_seg_t);



/*-------------------------------------------------------------------------
 * Function:    H5D_virtual_update_min_dims
 *
 * Purpose:     Updates the virtual layout's "min_dims" field to take into
 *              account the "idx"th entry in the mapping list.  The entry
 *              must be complete, though top level fields list_nused does
 *              (and of course min_dims) do not need to take it into
 *              account.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 10, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D_virtual_update_min_dims(H5O_layout_t *layout, size_t idx)
{
    H5S_sel_type    sel_type;
    int             rank;
    hsize_t         bounds_start[H5S_MAX_RANK];
    hsize_t         bounds_end[H5S_MAX_RANK];
    int             i;
    herr_t          ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    HDassert(layout);
    HDassert(layout->type == H5D_VIRTUAL);
    HDassert(idx < layout->storage.u.virt.list_nalloc);

    /* Get type of selection */
    if(H5S_SEL_ERROR == (sel_type = H5S_GET_SELECT_TYPE(layout->storage.u.virt.list[idx].source_dset.virtual_select)))
        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to get selection type")

    /* Do not update min_dims for "all" or "none" selections */
    if((sel_type == H5S_SEL_ALL) || (sel_type == H5S_SEL_NONE))
        HGOTO_DONE(SUCCEED)

    /* Get rank of vspace */
    if((rank = H5S_GET_EXTENT_NDIMS(layout->storage.u.virt.list[idx].source_dset.virtual_select)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to get number of dimensions")

    /* Get selection bounds */
    if(H5S_SELECT_BOUNDS(layout->storage.u.virt.list[idx].source_dset.virtual_select, bounds_start, bounds_end) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to get selection bounds")

    /* Update min_dims */
    for(i = 0; i < rank; i++)
        /* Don't check unlimited dimensions in the selection */
        if((i != layout->storage.u.virt.list[idx].unlim_dim_virtual)
                && (bounds_end[i] >= layout->storage.u.virt.min_dims[i]))
            layout->storage.u.virt.min_dims[i] = bounds_end[i] + (hsize_t)1;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D_virtual_update_min_dims() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_copy_layout
 *
 * Purpose:     Deep copies virtual storage layout message in memory.
 *              This function assumes that the top-level struct has
 *              already been copied (so the source struct retains
 *              ownership of the fields passed to this function).
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 10, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D__virtual_copy_layout(H5O_layout_t *layout)
{
    H5O_storage_virtual_ent_t *orig_list = NULL;
    size_t          i;
    herr_t          ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    HDassert(layout);
    HDassert(layout->type == H5D_VIRTUAL);

    if(layout->storage.u.virt.list_nused > 0) {
        HDassert(layout->storage.u.virt.list);

        /* Save original entry list for use as the "source" */
        orig_list = layout->storage.u.virt.list;

        /* Allocate memory for the list */
        if(NULL == (layout->storage.u.virt.list = (H5O_storage_virtual_ent_t *)H5MM_calloc(layout->storage.u.virt.list_nused * sizeof(H5O_storage_virtual_ent_t))))
            HGOTO_ERROR(H5E_DATASET, H5E_NOSPACE, FAIL, "unable to allocate memory for virtual dataset entry list")
        layout->storage.u.virt.list_nalloc = layout->storage.u.virt.list_nused;

        /* Copy the list entries, though set source_dset.dset and sub_dset to
         * NULL */
        for(i = 0; i < layout->storage.u.virt.list_nused; i++) {
            if(NULL == (layout->storage.u.virt.list[i].source_dset.file_name
                    = HDstrdup(orig_list[i].source_dset.file_name)))
                HGOTO_ERROR(H5E_DATASET, H5E_RESOURCE, FAIL, "unable to duplicate source file name")
            if(NULL == (layout->storage.u.virt.list[i].source_dset.dset_name
                    = HDstrdup(orig_list[i].source_dset.dset_name)))
                HGOTO_ERROR(H5E_DATASET, H5E_RESOURCE, FAIL, "unable to duplicate source dataset name")
            if(NULL == (layout->storage.u.virt.list[i].source_dset.virtual_select
                    = H5S_copy(orig_list[i].source_dset.virtual_select, FALSE, TRUE)))
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to copy virtual selection")
            if(NULL == (layout->storage.u.virt.list[i].source_select
                    = H5S_copy(orig_list[i].source_select, FALSE, TRUE)))
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to copy source selection")
            if(H5D__virtual_copy_parsed_name(&layout->storage.u.virt.list[i].parsed_source_file_name, orig_list[i].parsed_source_file_name) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to copy parsed source file name")
            layout->storage.u.virt.list[i].psfn_static_strlen = orig_list[i].psfn_static_strlen;
            layout->storage.u.virt.list[i].psfn_nsubs = orig_list[i].psfn_nsubs;
            if(H5D__virtual_copy_parsed_name(&layout->storage.u.virt.list[i].parsed_source_dset_name, orig_list[i].parsed_source_dset_name) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to copy parsed source dataset name")
            layout->storage.u.virt.list[i].psdn_static_strlen = orig_list[i].psdn_static_strlen;
            layout->storage.u.virt.list[i].psdn_nsubs = orig_list[i].psdn_nsubs;
            layout->storage.u.virt.list[i].unlim_dim_source = orig_list[i].unlim_dim_source;
            layout->storage.u.virt.list[i].unlim_dim_virtual = orig_list[i].unlim_dim_virtual;
            layout->storage.u.virt.list[i].unlim_extent_source = orig_list[i].unlim_extent_source;
            layout->storage.u.virt.list[i].unlim_extent_virtual = orig_list[i].unlim_extent_virtual;
            layout->storage.u.virt.list[i].clip_size_source = orig_list[i].clip_size_source;
            layout->storage.u.virt.list[i].clip_size_virtual = orig_list[i].clip_size_virtual;
            layout->storage.u.virt.list[i].source_space_status = orig_list[i].source_space_status;
            layout->storage.u.virt.list[i].virtual_space_status = orig_list[i].virtual_space_status;
        } /* end for */
    } /* end if */
    else {
        HDassert(0 && "checking code coverage...");//VDSINC
        /* Zero out other fields related to list, just to be sure */
        layout->storage.u.virt.list = NULL;
        layout->storage.u.virt.list_nalloc = 0;
    } /* end else */

done:
    /* Release allocated resources on failure */
    if((ret_value < 0) && orig_list
            && (orig_list != layout->storage.u.virt.list))
        if(H5D__virtual_reset_layout(layout) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CANTFREE, FAIL, "unable to reset virtual layout")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_copy_layout() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_reset_layout
 *
 * Purpose:     Frees internal structures in a virtual storage layout
 *              message in memory.  This function is safe to use on
 *              incomplete structures (for recovery from failure) provided
 *              the internal structures are initialized with all bytes set
 *              to 0.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 11, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D__virtual_reset_layout(H5O_layout_t *layout)
{
    size_t          i, j;
    herr_t          ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    HDassert(layout);
    HDassert(layout->type == H5D_VIRTUAL);

    /* Free the list entries.  Note we always attempt to free everything even in
     * the case of a failure.  Because of this, and because we free the list
     * afterwards, we do not need to zero out the memory in the list. */
    for(i = 0; i < layout->storage.u.virt.list_nused; i++) {
        /* Free source_dset */
        if(H5D__virtual_reset_source_dset(&layout->storage.u.virt.list[i], &layout->storage.u.virt.list[i].source_dset) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CANTFREE, FAIL, "unable to reset source dataset")

        /* Free source_select */
        if(layout->storage.u.virt.list[i].source_select)
            if(H5S_close(layout->storage.u.virt.list[i].source_select) < 0)
                HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release source selection")

        /* Free sub_dset */
        for(j = 0; j < layout->storage.u.virt.list[i].sub_dset_nalloc; j++) {
            //HDassert(0 && "Checking code coverage..."); //VDSINC
            if(H5D__virtual_reset_source_dset(&layout->storage.u.virt.list[i], &layout->storage.u.virt.list[i].sub_dset[j]) < 0)
                HDONE_ERROR(H5E_DATASET, H5E_CANTFREE, FAIL, "unable to reset source dataset")
        } //VDSINC
        layout->storage.u.virt.list[i].sub_dset = (H5O_storage_virtual_srcdset_t *)H5MM_xfree(layout->storage.u.virt.list[i].sub_dset);

        /* Free parsed_source_file_name */
        H5D_virtual_free_parsed_name(layout->storage.u.virt.list[i].parsed_source_file_name);

        /* Free parsed_source_dset_name */
        H5D_virtual_free_parsed_name(layout->storage.u.virt.list[i].parsed_source_dset_name);
    } /* end for */

    /* Free the list */
    layout->storage.u.virt.list = (H5O_storage_virtual_ent_t *)H5MM_xfree(layout->storage.u.virt.list);
    layout->storage.u.virt.list_nalloc = (size_t)0;
    layout->storage.u.virt.list_nused = (size_t)0;
    (void)HDmemset(layout->storage.u.virt.min_dims, 0, sizeof(layout->storage.u.virt.min_dims));

    /* Note the lack of a done: label.  This is because there are no HGOTO_ERROR
     * calls.  If one is added, a done: label must also be added */
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_reset_layout() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_delete
 *
 * Purpose:     Delete the file space for a virtual dataset
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D__virtual_delete(H5F_t *f, hid_t dxpl_id, H5O_storage_t *storage)
{
    herr_t ret_value = SUCCEED;   /* Return value */

    FUNC_ENTER_PACKAGE

    /* check args */
    HDassert(f);
    HDassert(storage);
    HDassert(storage->type == H5D_VIRTUAL);

    /* Delete the global heap block */
    if(H5HG_remove(f, dxpl_id, (H5HG_t *)&(storage->u.virt.serial_list_hobjid)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_WRITEERROR, FAIL, "Unable to remove heap object")

    /* Clear global heap ID in storage */
    storage->u.virt.serial_list_hobjid.addr = HADDR_UNDEF;
    storage->u.virt.serial_list_hobjid.idx = 0;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_delete */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_open_source_dset
 *
 * Purpose:     Attempts to open a source dataset.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              March 6, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_open_source_dset(const H5D_t *vdset,
    H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset, hid_t dxpl_id)
{
    H5F_t       *src_file = NULL;       /* Source file */
    hbool_t     src_file_open = FALSE;  /* Whether we have opened and need to close src_file */
    H5G_loc_t   src_root_loc;           /* Object location of source file root group */
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(vdset);
    HDassert(source_dset);
    HDassert(!source_dset->dset);
    HDassert(source_dset->file_name);
    HDassert(source_dset->dset_name);

    /* Get dapl and fapl from current (virtual dataset) location? VDSINC */
    /* Write code to check if these exist and return without opening dset
     * otherwise VDSINC */

    /* Check if we need to open the source file */
    if(HDstrcmp(source_dset->file_name, ".")) {
        /* Open the source file */
        if(NULL == (src_file = H5F_open(source_dset->file_name, H5F_INTENT(vdset->oloc.file) & H5F_ACC_RDWR, H5P_FILE_CREATE_DEFAULT, H5P_FILE_ACCESS_DEFAULT, dxpl_id)))
            H5E_clear_stack(NULL); //Quick hack until proper support for H5Fopen with missing file is implemented VDSINC HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENFILE, FAIL, "unable to open source file")
        else
            src_file_open = TRUE;
    } /* end if */
    else
        /* Source file is ".", use the virtual dataset's file */
        src_file = vdset->oloc.file;

    if(src_file) {
        /* Set up the root group in the destination file */
        if(NULL == (src_root_loc.oloc = H5G_oloc(H5G_rootof(src_file))))
            HGOTO_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL, "unable to get object location for root group")
        if(NULL == (src_root_loc.path = H5G_nameof(H5G_rootof(src_file))))
            HGOTO_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL, "unable to get path for root group")

        /* Open the source dataset */
        if(NULL == (source_dset->dset = H5D__open_name(&src_root_loc, source_dset->dset_name, H5P_DATASET_ACCESS_DEFAULT, dxpl_id)))
            H5E_clear_stack(NULL); //Quick hack until proper support for H5Fopen with missing file is implemented VDSINC HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENOBJ, FAIL, "unable to open dataset")
        else
            /* Patch the source selection if necessary */
            if(virtual_ent->source_space_status != H5O_VIRTUAL_STATUS_CORRECT) {
                if(H5S_extent_copy(virtual_ent->source_select, source_dset->dset->shared->space) < 0)
                    HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "can't copy source dataspace extent")
                virtual_ent->source_space_status = H5O_VIRTUAL_STATUS_CORRECT;
            } /* end if */
    } /* end if */

done:
    /* Close source file */
    if(src_file_open)
        if(H5F_try_close(src_file) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CANTCLOSEFILE, FAIL, "can't close source file")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_open_source_dset() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_reset_source_dset
 *
 * Purpose:     Frees space reference by a source dataset struct.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 20, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_reset_source_dset(H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset)
{
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(source_dset);

    /* Free dataset */
    if(source_dset->dset) {
        if(H5D_close(source_dset->dset) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to close source dataset")
        source_dset->dset = NULL;
    } /* end if */

    /* Free file name */
    if((source_dset == &virtual_ent->source_dset)
            || virtual_ent->parsed_source_file_name)
        source_dset->file_name = (char *)H5MM_xfree(source_dset->file_name);

    /* Free dataset name */
    if((source_dset == &virtual_ent->source_dset)
            || virtual_ent->parsed_source_dset_name)
        source_dset->dset_name = (char *)H5MM_xfree(source_dset->dset_name);

    /* Free virtual selection */
    if(source_dset->virtual_select) {
        if(H5S_close(source_dset->virtual_select) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release virtual selection")
        source_dset->virtual_select = NULL;
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_reset_source_dset() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_str_append
 *
 * Purpose:     Appends src_len bytes of the string src to the position *p
 *              in the buffer *buf (allocating *buf if necessary).
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 19, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_str_append(const char *src, size_t src_len, char **p, char **buf,
    size_t *buf_size)
{
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(src);
    HDassert(src_len > 0);
    HDassert(p);
    HDassert(buf);
    HDassert(*p >= *buf);
    HDassert(buf_size);

    /* Allocate or extend buffer if necessary */
    if(!*buf) {
        //HDassert(0 && "Checking code coverage..."); //VDSINC
        HDassert(!*p);
        HDassert(*buf_size == 0);

        /* Allocate buffer */
        if(NULL == (*buf = (char *)H5MM_malloc(src_len + (size_t)1)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to allocate name segment struct")
        *buf_size = src_len + (size_t)1;
        *p = *buf;
    } /* end if */
    else if(((size_t)(*p - *buf) + src_len + (size_t)1) > *buf_size) {
        HDassert(0 && "Checking code coverage..."); //VDSINC
        char *tmp_buf;
        size_t tmp_buf_size;

        /* Calculate new size of buffer */
        tmp_buf_size = MAX((size_t)(*p - *buf) + src_len + (size_t)1,
                *buf_size * (size_t)2);

        /* Reallocate buffer */
        if(NULL == (tmp_buf = (char *)H5MM_realloc(*buf, tmp_buf_size)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to reallocate name segment buffer")
        *buf = tmp_buf;
        *buf_size = tmp_buf_size;
    } /* end if */

    /* Copy string to *p.  Note that since src in not NULL terminated, we must
     * use memcpy */
    (void)HDmemcpy(*p, src, src_len);

    /* Advance *p */
    *p += src_len;

    /* Add NULL terminator */
    **p = '\0';

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5D__virtual_str_append() */


/*-------------------------------------------------------------------------
 * Function:    H5D_virtual_parse_source_name
 *
 * Purpose:     Parses a source file or dataset name.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 18, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D_virtual_parse_source_name(const char *source_name,
    H5O_storage_virtual_name_seg_t **parsed_name, size_t *static_strlen,
    size_t *nsubs)
{
    H5O_storage_virtual_name_seg_t *tmp_parsed_name = NULL;
    H5O_storage_virtual_name_seg_t **tmp_parsed_name_p = &tmp_parsed_name;
    size_t      tmp_static_strlen;
    size_t      tmp_strlen;
    size_t      tmp_nsubs = 0;
    const char  *p;
    const char  *pct;
    char        *name_seg_p = NULL;
    size_t      name_seg_size = 0;
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(source_name);
    HDassert(parsed_name);
    HDassert(static_strlen);
    HDassert(nsubs);

    /* Initialize p and tmp_static_strlen */
    p = source_name;
    tmp_static_strlen = tmp_strlen = HDstrlen(source_name);

    /* Iterate over name */
    /* Note this will not work with UTF-8!  We should support this eventually
     * -NAF 5/18/2015 */
    while((pct = HDstrchr(p, '%'))) {
        HDassert(pct >= p);

        /* Allocate name segment struct if necessary */
        if(!*tmp_parsed_name_p) {
            //HDassert(0 && "Checking code coverage..."); //VDSINC
            if(NULL == (*tmp_parsed_name_p = H5FL_CALLOC(H5O_storage_virtual_name_seg_t)))
                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to allocate name segment struct")
        } //VDSINC

        /* Check for type of format specifier */
        if(pct[1] == 'b') {
            /* Check for blank string before specifier */
            if(pct != p) {
                //HDassert(0 && "Checking code coverage..."); //VDSINC
                /* Append string to name segment */
                if(H5D__virtual_str_append(p, (size_t)(pct - p), &name_seg_p, &(*tmp_parsed_name_p)->name_segment,
    &name_seg_size) < 0)
                    HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to append name segment")
            } //VDSINC

            /* Update other variables */
            tmp_parsed_name_p = &(*tmp_parsed_name_p)->next;
            tmp_static_strlen -= 2;
            tmp_nsubs++;
            name_seg_p = NULL;
            name_seg_size = 0;
        } /* end if */
        else if(pct[1] == '%') {
            HDassert(0 && "Checking code coverage..."); //VDSINC
            /* Append string to name segment (include first '%') */
            if(H5D__virtual_str_append(p, (size_t)(pct - p) + (size_t)1, &name_seg_p, &(*tmp_parsed_name_p)->name_segment,
&name_seg_size) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to append name segment")

            /* Update other variables */
            tmp_static_strlen -= 1;
        } /* end else */
        else
            HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid format specifier")

        p = pct + 2;
    } /* end while */

    /* Copy last segment of name, if any, unless there were no substitutions */
    if(tmp_nsubs > 0) {
        HDassert(p >= source_name);
        if(*p == '\0') {
            HDassert(0 && "Checking code coverage..."); //VDSINC
            HDassert((size_t)(p - source_name) == tmp_strlen);
        } //VDSINC
        else {
            HDassert((size_t)(p - source_name) < tmp_strlen);

            /* Allocate name segment struct if necessary */
            if(!*tmp_parsed_name_p) {
                //HDassert(0 && "Checking code coverage..."); //VDSINC
                if(NULL == (*tmp_parsed_name_p = H5FL_CALLOC(H5O_storage_virtual_name_seg_t)))
                    HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to allocate name segment struct")
            } /* end if */

            /* Append string to name segment */
            if(H5D__virtual_str_append(p, tmp_strlen - (size_t)(p - source_name), &name_seg_p, &(*tmp_parsed_name_p)->name_segment,
    &name_seg_size) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "unable to append name segment")
        } /* end else */
    } /* end if */

    /* Set return values */
    *parsed_name = tmp_parsed_name;
    tmp_parsed_name = NULL;
    *static_strlen = tmp_static_strlen;
    *nsubs = tmp_nsubs;

done:
    if(tmp_parsed_name) {
        HDassert(ret_value < 0);
        H5D_virtual_free_parsed_name(tmp_parsed_name);
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D_virtual_parse_source_name() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_copy_parsed_name
 *
 * Purpose:     Deep copies a parsed source file or dataset name.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 19, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_copy_parsed_name(H5O_storage_virtual_name_seg_t **dst,
    H5O_storage_virtual_name_seg_t *src)
{
    H5O_storage_virtual_name_seg_t *tmp_dst = NULL;
    H5O_storage_virtual_name_seg_t *p_src = src;
    H5O_storage_virtual_name_seg_t **p_dst = &tmp_dst;
    herr_t          ret_value = SUCCEED;

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(dst);

    /* Walk over parsed name, duplicating it */
    while(p_src) {
        //HDassert(0 && "Checking code coverage..."); //VDSINC
        /* Allocate name segment struct */
        if(NULL == (*p_dst = H5FL_CALLOC(H5O_storage_virtual_name_seg_t)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to allocate name segment struct")

        /* Duplicate name segment */
        if(NULL == ((*p_dst)->name_segment = HDstrdup(p_src->name_segment)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to duplicate name segment")

        /* Advance pointers */
        p_src = p_src->next;
        p_dst = &(*p_dst)->next;
    } /* end while */

    /* Set dst */
    *dst = tmp_dst;
    tmp_dst = NULL;

done:
    if(tmp_dst) {
        HDassert(ret_value < 0);
        H5D_virtual_free_parsed_name(tmp_dst);
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_copy_parsed_name() */


/*-------------------------------------------------------------------------
 * Function:    H5D_virtual_free_parsed_name
 *
 * Purpose:     Frees the provided parsed name.
 *
 * Return:      void
 *
 * Programmer:  Neil Fortner
 *              May 19, 2015
 *
 *-------------------------------------------------------------------------
 */
void
H5D_virtual_free_parsed_name(H5O_storage_virtual_name_seg_t *name_seg)
{
    H5O_storage_virtual_name_seg_t *next_seg;

    FUNC_ENTER_NOAPI_NOERR

    /* Walk name segments, freeing them */
    while(name_seg) {
        //HDassert(0 && "Checking code coverage..."); //VDSINC
        (void)H5MM_xfree(name_seg->name_segment);
        next_seg = name_seg->next;
        (void)H5FL_FREE(H5O_storage_virtual_name_seg_t, name_seg);
        name_seg = next_seg;
    } /* end while */

    FUNC_LEAVE_NOAPI_VOID
} /* end H5D_virtual_free_parsed_name() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_build_source_name
 *
 * Purpose:     Builds a source file or dataset name from a parsed name.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 18, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_build_source_name(char *source_name,
    const H5O_storage_virtual_name_seg_t *parsed_name, size_t static_strlen,
    size_t nsubs, hsize_t blockno, char **built_name)
{
    char        *tmp_name = NULL;       /* Name buffer */
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(source_name);
    HDassert(built_name);

    /* Check for static name */
    if(nsubs == 0) {
        //HDassert(0 && "Checking code coverage..."); //VDSINC
        HDassert(!parsed_name);
        *built_name = source_name;
    } /* end if */
    else {
        const H5O_storage_virtual_name_seg_t *name_seg = parsed_name;
        char *p;
        hsize_t blockno_down = blockno;
        size_t blockno_len = 1;
        size_t name_len;
        size_t name_len_rem;
        size_t seg_len;
        size_t nsubs_rem = nsubs;

        HDassert(parsed_name);

        /* Calculate length of printed block number */
        do {
            blockno_down /= (hsize_t)10;
            if(blockno_down == 0)
                break;
            blockno_len++;
        } while(1);

        /* Calculate length of name buffer */
        name_len_rem = name_len = static_strlen + (nsubs * blockno_len) + (size_t)1;

        /* Allocate name buffer */
        if(NULL == (tmp_name = (char *)H5MM_malloc(name_len)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to allocate name buffer")
        p = tmp_name;

        /* Build name */
        do {
            /* Add name segment */
            if(name_seg->name_segment) {
                //HDassert(0 && "Checking code coverage..."); //VDSINC
                seg_len = HDstrlen(name_seg->name_segment);
                HDassert(seg_len > 0);
                HDassert(seg_len < name_len_rem);
                HDstrncpy(p, name_seg->name_segment, name_len_rem);
                name_len_rem -= seg_len;
                p += seg_len;
            } /* end if */

            /* Add block number */
            if(nsubs_rem > 0) {
                //HDassert(0 && "Checking code coverage..."); //VDSINC
                HDassert(blockno_len < name_len_rem);
                if(HDsnprintf(p, name_len_rem, "%llu", (long long unsigned)blockno) < 0)
                    HGOTO_ERROR(H5E_DATASET, H5E_WRITEERROR, FAIL, "unable to write block number to string")
                name_len_rem -= blockno_len;
                p += blockno_len;
                nsubs_rem--;
            } /* end if */

            /* Advance name_seg */
            name_seg = name_seg->next;
        } while(name_seg);

        /* Assign built_name */
        *built_name = tmp_name;
        tmp_name = NULL;
    } /* end else */

done:
    if(tmp_name) {
        HDassert(ret_value < 0);
        H5MM_free(tmp_name);
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_build_source_name() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_set_extent_unlim
 *
 * Purpose:     Sets the extent of the virtual dataset by checking the
 *              extents of source datasets where an unlimited selection
 *              matching.  Dimensions that are not unlimited in any
 *              virtual mapping selections are not affected.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              April 22, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D__virtual_set_extent_unlim(const H5D_t *dset, hid_t dxpl_id)
{
    H5O_storage_virtual_t *storage;
    hsize_t     new_dims[H5S_MAX_RANK];
    hsize_t     curr_dims[H5S_MAX_RANK];
    hsize_t     clip_size;
    int         rank;
    hbool_t     changed = FALSE;        /* Whether the VDS extent changed */
    size_t      i;
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(dset);
    storage = &dset->shared->layout.storage.u.virt;
    HDassert(dset->shared->layout.storage.type == H5D_VIRTUAL);
    HDassert((storage->view == H5D_VDS_FIRST_MISSING) || (storage->view == H5D_VDS_LAST_AVAILABLE));

    /* Get rank of VDS */
    if((rank = H5S_GET_EXTENT_NDIMS(dset->shared->space)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to get number of dimensions")

    /* Initialize new_dims to HSIZE_UNDEF */
    for(i = 0; i < (size_t)rank; i++)
        new_dims[i] = HSIZE_UNDEF;

    /* Iterate over mappings */
    for(i = 0; i < storage->list_nalloc; i++)
        /* Check for unlimited dimension */
        if(storage->list[i].unlim_dim_virtual >= 0) {
            /* Check for "printf" source dataset resolution */
            if(storage->list[i].parsed_source_file_name
                    || storage->list[i].parsed_source_dset_name) {
                hsize_t first_missing = 0;  /* First missing dataset in the current block of missing datasets */
                hsize_t j;

                /* Search for source datasets */
                HDassert(storage->printf_gap != HSIZE_UNDEF);
                for(j = 0; j <= (storage->printf_gap + first_missing); j++) {
                    /* Check for running out of space in sub_dset array */
                    if(j >= (hsize_t)storage->list[i].sub_dset_nalloc) {
                        if(storage->list[i].sub_dset_nalloc == 0) {
                            //HDassert(0 && "Checking code coverage..."); //VDSINC
                            /* Allocate sub_dset */
                            if(NULL == (storage->list[i].sub_dset = (H5O_storage_virtual_srcdset_t *)H5MM_calloc(H5D_VIRTUAL_DEF_SUB_DSET_SIZE * sizeof(H5O_storage_virtual_srcdset_t))))
                                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to allocate sub dataset array")
                            storage->list[i].sub_dset_nalloc = H5D_VIRTUAL_DEF_SUB_DSET_SIZE;
                        } /* end if */
                        else {
                            H5O_storage_virtual_srcdset_t *tmp_sub_dset;

                            HDassert(0 && "Checking code coverage..."); //VDSINC
                            /* Extend sub_dset */
                            if(NULL == (tmp_sub_dset = (H5O_storage_virtual_srcdset_t *)H5MM_realloc(storage->list[i].sub_dset, 2 * storage->list[i].sub_dset_nalloc * sizeof(H5O_storage_virtual_srcdset_t))))
                                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to extend sub dataset array")

                            /* Clear new space in sub_dset */
                            (void)HDmemset(&storage->list[i].sub_dset[storage->list[i].sub_dset_nalloc], 0, storage->list[i].sub_dset_nalloc * sizeof(H5O_storage_virtual_srcdset_t));

                            /* Update sub_dset_nalloc */
                            storage->list[i].sub_dset_nalloc *= 2;
                        } /* end else */
                    } /* end if */

                    /* Check if the dataset is already open */
                    if(storage->list[i].sub_dset[j].dset) {
                        //HDassert(0 && "Checking code coverage..."); //VDSINC
                        first_missing = j + 1;
                    } //VDSINC
                    else {
                        /* Resolve file name */
                        if(!storage->list[i].sub_dset[j].file_name) {
                            //HDassert(0 && "Checking code coverage..."); //VDSINC
                            if(H5D__virtual_build_source_name(storage->list[i].source_dset.file_name, storage->list[i].parsed_source_file_name, storage->list[i].psfn_static_strlen, storage->list[i].psfn_nsubs, j, &storage->list[i].sub_dset[j].file_name) < 0)
                                HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to build source file name")
                        } //VDSINC

                        /* Resolve dset name */
                        if(!storage->list[i].sub_dset[j].dset_name) {
                            //HDassert(0 && "Checking code coverage..."); //VDSINC
                            if(H5D__virtual_build_source_name(storage->list[i].source_dset.dset_name, storage->list[i].parsed_source_dset_name, storage->list[i].psdn_static_strlen, storage->list[i].psdn_nsubs, j, &storage->list[i].sub_dset[j].dset_name) < 0)
                                HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to build source dataset name")
                        } //VDSINC

                        /* Resolve virtual selection for block */
                        if(!storage->list[i].sub_dset[j].virtual_select) {
                            //HDassert(0 && "Checking code coverage..."); //VDSINC
                            if(NULL == (storage->list[i].sub_dset[j].virtual_select = H5S_hyper_get_unlim_block(storage->list[i].source_dset.virtual_select, j)))
                                HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to get block in unlimited selection")
                        } //VDSINC

                        /* Open source dataset */
                        if(H5D__virtual_open_source_dset(dset, &storage->list[i], &storage->list[i].sub_dset[j], dxpl_id) < 0)
                            HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENOBJ, FAIL, "unable to open source dataset")

                        /* Update first_missing */
                        if(storage->list[i].sub_dset[j].dset) {
                            //HDassert(0 && "Checking code coverage..."); //VDSINC
                            first_missing = j + 1;
                        } //VDSINC
                    } /* end else */
                } /* end for */

                /* Check if the size changed */
                if(first_missing != (hsize_t)storage->list[i].sub_dset_nused) {
                    /* Check for no datasets */
                    if(first_missing == 0) {
                        HDassert(0 && "Checking code coverage..."); //VDSINC
                        /* Set clip size to 0 */
                        clip_size = (hsize_t)0;
                    } //VDSINC
                    else {
                        hsize_t         bounds_start[H5S_MAX_RANK];
                        hsize_t         bounds_end[H5S_MAX_RANK];

                        //HDassert(0 && "Checking code coverage..."); //VDSINC
                        /* Get bounds from last valid virtual selection */
                        if(H5S_SELECT_BOUNDS(storage->list[i].sub_dset[first_missing - (hsize_t)1].virtual_select, bounds_start, bounds_end) < 0)
                            HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "unable to get selection bounds")

                        /* Set clip_size to bounds_end in unlimited dimension */
                        clip_size = bounds_end[storage->list[i].unlim_dim_virtual]
                                + (hsize_t)1;
                    } /* end else */

                    /* Clip entry root virtual select (virtual_select for all
                     * sub dsets) */
                    if(H5S_hyper_clip_unlim(storage->list[i].source_dset.virtual_select, clip_size))
                        HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "failed to clip unlimited selection")

                    /* Set sub_dset_nused and clip_size_virtual */
                    storage->list[i].sub_dset_nused = (size_t)first_missing;
                    storage->list[i].clip_size_virtual = clip_size;
                } /* end if */
                else {
                    //HDassert(0 && "Checking code coverage..."); //VDSINC
                    /* Use cached clip_size */
                    clip_size = storage->list[i].clip_size_virtual;
                } //VDSINC
            } /* end if */
            else {
                HDassert(storage->list[i].unlim_dim_source >= 0);

                /* Open source dataset */
                if(!storage->list[i].source_dset.dset)
                    if(H5D__virtual_open_source_dset(dset, &storage->list[i], &storage->list[i].source_dset, dxpl_id) < 0)
                        HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENOBJ, FAIL, "unable to open source dataset")

                /* Check if source dataset is open */
                if(storage->list[i].source_dset.dset) {
                    /* Retrieve current source dataset extent and patch mapping.
                     * Note this will clip the source selection to the extent. */
                    if(H5S_extent_copy(storage->list[i].source_select, storage->list[i].source_dset.dset->shared->space) < 0)
                        HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "can't copy source dataspace extent")

                    /* Get source space dimenstions */
                    if(H5S_get_simple_extent_dims(storage->list[i].source_select, curr_dims, NULL) < 0)
                        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "can't get source space dimensions")

                    /* Check if the source extent in the unlimited dimension
                     * changed since the last time the VDS extent/mapping
                     * was updated */
                    if(curr_dims[storage->list[i].unlim_dim_source]
                            == storage->list[i].unlim_extent_source)
                        /* Use cached result for clip size */
                        clip_size = storage->list[i].clip_size_virtual;
                    else {
                        /* Get size that virtual selection would be clipped to
                         * to match size of source selection */
                        if(H5S_hyper_get_clip_extent(storage->list[i].source_dset.virtual_select, storage->list[i].source_select, &clip_size, storage->view == H5D_VDS_FIRST_MISSING) < 0)
                            HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "can't get hyperslab clip size")

                        /* If we are setting the extent by the last available data,
                         * clip virtual_select.  Note that if we used the cached
                         * clip_size above, the selection will already be clipped to
                         * the correct size. */
                        if(storage->view == H5D_VDS_LAST_AVAILABLE)
                            if(H5S_hyper_clip_unlim(storage->list[i].source_dset.virtual_select, clip_size))
                                HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "failed to clip unlimited selection")

                        /* Update cached values unlim_extent_source and
                         * clip_size_virtual */
                        storage->list[i].unlim_extent_source = curr_dims[storage->list[i].unlim_dim_source];
                        storage->list[i].clip_size_virtual = clip_size;
                    } /* end else */
                } /* end if */
                else
                    clip_size = 0;
            } /* end else */

            /* Update new_dims */
            if((new_dims[storage->list[i].unlim_dim_virtual] == HSIZE_UNDEF)
                    || (storage->view == H5D_VDS_FIRST_MISSING ? (clip_size
                    < (hsize_t)new_dims[storage->list[i].unlim_dim_virtual])
                    : (clip_size
                    > (hsize_t)new_dims[storage->list[i].unlim_dim_virtual])))
                new_dims[storage->list[i].unlim_dim_virtual] = clip_size;
        } /* end if */

    /* Get current VDS dimensions */
    if(H5S_get_simple_extent_dims(dset->shared->space, curr_dims, NULL) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "can't get VDS dimensions")

    /* Calculate new extent */
    for(i = 0; i < (size_t)rank; i++) {
        if(new_dims[i] == HSIZE_UNDEF)
            new_dims[i] = curr_dims[i];
        else if(new_dims[i] < storage->min_dims[i]) {
            //HDassert(0 && "Checking code coverage..."); //VDSINC
            new_dims[i] = storage->min_dims[i];
        } //VDSINC
        if(new_dims[i] != curr_dims[i])
            changed = TRUE;
    } /* end for */

    /* If we did not change the VDS dimensions and we are setting the extent by
     * maximum, there is nothing more to update */
    if(changed || (storage->view == H5D_VDS_FIRST_MISSING)) {
        /* Update VDS extent */
        if(changed)
            if(H5S_set_extent(dset->shared->space, new_dims) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to modify size of data space")

        /* Iterate over mappings again to update source selections and virtual
         * mapping extents */
        for(i = 0; i < storage->list_nalloc; i++)
            /* Check for unlimited dimension */
            if(storage->list[i].unlim_dim_source >= 0) {
                HDassert(storage->list[i].unlim_dim_virtual >= 0);

                /* Update virtual mapping extent.  Note this function does not
                 * clip the selection. */
                if(changed)
                    if(H5S_set_extent(storage->list[i].source_dset.virtual_select, new_dims) < 0)
                        HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to modify size of data space")

                /* Check if we are setting extent by the minimum of mappings */
                if(storage->view == H5D_VDS_FIRST_MISSING) {
                    /* Clip virtual selection to extent (only necessary if the
                     * extent changed, otherwise it will already be clipped to
                     * the extent) */
                    if(changed)
                        if(H5S_hyper_clip_to_extent(storage->list[i].source_dset.virtual_select))
                            HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "failed to clip unlimited selection")

                    /* Check if the virtual extent in the unlimited dimension
                     * changed since the last time the VDS extent/mapping was
                     * updated */
                    if(new_dims[storage->list[i].unlim_dim_virtual]
                            == storage->list[i].unlim_extent_virtual)
                        /* Use cached result for clip size */
                        clip_size = storage->list[i].clip_size_source;
                    else {
                        /* Get size that source selection will be clipped to to
                         * match size of virtual selection */
                        if(H5S_hyper_get_clip_extent(storage->list[i].source_select, storage->list[i].source_dset.virtual_select, &clip_size, FALSE) < 0)
                            HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "can't get hyperslab clip size")

                        /* Update cached values unlim_extent_virtual and
                         * clip_size_source */
                        storage->list[i].unlim_extent_virtual = new_dims[storage->list[i].unlim_dim_virtual];
                        storage->list[i].clip_size_source = clip_size;
                    } /* end else */

                    /* Clip source_select */
                    if(H5S_hyper_clip_unlim(storage->list[i].source_select, clip_size))
                        HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "failed to clip unlimited selection")
                } /* end if */
            } /* end if */
    } /* end if */

    /* Call H5D__mark so dataspace is updated on disk? VDSINC */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_set_extent_unlim() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_init
 *
 * Purpose:     Initialize the virtual layout information for a dataset.
 *              This is called when the dataset is initialized.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              Thursday, April 30, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D__virtual_init(H5F_t UNUSED *f, hid_t UNUSED dxpl_id, const H5D_t *dset,
    hid_t dapl_id)
{
    H5P_genplist_t *dapl;               /* Data access property list object pointer */
    size_t      i;                      /* Local index variable */
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(dset);
    HDassert(dset->shared->layout.storage.u.virt.list || (dset->shared->layout.storage.u.virt.list_nused == 0));

    /* Patch the virtual selection dataspaces */
    for(i = 0; i < dset->shared->layout.storage.u.virt.list_nused; i++) {
        if(dset->shared->layout.storage.u.virt.list[i].virtual_space_status != H5O_VIRTUAL_STATUS_CORRECT) {
            if(H5S_extent_copy(dset->shared->layout.storage.u.virt.list[i].source_dset.virtual_select, dset->shared->space) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCOPY, FAIL, "can't copy virtual dataspace extent")
            dset->shared->layout.storage.u.virt.list[i].virtual_space_status = H5O_VIRTUAL_STATUS_CORRECT;
        } /* end if */
    } /* end for */

    /* Get dataset access property list */
    if(NULL == (dapl = (H5P_genplist_t *)H5I_object(dapl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for dapl ID")

    /* Get view option */
    if(H5P_get(dapl, H5D_ACS_VDS_VIEW_NAME, &dset->shared->layout.storage.u.virt.view) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get virtual view option")

    /* Get printf gap */
    if(H5P_get(dapl, H5D_ACS_VDS_PRINTF_GAP_NAME, &dset->shared->layout.storage.u.virt.printf_gap) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get virtual printf gap")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_init() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_is_space_alloc
 *
 * Purpose:     Query if space is allocated for layout
 *
 * Return:      TRUE if space is allocated
 *              FALSE if it is not
 *              Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
hbool_t
H5D__virtual_is_space_alloc(const H5O_storage_t UNUSED *storage)
{
    hbool_t ret_value;                  /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    /* Need to decide what to do here.  For now just return TRUE VDSINC */
    ret_value = TRUE;//storage->u.virt.serial_list_hobjid.addr != HADDR_UNDEF;

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_is_space_alloc() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_io_init
 *
 * Purpose:     Performs initialization before any sort of I/O on the raw data
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_io_init(const H5D_io_info_t UNUSED *io_info, const H5D_type_info_t UNUSED *type_info,
    hsize_t UNUSED nelmts, const H5S_t UNUSED *file_space, const H5S_t UNUSED *mem_space,
    H5D_chunk_map_t UNUSED *cm)
{
    FUNC_ENTER_STATIC_NOERR

    /* No-op for now.  Delete if we never add anything here. VDSINC */

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5D__virtual_io_init() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_read_one
 *
 * Purpose:     Read from a singe source dataset in a virtual dataset.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 15, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_read_one(H5D_io_info_t *io_info, const H5D_type_info_t *type_info,
    const H5S_t *file_space, const H5S_t *mem_space,
    H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset)
{
    H5S_t       *projected_mem_space = NULL; /* Memory space for selection in a single mapping */
    H5S_t       *projected_src_space = NULL; /* File space for selection in a single source dataset */
    hssize_t    select_nelmts;              /* Number of elements in selection */
    herr_t      ret_value = SUCCEED;        /* Return value */

    FUNC_ENTER_STATIC

    HDassert(virtual_ent);
    HDassert(source_dset);

    /* Project intersection of file space and mapping virtual space onto
     * memory space */
    if(H5S_select_project_intersection(file_space, mem_space, source_dset->virtual_select, &projected_mem_space) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "can't project virtual intersection onto memory space")

    /* Get number of elements in projected dataspace */
    if((select_nelmts = (hssize_t)H5S_GET_SELECT_NPOINTS(projected_mem_space)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTCOUNT, FAIL, "unable to get number of elements in selection")

    /* Only perform I/O if there are any elements */
    if(select_nelmts > 0) {
        /* Open source dataset */
        if(!source_dset->dset)
            /* Try to open dataset */
            if(H5D__virtual_open_source_dset(io_info->dset, virtual_ent, source_dset, io_info->dxpl_id) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENOBJ, FAIL, "unable to open source dataset")

        /* Check if source dataset is open */
        if(source_dset->dset) {
            /* Sanity check that the source space has been patched by now */
            HDassert(virtual_ent->source_space_status == H5O_VIRTUAL_STATUS_CORRECT);

            /* Project intersection of file space and mapping virtual space onto
             * mapping source space */
            if(H5S_select_project_intersection(source_dset->virtual_select, virtual_ent->source_select, file_space, &projected_src_space) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "can't project virtual intersection onto source space")

            /* Perform read on source dataset */
            if(H5D__read(source_dset->dset, type_info->dst_type_id, projected_mem_space, projected_src_space, io_info->dxpl_id, io_info->u.rbuf) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "can't read source dataset")

            /* Close projected_src_space */
            if(H5S_close(projected_src_space) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected source space")
            projected_src_space = NULL;
        } /* end if */
    } /* end if */

    /* Close projected_mem_space */
    if(H5S_close(projected_mem_space) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected memory space")
    projected_mem_space = NULL;

done:
    /* Release allocated resources on failure */
    if(projected_src_space) {
        HDassert(ret_value < 0);
        if(H5S_close(projected_src_space) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected source space")
    } /* end if */
    if(projected_mem_space) {
        HDassert(ret_value < 0);
        if(H5S_close(projected_mem_space) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected memory space")
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_read_one() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_read
 *
 * Purpose:     Read from a virtual dataset.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_read(H5D_io_info_t *io_info, const H5D_type_info_t *type_info,
    hsize_t nelmts, const H5S_t *file_space, const H5S_t *mem_space,
    H5D_chunk_map_t UNUSED *fm)
{
    H5O_storage_virtual_t *storage;         /* Convenient pointer into layout struct */
    size_t      i, j;                       /* Local index variables */
    herr_t      ret_value = SUCCEED;        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(io_info);
    HDassert(io_info->u.rbuf);
    HDassert(type_info);
    HDassert(mem_space);
    HDassert(file_space);

    storage = &io_info->dset->shared->layout.storage.u.virt;
    HDassert((storage->view == H5D_VDS_FIRST_MISSING) || (storage->view == H5D_VDS_LAST_AVAILABLE));

    /* Iterate over mappings */
    for(i = 0; i < storage->list_nused; i++) {
        /* Sanity check that the virtual space has been patched by now */
        HDassert(storage->list[i].virtual_space_status == H5O_VIRTUAL_STATUS_CORRECT);

        /* Check for "printf" source dataset resolution */
        if(storage->list[i].parsed_source_file_name
                || storage->list[i].parsed_source_dset_name) {
            /* Iterate over sub-source dsets */
            for(j = 0; j < storage->list[i].sub_dset_nused; j++) {
                //HDassert(0 && "Checking code coverage..."); //VDSINC
                if(H5D__virtual_read_one(io_info, type_info, file_space, mem_space, &storage->list[i], &storage->list[i].sub_dset[j]) < 0)
                    HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "unable to read source dataset")
            } //VDSINC
        } /* end if */
        else
            /* Read from source dataset */
            if(H5D__virtual_read_one(io_info, type_info, file_space, mem_space, &storage->list[i], &storage->list[i].source_dset) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "unable to read source dataset")
    } /* end for */

    /* Fill unmapped part of buffer with fill value.  Keep track of total number
     * elements written to memory buffer and assert that it == nelmts VDSINC */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_read() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_write_one
 *
 * Purpose:     Write to a singe source dataset in a virtual dataset.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              May 15, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_write_one(H5D_io_info_t *io_info, const H5D_type_info_t *type_info,
    const H5S_t *file_space, const H5S_t *mem_space,
    H5O_storage_virtual_ent_t *virtual_ent,
    H5O_storage_virtual_srcdset_t *source_dset)
{
    H5S_t       *projected_mem_space = NULL; /* Memory space for selection in a single mapping */
    H5S_t       *projected_src_space = NULL; /* File space for selection in a single source dataset */
    hssize_t    select_nelmts;              /* Number of elements in selection */
    herr_t      ret_value = SUCCEED;        /* Return value */

    FUNC_ENTER_STATIC

    HDassert(virtual_ent);
    HDassert(source_dset);

    /* Project intersection of file space and mapping virtual space onto
     * memory space */
    if(H5S_select_project_intersection(file_space, mem_space, source_dset->virtual_select, &projected_mem_space) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "can't project virtual intersection onto memory space")

    /* Get number of elements in projected dataspace */
    if((select_nelmts = (hssize_t)H5S_GET_SELECT_NPOINTS(projected_mem_space)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTCOUNT, FAIL, "unable to get number of elements in selection")

    /* Only perform I/O if there are any elements */
    if(select_nelmts > 0) {
        /* Open source dataset */
        if(!source_dset->dset) {
            /* Try to open dataset */
            if(H5D__virtual_open_source_dset(io_info->dset, virtual_ent, source_dset, io_info->dxpl_id) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENOBJ, FAIL, "unable to open source dataset")
            if(!source_dset->dset)
                HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENOBJ, FAIL, "did not open source dataset")
        } /* end if */

        /* Sanity check that the source space has been patched by now */
        HDassert(virtual_ent->source_space_status == H5O_VIRTUAL_STATUS_CORRECT);

        /* Extend source dataset if necessary and there is an unlimited
         * dimension VDSINC */
        /* Project intersection of file space and mapping virtual space onto
         * mapping source space */
        if(H5S_select_project_intersection(source_dset->virtual_select, virtual_ent->source_select, file_space, &projected_src_space) < 0)
            HGOTO_ERROR(H5E_DATASET, H5E_CANTCLIP, FAIL, "can't project virtual intersection onto source space")

        /* Perform write on source dataset */
        if(H5D__write(source_dset->dset, type_info->dst_type_id, projected_mem_space, projected_src_space, io_info->dxpl_id, io_info->u.wbuf) < 0)
            HGOTO_ERROR(H5E_DATASET, H5E_WRITEERROR, FAIL, "can't write to source dataset")

        /* Close projected_src_space */
        if(H5S_close(projected_src_space) < 0)
            HGOTO_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected source space")
        projected_src_space = NULL;
    } /* end if */

    /* Close projected_mem_space */
    if(H5S_close(projected_mem_space) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected memory space")
    projected_mem_space = NULL;

done:
    /* Release allocated resources on failure */
    if(projected_src_space) {
        HDassert(ret_value < 0);
        if(H5S_close(projected_src_space) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected source space")
    } /* end if */
    if(projected_mem_space) {
        HDassert(ret_value < 0);
        if(H5S_close(projected_mem_space) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "can't close projected memory space")
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_write_one() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_write
 *
 * Purpose:     Write to a virtual dataset.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_write(H5D_io_info_t *io_info, const H5D_type_info_t *type_info,
    hsize_t nelmts, const H5S_t *file_space, const H5S_t *mem_space,
    H5D_chunk_map_t UNUSED *fm)
{
    H5O_storage_virtual_t *storage;         /* Convenient pointer into layout struct */
    size_t      i, j;                       /* Local index variable */
    herr_t      ret_value = SUCCEED;        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(io_info);
    HDassert(io_info->u.wbuf);
    HDassert(type_info);
    HDassert(mem_space);
    HDassert(file_space);

    storage = &io_info->dset->shared->layout.storage.u.virt;
    HDassert((storage->view == H5D_VDS_FIRST_MISSING) || (storage->view == H5D_VDS_LAST_AVAILABLE));

    /* Iterate over mappings */
    for(i = 0; i < storage->list_nused; i++) {
        /* Sanity check that virtual space has been patched by now */
        HDassert(storage->list[i].virtual_space_status == H5O_VIRTUAL_STATUS_CORRECT);

        /* Check for "printf" source dataset resolution */
        if(storage->list[i].parsed_source_file_name
                || storage->list[i].parsed_source_dset_name) {
            /* Iterate over sub-source dsets */
            for(j = 0; j < storage->list[i].sub_dset_nused; j++) {
                HDassert(0 && "Checking code coverage..."); //VDSINC
                if(H5D__virtual_write_one(io_info, type_info, file_space, mem_space, &storage->list[i], &storage->list[i].sub_dset[j]) < 0)
                    HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "unable to write to source dataset")
            } //VDSINC
        } /* end if */
        else
            /* Write to source dataset */
            if(H5D__virtual_write_one(io_info, type_info, file_space, mem_space, &storage->list[i], &storage->list[i].source_dset) < 0)
                HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "unable to write to source dataset")
    } /* end for */

    /* Keep track of total number elements written to disk buffer and issure
     * error if it != nelmts VDSINC */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5D__virtual_write() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_flush
 *
 * Purpose:     Writes all dirty data to disk.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5D__virtual_flush(H5D_t UNUSED *dset, hid_t UNUSED dxpl_id)
{
    FUNC_ENTER_STATIC_NOERR

    /* Flush only open datasets */
    /* No-op for now VDSINC */

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5D__virtual_flush() */


/*-------------------------------------------------------------------------
 * Function:    H5D__virtual_copy
 *
 * Purpose:     Copy virtual storage raw data from SRC file to DST file.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Neil Fortner
 *              February 6, 2015
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5D__virtual_copy(H5F_t UNUSED *f_src, const H5O_storage_virtual_t UNUSED *storage_src,
    H5F_t UNUSED *f_dst, H5O_storage_virtual_t UNUSED *storage_dst, H5T_t UNUSED *dt_src,
    H5O_copy_t UNUSED *cpy_info, hid_t UNUSED dxpl_id)
{
    FUNC_ENTER_PACKAGE_NOERR

    HDassert(0 && "Not yet implemented...");//VDSINC

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5D__virtual_copy() */

