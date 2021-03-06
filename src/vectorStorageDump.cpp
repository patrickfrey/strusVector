/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the contents dump of a vector storage
/// \file vectorStorageDump.cpp
#include "vectorStorageDump.hpp"
#include "databaseAdapter.hpp"
#include "databaseHelpers.hpp"
#include "strus/storage/databaseOptions.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include <string>
#include <iostream>
#include <sstream>

using namespace strus;

#define MODULENAME   "vector storage"

VectorStorageDump::VectorStorageDump( const DatabaseInterface* database_, const std::string& configsrc, ErrorBufferInterface* errorhnd_)
	:m_database(database_,configsrc,errorhnd_)
	,m_itr()
	,m_chunk()
	,m_errorhnd(errorhnd_)
{
	m_database.checkVersion();
}

bool VectorStorageDump::nextChunk( const char*& chunk, std::size_t& chunksize)
{
	try
	{
		std::ostringstream output;
		std::size_t rowcnt = 0;
		if (!m_itr.get())
		{
			m_itr.reset( m_database.createDumpIterator());
			if (!m_itr.get()) throw std::runtime_error( _TXT("failed to create vector storage dump cursor"));
		}
		while (rowcnt++ <= NofKeyValuePairsPerChunk && m_itr->dumpNext( output)){}
		m_chunk = output.str();
		chunk = m_chunk.c_str();
		chunksize = m_chunk.size();
		return (chunksize != 0);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error fetching next chunk of '%s' dump: %s"), MODULENAME, *m_errorhnd, false);
}


