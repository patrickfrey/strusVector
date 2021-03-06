/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Exported functions of the strus standard vector storage interface library
/// \file vector_std.hpp
#ifndef _STRUS_VECTOR_STORAGE_STD_LIB_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_STD_LIB_HPP_INCLUDED
#include <string>

/// \brief strus toplevel namespace
namespace strus {

/// \brief Forward declaration
class FileLocatorInterface;
/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class VectorStorageInterface;
/// \brief Forward declaration
class DatabaseInterface;

/// \brief Create standard vector storage interface
/// \param[in] filelocator interface to locate files to read or the working directory where to write files to
/// \param[in] errorhnd error handler to use
VectorStorageInterface* createVectorStorage_std( const FileLocatorInterface* filelocator, ErrorBufferInterface* errorhnd);

}//namespace
#endif

