/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the contents dump of a standard vector space model storage
/// \file vectorSpaceModelDump.hpp
#ifndef _STRUS_VECTOR_SPACE_MODEL_DUMP_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_DUMP_IMPLEMENTATION_HPP_INCLUDED
#include "strus/vectorSpaceModelDumpInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/databaseClientInterface.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/reference.hpp"
#include <string>

namespace strus
{
/// \brief Forward declaration
class ErrorBufferInterface;

/// \brief Interface for fetching the dump of a strus vector space model storage
class VectorSpaceModelDump
	:public VectorSpaceModelDumpInterface
{
public:
	VectorSpaceModelDump( const DatabaseInterface* database_, const std::string& configsrc, const std::string& keyprefix, ErrorBufferInterface* errorhnd_);
	virtual ~VectorSpaceModelDump(){}

	virtual bool nextChunk( const char*& chunk, std::size_t& chunksize);

private:
	const DatabaseClientInterface* m_database;
	Reference<DatabaseCursorInterface> m_cursor;
	DatabaseCursorInterface::Slice m_key;
	std::string m_chunk;
	ErrorBufferInterface* m_errorhnd;			///< error buffer for exception free interface
};

}//namespace
#endif

