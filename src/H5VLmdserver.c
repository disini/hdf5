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

/* Programmer:  Mohamad Chaarawi <chaarawi@hdfgroup.org>
 *
 * Purpose:	Metadata server code
 */

/****************/
/* Module Setup */
/****************/


#define H5A_PACKAGE		/*suppress error about including H5Apkg	  */
#define H5D_PACKAGE		/*suppress error about including H5Dpkg	  */
#define H5F_PACKAGE		/*suppress error about including H5Fpkg	  */
#define H5G_PACKAGE		/*suppress error about including H5Gpkg   */
#define H5L_PACKAGE		/*suppress error about including H5Lpkg   */
#define H5O_PACKAGE		/*suppress error about including H5Opkg	  */
#define H5P_PACKAGE		/*suppress error about including H5Ppkg	  */
#define H5R_PACKAGE		/*suppress error about including H5Rpkg	  */
#define H5T_PACKAGE		/*suppress error about including H5Tpkg	  */

/* Interface initialization */
#define H5_INTERFACE_INIT_FUNC	H5VL_init_mdserver_interface


/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5Apkg.h"             /* Attribute pkg                        */
#include "H5Aprivate.h"		/* Attributes				*/
#include "H5Dpkg.h"             /* Dataset pkg                          */
#include "H5Dprivate.h"		/* Datasets				*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Fprivate.h"		/* File access				*/
#include "H5Fpkg.h"             /* File pkg                             */
#include "H5FDmpi.h"            /* MPI-based file drivers		*/
#include "H5FDmds.h"            /* MDS file driver      		*/
#include "H5Gpkg.h"		/* Groups		  		*/
#include "H5HGprivate.h"	/* Global Heaps				*/
#include "H5Iprivate.h"		/* IDs			  		*/
#include "H5Lprivate.h"         /* links                                */
#include "H5Lpkg.h"             /* links headers			*/
#include "H5MFprivate.h"	/* File memory management		*/
#include "H5MMprivate.h"	/* Memory management			*/
#include "H5Opkg.h"             /* Object headers			*/
#include "H5Pprivate.h"		/* Property lists			*/
#include "H5Ppkg.h"		/* Property lists		  	*/
#include "H5Rpkg.h"		/* References   			*/
#include "H5Sprivate.h" 	/* Dataspaces                      	*/
#include "H5SMprivate.h"	/* Shared Object Header Messages	*/
#include "H5Tpkg.h"		/* Datatypes				*/
#include "H5Tprivate.h"		/* Datatypes				*/
#include "H5VLprivate.h"	/* VOL plugins				*/
#include "H5VLnative.h"         /* Native VOL plugin			*/
#include "H5VLmdserver.h"       /* MDS helper routines			*/

#ifdef H5_HAVE_PARALLEL

/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/


/********************/
/* Local Prototypes */
/********************/
static herr_t H5VL__file_create_cb(uint8_t *p, int source);
static herr_t H5VL__file_open_cb(uint8_t *p, int source);
static herr_t H5VL__file_flush_cb(uint8_t *p, int source);
static herr_t H5VL__file_close_cb(uint8_t *p, int source);
static herr_t H5VL__dataset_create_cb(uint8_t *p, int source);
static herr_t H5VL__dataset_open_cb(uint8_t *p, int source);
static herr_t H5VL__dataset_close_cb(uint8_t *p, int source);
static herr_t H5VL__datatype_commit_cb(uint8_t *p, int source);
static herr_t H5VL__datatype_open_cb(uint8_t *p, int source);
static herr_t H5VL__datatype_close_cb(uint8_t *p, int source);
static herr_t H5VL__allocate_cb(uint8_t *p, int source);
static herr_t H5VL__set_eoa_cb(uint8_t *p, int source);
static herr_t H5VL__get_eoa_cb(uint8_t *p, int source);

typedef herr_t (*H5VL_mds_op)(uint8_t *p, int source);

/*********************/
/* Package Variables */
/*********************/


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/



