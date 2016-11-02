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
#include "databaseAdapter.hpp"
#include "databaseHelpers.hpp"
#include "strus/databaseOptions.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "utils.hpp"
#include <string>
#include <iostream>
#include <sstream>

using namespace strus;

#define MODULENAME   "standard vector space model"

VectorSpaceModelDump::VectorSpaceModelDump( const DatabaseInterface* database_, const std::string& configsrc, const std::string& keyprefix_, ErrorBufferInterface* errorhnd_)
	:m_database(database_,configsrc,errorhnd_)
	,m_chunk()
	,m_keyprefix(keyprefix_)
	,m_errorhnd(errorhnd_)
{
}

bool VectorSpaceModelDump::nextChunk( const char*& chunk, std::size_t& chunksize)
{
	try
	{
		std::ostringstream output;
		std::size_t rowcnt = 0;
		if (m_chunk.empty())
		{
			if (!m_database.dumpFirst( output, m_keyprefix)) return false;
			rowcnt += 1;
		}
		while (rowcnt++ <= NofKeyValuePairsPerChunk && m_database.dumpNext( output)){}
		m_chunk = output.str();
		chunk = m_chunk.c_str();
		chunksize = m_chunk.size();
		return (chunksize != 0);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error fetching next chunk of '%s' dump: %s"), MODULENAME, *m_errorhnd, false);
}


