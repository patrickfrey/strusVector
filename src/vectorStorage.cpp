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
#include "vectorStorageBuilder.hpp"
#include "vectorStorageConfig.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/vectorStorageSearchInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include "vectorStorageConfig.hpp"
#include "databaseAdapter.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "simHash.hpp"
#include "simHashMap.hpp"
#include "simRelationMap.hpp"
#include "simRelationReader.hpp"
#include "simRelationMapBuilder.hpp"
#include "getSimhashValues.hpp"
#include "lshModel.hpp"
#include "genModel.hpp"
#include "stringList.hpp"
#include "armadillo"
#include <memory>
#include <limits>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using namespace strus;
#define MODULENAME   "standard vector storage"

#undef STRUS_LOWLEVEL_DEBUG

#define MAIN_CONCEPT_CLASSNAME ""



VectorStorage::VectorStorage( ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_){}


bool VectorStorage::createStorage( const std::string& configsource, const DatabaseInterface* dbi) const
{
	try
	{
		VectorStorageConfig config( configsource, m_errorhnd);
		if (dbi->createDatabase( config.databaseConfig))
		{
			DatabaseAdapter database( dbi, config.databaseConfig, m_errorhnd);
			Reference<DatabaseAdapter::Transaction> transaction( database.createTransaction());
			transaction->writeVersion();
			transaction->writeConfig( config);
			LshModel lshmodel( config.dim, config.bits, config.variations);
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

VectorStorageClientInterface* VectorStorage::createClient( const std::string& configsrc, const DatabaseInterface* database) const
{
	try
	{
		return new VectorStorageClient( VectorStorageConfig(configsrc,m_errorhnd), configsrc, database, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' client interface: %s"), MODULENAME, *m_errorhnd, 0);
}

VectorStorageDumpInterface* VectorStorage::createDump( const std::string& configsource, const DatabaseInterface* database, const std::string& keyprefix) const
{
	try
	{
		return new VectorStorageDump( database, configsource, keyprefix, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' dump: %s"), MODULENAME, *m_errorhnd, 0);
}

bool VectorStorage::runBuild( const std::string& commands, const std::string& configstr, const DatabaseInterface* database)
{
	try
	{
		VectorStorageConfig config( configstr, m_errorhnd);
		VectorStorageBuilder builder( config, configstr, database, m_errorhnd);
		char const* si = commands.c_str();
		const char* se = si + commands.size();
		for (; si != se && (unsigned char)*si > 32; ++si){}	//... skip spaces
		if (si == se)
		{
			if (!builder.run( "")) throw strus::runtime_error(_TXT("failed to run command: %s"), m_errorhnd->fetchError());
		}
		else while (si < se)
		{
			for (; si != se && (unsigned char)*si > 32; ++si){}	//... skip spaces
			if (si == se) break;
			std::string cmd;
			char const* start = si;
			for (; si != se; ++si)
			{
				if (*si == ';')
				{
					cmd.append( start, si - start);
					++si;
					break;
				}
			}
			if (!builder.run( cmd))
			{
				throw strus::runtime_error(_TXT("failed to run command '%s': %s"), cmd.c_str(), m_errorhnd->fetchError());
			}
		}
		return true;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error creating '%s' dump: %s"), MODULENAME, *m_errorhnd, false);
}