/*--------------------------------------------------------------------------
NAME
   H5VL_init_mdserver_interface -- Initialize interface-specific information
USAGE
    herr_t H5VL_init_mdserver_interface()
RETURNS
    Non-negative on success/Negative on failure
DESCRIPTION
    Initializes any interface-specific data or routines.  (Just calls
    H5VL_init() currently).

--------------------------------------------------------------------------*/
static herr_t
H5VL_init_mdserver_interface(void)
{
    FUNC_ENTER_NOAPI_NOINIT_NOERR

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* H5VL_init_mdserver_interface() */


/*-------------------------------------------------------------------------
 * Function:	H5VLmds_start
 *
 * Purpose:	This is the API routine that the MDS process calls to start 
 *              looping and accept requests from clients.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:  Mohamad Chaarawi
 *              August, 2012
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_mds_start(void)
{
    herr_t ret_value = SUCCEED;
    H5VL_mds_op mds_ops[H5VL_NUM_OPS];

    FUNC_ENTER_NOAPI_NOINIT

    mds_ops[H5VL_FILE_CREATE]   = H5VL__file_create_cb;
    mds_ops[H5VL_FILE_OPEN]     = H5VL__file_open_cb;
    mds_ops[H5VL_FILE_FLUSH]    = H5VL__file_flush_cb;
    mds_ops[H5VL_FILE_CLOSE]    = H5VL__file_close_cb;
    mds_ops[H5VL_DSET_CREATE]   = H5VL__dataset_create_cb;
    mds_ops[H5VL_DSET_OPEN]     = H5VL__dataset_open_cb;
    mds_ops[H5VL_DSET_CLOSE]    = H5VL__dataset_close_cb;
    mds_ops[H5VL_DTYPE_COMMIT]  = H5VL__datatype_commit_cb;
    mds_ops[H5VL_DTYPE_OPEN]    = H5VL__datatype_open_cb;
    mds_ops[H5VL_DTYPE_CLOSE]   = H5VL__datatype_close_cb;
    mds_ops[H5VL_ALLOC]         = H5VL__allocate_cb;
    mds_ops[H5VL_GET_EOA]       = H5VL__set_eoa_cb;
    mds_ops[H5VL_SET_EOA]       = H5VL__get_eoa_cb;

    /* turn off commsplitter to talk to the other processes */
    MPI_Pcontrol(0);

    while(1) {
        MPI_Status status;
        int incoming_msg_size;
        void *recv_buf = NULL;
        uint8_t *p;     /* Current pointer into buffer */
        H5VL_op_type_t op_type;

        printf("MDS Process Waiting\n");
        /* probe for a message from a client */
        if(MPI_SUCCESS != MPI_Probe(MPI_ANY_SOURCE, H5VL_MDS_LISTEN_TAG, MPI_COMM_WORLD, &status))
            HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to probe for a message");
        /* get the incoming message size from the probe result */
        if(MPI_SUCCESS != MPI_Get_count(&status, MPI_BYTE, &incoming_msg_size))
            HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to get incoming message size");

        /* allocate the receive buffer */
        if(NULL == (recv_buf = H5MM_malloc((size_t)incoming_msg_size)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

        /* receive the actual message */
        if(MPI_SUCCESS != MPI_Recv (recv_buf, incoming_msg_size, MPI_BYTE, status.MPI_SOURCE, 
                                    H5VL_MDS_LISTEN_TAG, MPI_COMM_WORLD, &status))
            HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to receivemessage");

        p = (uint8_t *)recv_buf;
        op_type = (H5VL_op_type_t)*p++;

        if((*mds_ops[op_type])(p, status.MPI_SOURCE) < 0) {
            printf("failed mds op\n");
            sleep(2);
            exit(0);
        }

        if(NULL != recv_buf)
            H5MM_free(recv_buf);
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VLmds_start() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_create_cb
 *------------------------------------------------------------------------- */
static herr_t
H5VL__file_create_cb(uint8_t *p, int source)
{
    char *mds_filename = NULL; /* name of the metadata file (generated from name) */
    unsigned flags; /* access flags */
    hid_t fcpl_id = FAIL, fapl_id = FAIL; /* plist IDs */
    hid_t temp_fapl; /* fapl used for underlying MDS VFD */
    H5F_t *new_file = NULL; /* struct for MDS file */
    hid_t file_id = FAIL; /* ID of MDS file */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_file_create_params(p, &mds_filename, &flags, &fcpl_id, &fapl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode file create params");

    /* set the underlying MDS VFD */
    temp_fapl = H5Pcreate(H5P_FILE_ACCESS);
    if(H5P_set_fapl_mds(fapl_id, mds_filename, temp_fapl) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "failed to set MDS plist");

    /* create the metadata file locally */
    if(NULL == (new_file = H5F_open(mds_filename, flags, fcpl_id, fapl_id, H5AC_dxpl_id)))
        HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, FAIL, "unable to create file");

    new_file->id_exists = TRUE;

    if((file_id = H5VL_native_register(H5I_FILE, new_file, FALSE)) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, FAIL, "unable to create file");

done:
    if(mds_filename)
        H5MM_xfree(mds_filename);

    if(SUCCEED == ret_value) {
        /* Send the meta data file ID to the client */
        if(MPI_SUCCESS != MPI_Send(&file_id, sizeof(hid_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    else {
        /* send a failed message to the client */
        if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    printf("MDS created file %d\n", file_id);
    FUNC_LEAVE_NOAPI(ret_value)
}/* H5VL__file_create_cb */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_open_cb
 *------------------------------------------------------------------------- */
static herr_t
H5VL__file_open_cb(uint8_t *p, int source)
{
    char *mds_filename = NULL; /* name of the metadata file (generated from name) */
    unsigned flags; /* access flags */
    hid_t fapl_id = FAIL; /* plist IDs */
    hid_t temp_fapl; /* fapl used for underlying MDS VFD */
    H5F_t *new_file = NULL; /* struct for MDS file */
    hid_t file_id; /* ID of MDS file */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_file_open_params(p, &mds_filename, &flags, &fapl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode file open params");

    /* set the underlying MDS VFD */
    temp_fapl = H5Pcreate(H5P_FILE_ACCESS);
    if(H5P_set_fapl_mds(fapl_id, mds_filename, temp_fapl) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTINIT, FAIL, "failed to set MDS plist");

    /* create the metadata file locally */
    if(NULL == (new_file = H5F_open(mds_filename, flags, H5P_FILE_CREATE_DEFAULT, 
                                    fapl_id, H5AC_dxpl_id)))
        HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, FAIL, "unable to open file")

    new_file->id_exists = TRUE;

    if((file_id = H5VL_native_register(H5I_FILE, new_file, FALSE)) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, FAIL, "unable to create file");

done:
    if(mds_filename)
        H5MM_xfree(mds_filename);

    if(SUCCEED == ret_value) {
        /* Send the meta data file ID to the client */
        if(MPI_SUCCESS != MPI_Send(&file_id, sizeof(hid_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    else {
        /* send a failed message to the client */
        if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    printf("MDS opened file %d\n", file_id);
    FUNC_LEAVE_NOAPI(ret_value)
}/* H5VL__file_open_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__file_flush_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__file_flush_cb(uint8_t *p, int source)
{
    hid_t obj_id; /* metadata object ID */
    H5F_t *f = NULL; /* metadata file struct */
    H5F_scope_t scope;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_file_flush_params(p, &obj_id, &scope) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode file flush params");

    if(NULL == (f = H5VL_native_get_file(H5I_object(obj_id), H5I_get_type(obj_id))))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object");

    /* Flush the file */
    /*
     * Nothing to do if the file is read only.	This determination is
     * made at the shared open(2) flags level, implying that opening a
     * file twice, once for read-only and once for read-write, and then
     * calling H5Fflush() with the read-only handle, still causes data
     * to be flushed.
     */
    if(H5F_ACC_RDWR & H5F_INTENT(f)) {
        /* Flush other files, depending on scope */
        if(H5F_SCOPE_GLOBAL == scope) {
            /* Call the flush routine for mounted file hierarchies */
            if((ret_value = H5F_flush_mounts(f, H5AC_dxpl_id)) < 0)
                HGOTO_ERROR(H5E_FILE, H5E_CANTFLUSH, FAIL, "unable to flush mounted file hierarchy");
        } /* end if */
        else {
            /* Call the flush routine, for this file */
            if((ret_value = H5F_flush(f, H5AC_dxpl_id, FALSE)) < 0)
                HGOTO_ERROR(H5E_FILE, H5E_CANTFLUSH, FAIL, "unable to flush file's cached information");
        } /* end else */
    } /* end if */

done:
    /* send status to client */
    if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");

    printf("MDS flushed file on object %d\n", obj_id);
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__file_flush_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__file_close_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__file_close_cb(uint8_t *p, int source)
{
    hid_t file_id; /* metadata file ID */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_file_close_params(p, &file_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode file close params");

    /* Check/fix arguments. */
    if(H5I_FILE != H5I_get_type(file_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file ID");

    /* Decrement reference count on atom.  When it reaches zero the file will be closed. */
    if(H5I_dec_ref(file_id) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTCLOSEFILE, FAIL, "decrementing file ID failed");
#if 0
    if(NULL == (f = (H5F_t *)H5I_object_verify(file_id, H5I_FILE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file ID");

    /* Flush file if this is the last reference to this id and we have write
     * intent, unless it will be flushed by the "shared" file being closed.
     * This is only necessary to replicate previous behaviour, and could be
     * disabled by an option/property to improve performance. */
    if((f->shared->nrefs > 1) && (H5F_INTENT(f) & H5F_ACC_RDWR)) {
        /* get the file ID corresponding to the H5F_t struct */
        if((file_id = H5I_get_id(f, H5I_FILE)) < 0)
            HGOTO_ERROR(H5E_ATOM, H5E_CANTGET, FAIL, "invalid atom");
        /* get the number of references outstanding for this file ID */
        if((nref = H5I_get_ref(file_id, FALSE)) < 0)
            HGOTO_ERROR(H5E_ATOM, H5E_CANTGET, FAIL, "can't get ID ref count");
        if(nref == 1)
            if(H5F_flush(f, H5AC_dxpl_id, FALSE) < 0)
                HGOTO_ERROR(H5E_CACHE, H5E_CANTFLUSH, FAIL, "unable to flush cache");
    } /* end if */

    /* close the file */
    if((ret_value = H5F_close(f)) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTDEC, FAIL, "can't close file");
#endif

done:
    /* send status to client */
    if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    printf("MDS closed file %d\n", file_id);
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__file_close_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__dataset_create_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__dataset_create_cb(uint8_t *p, int source)
{
    hid_t obj_id; /* id of location for dataset */
    hid_t dset_id = FAIL; /* dset id */
    H5D_t *dset = NULL; /* New dataset's info */
    H5VL_loc_params_t loc_params; /* location parameters for obj_id */
    H5G_loc_t loc; /* Object location to insert dataset into */
    char *name = NULL; /* name of dataset (if named) */
    hid_t dcpl_id = FAIL, dapl_id = FAIL, lcpl_id = FAIL; /* plist IDs */
    hid_t type_id; /* datatype for dataset */
    hid_t space_id; /* dataspace for dataset */
    const H5S_t *space; /* Dataspace for dataset */
    void *send_buf = NULL; /* buffer to hold the dataset id and layout to be sent to client */
    size_t buf_size = 0; /* send_buf size */
    uint8_t *p1 = NULL; /* temporary pointer into send_buf for encoding */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_dataset_create_params(p, &obj_id, &loc_params, &name, &dcpl_id, &dapl_id,
                                          &type_id, &space_id, &lcpl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode dataset create params");

    /* Check dataset create parameters */
    if(H5G_loc(obj_id, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object");
    if(H5I_DATATYPE != H5I_get_type(type_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype ID");
    if(NULL == (space = (const H5S_t *)H5I_object_verify(space_id, H5I_DATASPACE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a dataspace ID");

    /* H5Dcreate_anon */
    if(NULL == name) {
        /* build and open the new dataset */
        if(NULL == (dset = H5D__create(loc.oloc->file, type_id, space, dcpl_id, dapl_id, 
                                       H5AC_dxpl_id)))
            HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to create dataset");
    }
    /* H5Dcreate2 */
    else {
        /* Create the new dataset & get its ID */
        if(NULL == (dset = H5D__create_named(&loc, name, type_id, space, lcpl_id, 
                                             dcpl_id, dapl_id, H5AC_dxpl_id)))
            HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to create dataset");
    }

    if((dset_id = H5VL_native_register(H5I_DATASET, dset, FALSE)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENFILE, FAIL, "unable to create dataset");

    /* determine the buffer size needed to store the encoded layout of the dataset */ 
    if(FAIL == (ret_value = H5D__encode_layout(dset->shared->layout, NULL, &buf_size)))
        HGOTO_ERROR(H5E_SYM, H5E_CANTENCODE, FAIL, "failed to encode dataset layout");

    /* for the dataset ID */
    buf_size += sizeof(int);

    /* allocate the buffer for encoding the parameters */
    if(NULL == (send_buf = H5MM_malloc(buf_size)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

    p1 = (uint8_t *)send_buf;

    /* encode the object id */
    INT32ENCODE(p1, dset_id);

    /* encode layout of the dataset */ 
    if(FAIL == (ret_value = H5D__encode_layout(dset->shared->layout, p1, &buf_size)))
        HGOTO_ERROR(H5E_SYM, H5E_CANTENCODE, FAIL, "failed to encode dataset layout");
    buf_size += sizeof(int);

done:
    if(SUCCEED == ret_value) {
        /* Send the dataset id & metadata to the client */
        if(MPI_SUCCESS != MPI_Send(send_buf, (int)buf_size, MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    else {
        /* Send the dataset id & metadata to the client */
        if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }

    if(send_buf)
        H5MM_free(send_buf);
    if(name)
        H5MM_xfree(name);
    printf("MDS created dataset %d on file %d\n", dset_id, obj_id);
    FUNC_LEAVE_NOAPI(ret_value)
}/* H5VL__dataset_create_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__dataset_open_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__dataset_open_cb(uint8_t *p, int source)
{
    hid_t obj_id; /* id of location for dataset */
    hid_t dset_id = FAIL; /* dset id */
    H5D_t *dset = NULL; /* New dataset's info */
    H5VL_loc_params_t loc_params; /* location parameters for obj_id */
    char *name = NULL; /* name of dataset (if named) */
    H5G_loc_t loc; /* Object location of group */
    H5G_loc_t dset_loc; /* Object location of dataset */
    H5G_name_t path; /* Dataset group hier. path */
    H5O_loc_t oloc; /* Dataset object location */
    H5O_type_t obj_type; /* Type of object at location */
    hbool_t loc_found = FALSE; /* Location at 'name' found */
    size_t dcpl_size = 0, type_size = 0, space_size = 0, layout_size = 0;
    H5P_genplist_t *dcpl;
    size_t buf_size = 0;
    hid_t dapl_id = FAIL; /* plist IDs */
    void *send_buf = NULL; /* buffer to hold the dataset id and layout to be sent to client */
    uint8_t *p1 = NULL; /* temporary pointer into send_buf for encoding */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_dataset_open_params(p, &obj_id, &loc_params, &name, &dapl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode dataset open params");

    /* START open dataset */
    /* Check dataset create parameters */
    if(H5G_loc(obj_id, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object");

    /* Set up dataset location to fill in */
    dset_loc.oloc = &oloc;
    dset_loc.path = &path;
    H5G_loc_reset(&dset_loc);

    /* Find the dataset object */
    if(H5G_loc_find(&loc, name, &dset_loc, dapl_id, H5AC_dxpl_id) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_NOTFOUND, FAIL, "not found");
    loc_found = TRUE;

    /* Check that the object found is the correct type */
    if(H5O_obj_type(&oloc, &obj_type, H5AC_dxpl_id) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "can't get object type");
    if(obj_type != H5O_TYPE_DATASET)
        HGOTO_ERROR(H5E_DATASET, H5E_BADTYPE, FAIL, "not a dataset");

    /* Open the dataset */
    if(NULL == (dset = H5D_open(&dset_loc, dapl_id, H5AC_dxpl_id)))
        HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "can't open dataset");

    if((dset_id = H5VL_native_register(H5I_DATASET, dset, FALSE)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENFILE, FAIL, "unable to create dataset");
    /* END open dataset */

    /* START Encode metadata of the dataset and send them to the client along with the ID */
    /* determine the size of the dcpl if it is not default */
    if(H5P_DATASET_CREATE_DEFAULT != dset->shared->dcpl_id) {
        if(NULL == (dcpl = (H5P_genplist_t *)H5I_object_verify(dset->shared->dcpl_id, 
                                                               H5I_GENPROP_LST)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list");
        if((ret_value = H5P__encode(dcpl, FALSE, NULL, &dcpl_size)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "unable to encode property list");
    }

    if(NULL != dset->shared->type) {
        /* get Type size to encode */
        if((ret_value = H5T_encode(dset->shared->type, NULL, &type_size)) < 0)
            HGOTO_ERROR(H5E_DATATYPE, H5E_CANTENCODE, FAIL, "unable to encode datatype");
    }

    if(NULL != dset->shared->space) {
        /* get Dataspace size to encode */
        if((ret_value = H5S_encode(dset->shared->space, NULL, &space_size)) < 0)
            HGOTO_ERROR(H5E_DATASPACE, H5E_CANTENCODE, FAIL, "unable to encode dataspace");
    }

    /* determine the buffer size needed to store the encoded layout of the dataset */ 
    if(FAIL == H5D__encode_layout(dset->shared->layout, NULL, &layout_size))
        HGOTO_ERROR(H5E_SYM, H5E_CANTENCODE, FAIL, "failed to encode dataset layout");

    /* for the dataset ID */
    buf_size = sizeof(hid_t) + /* dataset ID */
        1 + H5V_limit_enc_size((uint64_t)dcpl_size) + dcpl_size +
        1 + H5V_limit_enc_size((uint64_t)type_size) + type_size + 
        1 + H5V_limit_enc_size((uint64_t)space_size) + space_size + 
        1 + H5V_limit_enc_size((uint64_t)layout_size) + layout_size;

    /* allocate the buffer for encoding the parameters */
    if(NULL == (send_buf = H5MM_malloc(buf_size)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

    p1 = (uint8_t *)send_buf;

    /* encode the object id */
    INT32ENCODE(p1, dset_id);

    /* encode the plist size */
    UINT64ENCODE_VARLEN(p1, dcpl_size);
    /* encode property lists if they are not default*/
    if(H5P_DATASET_CREATE_DEFAULT != dset->shared->dcpl_id) {
        if((ret_value = H5P__encode(dcpl, FALSE, p1, &dcpl_size)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTENCODE, FAIL, "unable to encode property list");
        p1 += dcpl_size;
    }

    /* encode the datatype size */
    UINT64ENCODE_VARLEN(p1, type_size);
    if(type_size) {
        /* encode datatype */
        if((ret_value = H5T_encode(dset->shared->type, p1, &type_size)) < 0)
            HGOTO_ERROR(H5E_DATATYPE, H5E_CANTENCODE, FAIL, "unable to encode datatype");
        p1 += type_size;
    }

    /* encode the dataspace size */
    UINT64ENCODE_VARLEN(p1, space_size);
    if(space_size) {
        /* encode datatspace */
        if((ret_value = H5S_encode(dset->shared->space, p1, &space_size)) < 0)
            HGOTO_ERROR(H5E_DATASPACE, H5E_CANTENCODE, FAIL, "unable to encode datatspace");
        p1 += space_size;
    }

    /* encode the layout size */
    UINT64ENCODE_VARLEN(p1, layout_size);
    if(layout_size) {
        /* encode layout of the dataset */ 
        if(FAIL == H5D__encode_layout(dset->shared->layout, p1, &layout_size))
            HGOTO_ERROR(H5E_SYM, H5E_CANTENCODE, FAIL, "failed to encode dataset layout");
        //p1 += layout_size;
    }

done:
    if(SUCCEED == ret_value) {
        /* Send the dataset id & metadata to the client */
        if(MPI_SUCCESS != MPI_Send(send_buf, (int)buf_size, MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    else {
        /* Send failed to the client */
        if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }

    printf("MDS opened dataset %d on file %d\n", dset_id, obj_id);
    if(send_buf)
        H5MM_free(send_buf);
    if(name)
        H5MM_xfree(name);

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__dataset_open_cb */

#if 0
case H5VL_MDS_DSET_WRITE:
{
    ;
} /* end H5VL_MDS_DSET_WRITE */
case H5VL_MDS_DSET_READ:
{
    ;
} /* end H5VL_MDS_DSET_READ */
#endif

/*-------------------------------------------------------------------------
* Function:	H5VL__dataset_close_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__dataset_close_cb(uint8_t *p, int source)
{
    hid_t dset_id = FAIL; /* dset id */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    /* decode the object id */
    if(H5VL__decode_dataset_close_params(p, &dset_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode dataset close params");

    printf("MDS closing dataset %d\n", dset_id);

    /* Check/fix arguments. */
    if(H5I_DATASET != H5I_get_type(dset_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a dataset ID");

    if(H5I_dec_app_ref_always_close(dset_id) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTDEC, FAIL, "can't decrement count on dataset ID");

done:
    /* send status to client */
    if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__dataset_close_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__datatype_commit_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__datatype_commit_cb(uint8_t *p, int source)
{
    hid_t obj_id; /* id of location for dataset */
    H5T_t *type = NULL; /* New dataset's info */
    H5VL_loc_params_t loc_params; /* location parameters for obj_id */
    H5G_loc_t loc; /* Object location to insert dataset into */
    char *name = NULL; /* name of dataset (if named) */
    hid_t tcpl_id = FAIL, tapl_id = FAIL, lcpl_id = FAIL; /* plist IDs */
    hid_t type_id; /* datatype for dataset */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_datatype_commit_params(p, &obj_id, &loc_params, &name, &type_id, &lcpl_id, 
                                           &tcpl_id, &tapl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode dataset create params");

    if(H5G_loc(obj_id, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object");
    if(NULL == (type = (H5T_t *)H5I_object_verify(type_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype");

    if(NULL != name) { /* H5Tcommit */
        /* Commit the type */
        if(H5T__commit_named(&loc, name, type, lcpl_id, tcpl_id, tapl_id, H5AC_dxpl_id) < 0)
            HGOTO_ERROR(H5E_DATATYPE, H5E_CANTINIT, FAIL, "unable to commit datatype");
    }
    else { /* H5Tcommit_anon */
        /* Commit the type */
        if(H5T__commit(loc.oloc->file, type, tcpl_id, H5AC_dxpl_id) < 0)
            HGOTO_ERROR(H5E_DATATYPE, H5E_CANTINIT, FAIL, "unable to commit datatype");

        /* Release the datatype's object header */
        {
            H5O_loc_t *oloc;         /* Object location for datatype */

            /* Get the new committed datatype's object location */
            if(NULL == (oloc = H5T_oloc(type)))
                HGOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL, "unable to get object location of committed datatype");

            /* Decrement refcount on committed datatype's object header in memory */
            if(H5O_dec_rc_by_loc(oloc, H5AC_dxpl_id) < 0)
                HGOTO_ERROR(H5E_DATATYPE, H5E_CANTDEC, FAIL, "unable to decrement refcount on newly created object");
        } /* end if */
    }

done:
    if(SUCCEED == ret_value) {
        /* Send the meta data type ID to the client */
        if(MPI_SUCCESS != MPI_Send(&type_id, sizeof(hid_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    else {
        /* send a failed message to the client */
        if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    if(name)
        H5MM_xfree(name);
    printf("MDS created datatype %d on file %d\n", type_id, obj_id);
    FUNC_LEAVE_NOAPI(ret_value)
}/* H5VL__datatype_commit_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__datatype_open_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__datatype_open_cb(uint8_t *p, int source)
{
    hid_t obj_id; /* id of location for dataset */
    H5T_t *type = NULL; /* New datatype */
    hid_t type_id; /* datatype id */
    hid_t tapl_id = FAIL;
    H5VL_loc_params_t loc_params; /* location parameters for obj_id */
    H5G_loc_t loc; /* Object location to insert dataset into */
    H5G_name_t   path;            	/* Datatype group hier. path */
    H5O_loc_t    oloc;            	/* Datatype object location */
    H5O_type_t   obj_type;              /* Type of object at location */
    H5G_loc_t    type_loc;              /* Group object for datatype */
    hbool_t      obj_found = FALSE;     /* Object at 'name' found */
    hid_t        dxpl_id = H5AC_dxpl_id; /* dxpl to use to open datatype */
    char *name = NULL; /* name of dataset (if named) */
    size_t type_size = 0, buf_size = 0;
    void *send_buf = NULL; /* buffer to hold the dataset id and layout to be sent to client */
    uint8_t *p1 = NULL; /* temporary pointer into send_buf for encoding */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    if(H5VL__decode_datatype_open_params(p, &obj_id, &loc_params, &name, &tapl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode dataset create params");

    if(H5G_loc(obj_id, &loc) < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object");

    /* Set up datatype location to fill in */
    type_loc.oloc = &oloc;
    type_loc.path = &path;
    H5G_loc_reset(&type_loc);

    /*
     * Find the named datatype object header and read the datatype message
     * from it.
     */
    if(H5G_loc_find(&loc, name, &type_loc/*out*/, tapl_id, dxpl_id) < 0)
        HGOTO_ERROR(H5E_DATATYPE, H5E_NOTFOUND, FAIL, "not found");
    obj_found = TRUE;

    /* Check that the object found is the correct type */
    if(H5O_obj_type(&oloc, &obj_type, dxpl_id) < 0)
        HGOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL, "can't get object type");
    if(obj_type != H5O_TYPE_NAMED_DATATYPE)
        HGOTO_ERROR(H5E_DATATYPE, H5E_BADTYPE, FAIL, "not a named datatype");

    /* Open it */
    if(NULL == (type = H5T_open(&type_loc, dxpl_id)))
        HGOTO_ERROR(H5E_DATATYPE, H5E_CANTOPENOBJ, FAIL, "unable to open named datatype");

    if((type_id = H5I_register(H5I_DATATYPE, type, FALSE)) < 0)
        HGOTO_ERROR(H5E_DATASET, H5E_CANTOPENFILE, FAIL, "unable to create datatype");

    /* get Type size to encode */
    if((ret_value = H5T_encode(type, NULL, &type_size)) < 0)
        HGOTO_ERROR(H5E_DATATYPE, H5E_CANTENCODE, FAIL, "unable to encode datatype");

    buf_size = type_size + sizeof(hid_t);

    /* allocate the buffer for encoding the parameters */
    if(NULL == (send_buf = H5MM_malloc(buf_size)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

    p1 = (uint8_t *)send_buf;

    /* encode the object id */
    INT32ENCODE(p1, type_id);

    /* encode datatype */
    if((ret_value = H5T_encode(type, p1, &type_size)) < 0)
        HGOTO_ERROR(H5E_DATATYPE, H5E_CANTENCODE, FAIL, "unable to encode datatype");
    p1 += type_size;

done:
    if(SUCCEED == ret_value) {
        /* Send the datatype id & metadata to the client */
        if(MPI_SUCCESS != MPI_Send(send_buf, (int)buf_size, MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }
    else {
        /* Send the failed to the client */
        if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                                   H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
            HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");
    }

    if(send_buf)
        H5MM_free(send_buf);
    if(name)
        H5MM_xfree(name);
    printf("MDS opened datatype %d on file %d\n", type_id, obj_id);
    FUNC_LEAVE_NOAPI(ret_value)
}/* H5VL__datatype_open_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__datatype_close_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__datatype_close_cb(uint8_t *p, int source)
{
    hid_t type_id = FAIL;
    H5T_t *dt = NULL;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    /* decode the object id */
    if(H5VL__decode_datatype_close_params(p, &type_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDECODE, FAIL, "can't decode datatype close params");
    printf("MDS closing datatype %d\n", type_id);

    if(NULL == (dt = (H5T_t *)H5I_object_verify(type_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype ID");

    if((ret_value = H5T_close(dt)) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDEC, FAIL, "can't close datatype")

done:
    /* send status to client */
    if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(herr_t), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__datatype_close_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__allocate_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__allocate_cb(uint8_t *p, int source)
{
    hid_t file_id; /* metadata file ID */
    H5F_t *file = NULL; /* metadata file struct */
    H5FD_t *fd = NULL; /* metadata file driver struct */
    hid_t dxpl_id; /* encoded dxpl */
    size_t dxpl_size;
    H5FD_mem_t type; /* Memory VFD type to indicate metadata or raw data */
    hsize_t size; /* requested size to allocate from the EOA */
    hsize_t orig_size; /* Original allocation size */
    haddr_t eoa; /* Address of end-of-allocated space */
    hsize_t extra; /* Extra space to allocate, to align request */
    haddr_t return_addr = HADDR_UNDEF;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    /* the metadata file id */
    INT32DECODE(p, file_id);

    /* the memory VFD type */
    type = (H5FD_mem_t)*p++;

    /* decode the plist size */
    UINT64DECODE_VARLEN(p, dxpl_size);
    /* decode property lists if they are not default*/
    if(dxpl_size) {
        if((dxpl_id = H5P__decode(p)) < 0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTDECODE, FAIL, "unable to decode property list");
        p += dxpl_size;
    }
    else {
        dxpl_id = H5P_DATASET_XFER_DEFAULT;
    }

    /* size requested to allocate from the VFD */
    UINT64DECODE_VARLEN(p, size);

    /* get the file object */
    if(NULL == (file = (H5F_t *)H5I_object(file_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier");

    fd = file->shared->lf;
    /* Get current end-of-allocated space address */
    eoa = fd->cls->get_eoa(fd, type);

    /* Compute extra space to allocate, if this is a new block and should be aligned */
    extra = 0;
    orig_size = size;

    if(fd->alignment > 1 && orig_size >= fd->threshold) {
        hsize_t mis_align;              /* Amount EOA is misaligned */

        /* Check for EOA already aligned */
        if((mis_align = (eoa % fd->alignment)) > 0) {
            extra = fd->alignment - mis_align;
        } /* end if */
    } /* end if */

    /* Add in extra allocation amount */
    size += extra;

    /* Check for overflow when extending */
    if(H5F_addr_overflow(eoa, size) || (eoa + size) > fd->maxaddr)
        HGOTO_ERROR(H5E_VFL, H5E_NOSPACE, FAIL, "file allocation request failed");
    /* Set the [possibly aligned] address to return */
    return_addr = eoa + extra;

    /* Extend the end-of-allocated space address */
    eoa += size;
    if(fd->cls->set_eoa(fd, type, eoa) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_NOSPACE, FAIL, "file allocation request failed");

    if(H5F_super_dirty(file) < 0)
        HGOTO_ERROR(H5E_VFL, H5E_CANTMARKDIRTY, FAIL, "unable to mark superblock as dirty");

done:
    if(SUCCEED != ret_value)
        return_addr = HADDR_UNDEF;

    /* Send the haddr to the client */
    if(MPI_SUCCESS != MPI_Send(&return_addr, sizeof(uint64_t), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__allocate_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__get_eoa_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__get_eoa_cb(uint8_t *p, int source)
{
    hid_t file_id; /* metadata file ID */
    H5F_t *file = NULL; /* metadata file struct */
    H5FD_t *fd = NULL; /* metadata file driver struct */
    H5FD_mem_t type; /* Memory VFD type to indicate metadata or raw data */
    haddr_t eoa; /* Address of end-of-allocated space */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    /* the metadata file id */
    INT32DECODE(p, file_id);
    /* the memory VFD type */
    type = (H5FD_mem_t)*p++;

    /* get the file object */
    if(NULL == (file = (H5F_t *)H5I_object(file_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier");

    fd = file->shared->lf;
    /* Get current end-of-allocated space address */
    eoa = fd->cls->get_eoa(fd, type);

done:
    if(SUCCEED != ret_value)
        eoa = HADDR_UNDEF;

    /* Send the haddr to the client */
    if(MPI_SUCCESS != MPI_Send(&eoa, sizeof(uint64_t), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_eoa_cb */

/*-------------------------------------------------------------------------
* Function:	H5VL__set_eoa_cb
*------------------------------------------------------------------------- */
static herr_t
H5VL__set_eoa_cb(uint8_t *p, int source)
{
    hid_t file_id; /* metadata file ID */
    H5F_t *file = NULL; /* metadata file struct */
    H5FD_t *fd = NULL; /* metadata file driver struct */
    H5FD_mem_t type; /* Memory VFD type to indicate metadata or raw data */
    haddr_t eoa; /* Address of end-of-allocated space */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT

    /* the metadata file id */
    INT32DECODE(p, file_id);
    /* the memory VFD type */
    type = (H5FD_mem_t)*p++;
    /* the EOA to set*/
    UINT64DECODE(p, eoa);

    /* get the file object */
    if(NULL == (file = (H5F_t *)H5I_object(file_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier");

    fd = file->shared->lf;
    /* Get current end-of-allocated space address */
    ret_value = fd->cls->set_eoa(fd, type, eoa);

done:
    /* Send the confirmation to the client */
    if(MPI_SUCCESS != MPI_Send(&ret_value, sizeof(int), MPI_BYTE, source, 
                               H5VL_MDS_SEND_TAG, MPI_COMM_WORLD))
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "failed to send message");

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_MDS_SET_EOA */

#endif /* H5_HAVE_PARALLEL */
