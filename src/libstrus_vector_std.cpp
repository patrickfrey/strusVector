/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/vector_std.hpp"
#include "strus/errorBufferInterface.hpp"
#include "vectorStorage.hpp"
#include "vectorStorageConfig.hpp"
#include "vectorStorageBuilder.hpp"
#include "strus/base/dll_tags.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"

using namespace strus;

DLL_PUBLIC VectorStorageInterface* strus::createVectorStorage_std( ErrorBufferInterface* errorhnd)
{
	try
	{
		static bool intl_initialized = false;
		if (!intl_initialized)
		{
			strus::initMessageTextDomain();
			intl_initialized = true;
		}
		return new VectorStorage( errorhnd);
	}
	CATCH_ERROR_MAP_RETURN( _TXT("error creating standard vector storage: %s"), *errorhnd, 0);
}

DLL_PUBLIC bool strus::runVectorStorageBuild_std( const DatabaseInterface* database, const std::string& configstr, const std::string& commands, ErrorBufferInterface* errorhnd)
{
	try
	{
		static bool intl_initialized = false;
		if (!intl_initialized)
		{
			strus::initMessageTextDomain();
			intl_initialized = true;
		}
		VectorStorageConfig config( configstr, errorhnd);
		VectorStorageBuilder builder( config, configstr, database, errorhnd);
		char const* si = commands.c_str();
		const char* se = si + commands.size();
		for (; si != se && (unsigned char)*si > 32; ++si){}	//... skip spaces
		if (si == se)
		{
			if (!builder.run( "")) throw strus::runtime_error(_TXT("failed to run command: %s"), errorhnd->fetchError());
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
				throw strus::runtime_error(_TXT("failed to run command '%s': %s"), cmd.c_str(), errorhnd->fetchError());
			}
		}
		return true;
	}
	CATCH_ERROR_MAP_RETURN( _TXT("error learning and assigning concepts for standard vector storage: %s"), *errorhnd, 0);
}


