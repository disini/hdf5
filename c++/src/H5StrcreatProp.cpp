/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <string>

#include "H5Include.h"
#include "H5Exception.h"
#include "H5IdComponent.h"
#include "H5PropList.h"
#include "H5StrcreatProp.h"

namespace H5 {

//--------------------------------------------------------------------------
// Function:    StrCreatPropList::setCharEncoding
///\brief       Sets the character encoding of the string.
///\exception   H5::PropListIException
// March, 2018
//--------------------------------------------------------------------------
void StrCreatPropList::setCharEncoding(H5T_cset_t encoding) const
{
    herr_t ret_value = H5Pset_char_encoding(id, encoding);
    // Throw exception if H5Pset_char_encoding returns failure
    if (ret_value < 0)
    {
        throw PropListIException("setCharEncoding", "H5Pset_char_encoding failed");
    }
}

//--------------------------------------------------------------------------
// Function:    StrCreatPropList::getCharEncoding
///\brief       Gets the character encoding of the string.
///\exception   H5::PropListIException
// March, 2018
//--------------------------------------------------------------------------
H5T_cset_t StrCreatPropList::getCharEncoding() const
{
    H5T_cset_t encoding;
    herr_t ret_value = H5Pget_char_encoding(id, &encoding);
    // Throw exception if H5Pget_char_encoding returns failure
    if (ret_value < 0)
    {
        throw PropListIException("getCharEncoding", "H5Pget_char_encoding failed");
    }
    return(encoding);
}

} // end namespace
