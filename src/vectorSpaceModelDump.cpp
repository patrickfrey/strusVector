/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the contents dump of a standard vector space model storage
/// \file vectorSpaceModelDump.cpp
#include "vectorSpaceModelDump.hpp"
#include "strus/databaseOptions.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "utils.hpp"
#include <string>

using namespace strus;

#define MODULENAME   "standard vector space model"

VectorSpaceModelDump::VectorSpaceModelDump( const DatabaseInterface* database_, const std::string& configsrc, const std::string& keyprefix, ErrorBufferInterface* errorhnd_)
	:m_database(database_->createClient( configsrc))
	,m_cursor()
	,m_errorhnd(errorhnd_)
{
	m_cursor.reset( m_database->createCursor( DatabaseOptions()));
	if (!m_cursor.get()) throw strus::runtime_error(_TXT("error creating database cursor"));
	m_key = m_cursor->seekFirst( keyprefix.c_str(), keyprefix.size());
}

bool VectorSpaceModelDump::nextChunk( const char*& chunk, std::size_t& chunksize)
{
	try
	{
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' instance: %s"), MODULENAME, *m_errorhnd, false);
}


