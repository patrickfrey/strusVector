/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage interface
#include "vectorStorage.hpp"
#include "vectorStorageClient.hpp"
#include "vectorStorageDump.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/configParser.hpp"
#include "databaseAdapter.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "logger.hpp"
#include "simHash.hpp"
#include "simHashMap.hpp"
#include "getSimhashValues.hpp"
#include "lshModel.hpp"
#include "stringList.hpp"
#include "armadillo"
#include <memory>
#include <limits>

using namespace strus;
#define MODULENAME   "vector storage"
#define STRUS_DBGTRACE_COMPONENT_NAME "vector"

VectorStorage::VectorStorage( const std::string& workdir_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_workdir(workdir_)
{
	DebugTraceInterface* dbg = m_errorhnd->debugTrace();
	m_debugtrace = dbg ? dbg->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME) : NULL;
}

VectorStorage::~VectorStorage()
{
	if (m_debugtrace) delete m_debugtrace;
}

bool VectorStorage::createStorage( const std::string& configsource, const DatabaseInterface* dbi) const
{
	try
	{
		if (m_debugtrace) m_debugtrace->open( "create storage", configsource);
		std::string configstring( configsource);
		Config config;
		unsigned int value;
		if (strus::extractUIntFromConfigString( value, configstring, "vecdim", m_errorhnd))
		{
			if (m_debugtrace) m_debugtrace->event( "param", "vecdim %d", value);
			config = Config( value);
		}
		if (strus::extractUIntFromConfigString( value, configstring, "simdist", m_errorhnd))
		{
			if (m_debugtrace) m_debugtrace->event( "param", "simdist %d", value);
			config.simdist = value;
		}
		if (strus::extractUIntFromConfigString( value, configstring, "probsimdist", m_errorhnd))
		{
			if (m_debugtrace) m_debugtrace->event( "param", "probsimdist %d", value);
			config.probsimdist = value;
		}
		if (strus::extractUIntFromConfigString( value, configstring, "bits", m_errorhnd))
		{
			if (m_debugtrace) m_debugtrace->event( "param", "bits %d", value);
			config.bits = value;
		}
		if (strus::extractUIntFromConfigString( value, configstring, "variations", m_errorhnd))
		{
			if (m_debugtrace) m_debugtrace->event( "param", "variations %d", value);
			config.variations = value;
		}
		if (m_debugtrace) m_debugtrace->close();
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("error reading vector storage configuration: %s"), m_errorhnd->fetchError());
		}
		LshModel lshmodel( config.vecdim, config.bits, config.variations, config.simdist, config.probsimdist);
		if (dbi->createDatabase( configstring))
		{
			DatabaseAdapter database( dbi, configstring, m_errorhnd);
			Reference<DatabaseAdapter::Transaction> transaction( database.createTransaction());
			transaction->writeVersion();
			transaction->writeVariable( "config", configsource);
			transaction->writeLshModel( lshmodel);
			if (!transaction->commit())
			{
				throw strus::runtime_error(_TXT("failed to initialize vector storage: %s"), m_errorhnd->fetchError());
			}
		}
		else
		{
			throw strus::runtime_error(_TXT("failed to create repository for vector storage: %s"), m_errorhnd->fetchError());
		}
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' repository: %s"), MODULENAME, *m_errorhnd, false);
}

static std::string extractConfigParamsOpenStorage( const std::string& configsource, DebugTraceContextInterface* debugtrace, ErrorBufferInterface* errorhnd)
{
	if (debugtrace) debugtrace->open( "open storage", configsource);
	std::string configstring( configsource);
	std::string value;
	if (strus::extractStringFromConfigString( value, configstring, "vecdim", errorhnd))
	{
		if (debugtrace) debugtrace->event( "warning", "param '%s' only allowed on storage creation and has no effect", "vecdim");
	}
	if (strus::extractStringFromConfigString( value, configstring, "bits", errorhnd))
	{
		if (debugtrace) debugtrace->event( "warning", "param '%s' only allowed on storage creation and has no effect", "bits");
	}
	if (strus::extractStringFromConfigString( value, configstring, "variations", errorhnd))
	{
		if (debugtrace) debugtrace->event( "warning", "param '%s' only allowed on storage creation and has no effect", "variations");
	}
	if (debugtrace) debugtrace->close();
	if (errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("error reading vector storage configuration: %s"), errorhnd->fetchError());
	}
	return configstring;
}

VectorStorageClientInterface* VectorStorage::createClient( const std::string& configsource, const DatabaseInterface* dbi) const
{
	try
	{
		std::string configstring = extractConfigParamsOpenStorage( configsource, m_debugtrace, m_errorhnd);
		return new VectorStorageClient( dbi, configstring, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' client interface: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorStorageDumpInterface* VectorStorage::createDump( const std::string& configsource, const DatabaseInterface* dbi) const
{
	try
	{
		std::string configstring = extractConfigParamsOpenStorage( configsource, m_debugtrace, m_errorhnd);
		return new VectorStorageDump( dbi, configstring, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' dump: %s"), MODULENAME, *m_errorhnd, 0);
}


