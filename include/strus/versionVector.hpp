/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Version of the strus vector project
/// \file versionVector.hpp
#ifndef _STRUS_VECTOR_VERSION_HPP_INCLUDED
#define _STRUS_VECTOR_VERSION_HPP_INCLUDED

/// \brief strus toplevel namespace
namespace strus
{

/// \brief Version number of the strus core (storage)
#define STRUS_VECTOR_VERSION (\
	0 * 1000000\
	+ 16 * 10000\
	+ 0\
)

/// \brief Major version number of the strus vector project
#define STRUS_VECTOR_VERSION_MAJOR 0
/// \brief Minor version number of the strus vector project
#define STRUS_VECTOR_VERSION_MINOR 16

/// \brief The version of the strus vector project as string
#define STRUS_VECTOR_VERSION_STRING "0.16.0"

}//namespace
#endif
