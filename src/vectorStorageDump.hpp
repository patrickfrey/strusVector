/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the contents dump of a standard vector storage
/// \file vectorStorageDump.hpp
#ifndef _STRUS_VECTOR_STORAGE_DUMP_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_DUMP_IMPLEMENTATION_HPP_INCLUDED
#include "strus/vectorStorageDumpInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/databaseClientInterface.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/reference.hpp"
#include "databaseAdapter.hpp"
#include <string>

namespace strus
{
/// \brief Forward declaration
class ErrorBufferInterface;

/// \brief Interface for fetching the dump of a strus vector storage
class VectorStorageDump
	:public VectorStorageDumpInterface
{
public:
	VectorStorageDump( const DatabaseInterface* database_, const std::string& configsrc, const std::string& keyprefix_, ErrorBufferInterface* errorhnd_);
	virtual ~VectorStorageDump(){}

	virtual bool nextChunk( const char*& chunk, std::size_t& chunksize);

	/// \brief How many key/value pairs to return in one chunk
	enum {NofKeyValuePairsPerChunk=256};

private:
	DatabaseAdapter m_database;
	std::string m_chunk;
	std::string m_keyprefix;
	ErrorBufferInterface* m_errorhnd;			///< error buffer for exception free interface
};

}//namespace
#endif

