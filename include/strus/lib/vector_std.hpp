/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Exported functions of the strus standard vector space model library
/// \file vector_std.hpp
#ifndef _STRUS_VECTOR_STORAGE_STD_LIB_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_STD_LIB_HPP_INCLUDED
#include <string>

/// \brief strus toplevel namespace
namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class VectorStorageInterface;
/// \brief Forward declaration
class DatabaseInterface;

/// \brief Create standard vector space model
/// \param[in] errorhnd error handler to use
VectorStorageInterface* createVectorStorage_std( ErrorBufferInterface* errorhnd);

}//namespace
#endif

