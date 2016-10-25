/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/error.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/versionVector.hpp"
#include "strus/versionBase.hpp"
#include "strus/versionStorage.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "utils.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "databaseAdapter.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/fileio.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>

#undef STRUS_LOWLEVEL_DEBUG

static void printUsage()
{
	std::cout << "strusInspectVSM [options] <cmd> <args...>" << std::endl;
	std::cout << "options:" << std::endl;
	std::cout << "-h|--help" << std::endl;
	std::cout << "    " << _TXT("Print this usage and do nothing else") << std::endl;
	std::cout << "-v|--version" << std::endl;
	std::cout << "    " << _TXT("Print the program version and do nothing else") << std::endl;
	std::cout << "-s|--config <CFGSTR>" << std::endl;
	std::cout << "    " << _TXT("Define the model configuration string as <CFGSTR>") << std::endl;
	std::cout << "-S|--configfile <CFGFILE>" << std::endl;
	std::cout << "    " << _TXT("Define the model configuration file as <CFGFILE>") << std::endl;
	std::cout << "<cmd>        : " << _TXT("command to execute (what to inspect), one of:") << std::endl;
	std::cout << "               'simrel'      =   " << _TXT("similarity relation map") << std::endl;
	std::cout << "               'simrelterms' =   " << _TXT("similarity relation map with words") << std::endl;
}

static strus::ErrorBufferInterface* g_errorBuffer = 0;

static bool isUnsignedInt( const char* arg)
{
	char const* ai = arg;
	for (; *ai; ++ai) if (*ai < '0' || *ai > '9') return false;
	return ai != arg;
}

static unsigned int parseUnsignedInt( const char* arg)
{
	unsigned int rt = 0;
	char const* ai = arg;
	for (; *ai; ++ai)
	{
		if (*ai < '0' || *ai > '9') return 0;
		rt = rt * 10 + (*ai - '0');
	}
	return rt;
}

static void printSimilarityRelation( strus::DatabaseAdapter& database, const strus::SampleIndex& sidx, const std::string& indent=std::string())
{
	std::vector<strus::SimRelationMap::Element> elems = database.readSimRelations( sidx);
	std::vector<strus::SimRelationMap::Element>::const_iterator ei = elems.begin(), ee = elems.end();
	for (; ei != ee; ++ei)
	{
		std::string name = database.readSampleName( ei->index);
		std::cout << indent << ei->index << ": " << name << " " << ei->simdist << std::endl;
	}
}

static void printSimilarityRelation( strus::DatabaseAdapter& database, const std::string& name, const std::string& indent=std::string())
{
	strus::SampleIndex sidx = database.readSampleIndex( name);
	std::vector<strus::SimRelationMap::Element> elems = database.readSimRelations( sidx);
	std::vector<strus::SimRelationMap::Element>::const_iterator ei = elems.begin(), ee = elems.end();
	for (; ei != ee; ++ei)
	{
		std::string name = database.readSampleName( ei->index);
		std::cout << indent << ei->index << ": " << name << " " << ei->simdist << std::endl;
	}
}

static void printSimilarityRelation( strus::DatabaseAdapter& database)
{
	strus::SampleIndex si = 0, se = database.readNofSamples();
	for (; si != se; ++si)
	{
		std::string name = database.readSampleName( si);
		std::cout << si << ": " << name << ":" << std::endl;
		printSimilarityRelation( database, si, "  ");
	}
}


int main( int argc, const char* argv[])
{
	int rt = 0;
	std::auto_ptr<strus::ErrorBufferInterface> errorBuffer( strus::createErrorBuffer_standard( 0, 2));
	if (!errorBuffer.get())
	{
		std::cerr << _TXT("failed to create error buffer") << std::endl;
		return -1;
	}
	g_errorBuffer = errorBuffer.get();

	try
	{
		bool doExit = false;
		int argi = 1;
		std::string configstr;

		// Parsing arguments:
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-h") || 0==std::strcmp( argv[argi], "--help"))
			{
				printUsage();
				doExit = true;
			}
			else if (0==std::strcmp( argv[argi], "-v") || 0==std::strcmp( argv[argi], "--version"))
			{
				std::cerr << "strus base version " << STRUS_BASE_VERSION_STRING << std::endl;
				std::cerr << "strus storage version " << STRUS_STORAGE_VERSION_STRING << std::endl;
				std::cerr << "strus vector version " << STRUS_VECTOR_VERSION_STRING << std::endl;
				doExit = true;
			}
			else if (0==std::strcmp( argv[argi], "-s") || 0==std::strcmp( argv[argi], "--config"))
			{
				if (!configstr.empty())
				{
					throw strus::runtime_error( _TXT("option --config or --configfile specified twice"));
				}
				if (argi == argc || argv[argi+1][0] == '-')
				{
					throw strus::runtime_error( _TXT("no argument given to option --config"));
				}
				++argi;
				configstr = argv[ argi];
				if (configstr.empty())
				{
					throw strus::runtime_error( _TXT("option --config argument is empty"));
				}
			}
			else if (0==std::strcmp( argv[argi], "-S") || 0==std::strcmp( argv[argi], "--configfile"))
			{
				if (!configstr.empty())
				{
					throw strus::runtime_error( _TXT("option --config or --configfile specified twice"));
				}
				if (argi == argc || argv[argi+1][0] == '-')
				{
					throw strus::runtime_error( _TXT("no argument given to option --config"));
				}
				++argi;
				unsigned int ec = strus::readFile( argv[ argi], configstr);
				if (ec) throw strus::runtime_error( _TXT("error reading configuration file '%s': %s (%u)"), argv[ argi], ::strerror(ec),ec);
				if (configstr.empty())
				{
					throw strus::runtime_error( _TXT("configuration file is empty"));
				}
			}
			else if (argv[argi][0] == '-')
			{
				throw strus::runtime_error(_TXT("unknown option %s"), argv[ argi]);
			}
			else
			{
				break;
			}
		}
		if (doExit) return 0;
		if (argc - argi < 1) throw strus::runtime_error( _TXT("too few arguments (given %u, at least required %u)"), argc - argi, 1);

		std::auto_ptr<strus::DatabaseInterface> database( createDatabaseType_leveldb( g_errorBuffer));
		if (!database.get())
		{
			throw strus::runtime_error(_TXT("failed to create LevelDB database object"));
		}
		strus::VectorSpaceModelConfig config( configstr, g_errorBuffer);
		if (g_errorBuffer->hasError())
		{
			throw strus::runtime_error(_TXT("failed to parse configuration"));
		}
		std::string command( strus::utils::tolower( argv[ argi+0]));
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << strus::string_format( _TXT("execute command '%s'"), command.c_str()) << std::endl;
#endif
		if (command == "simrel")
		{
			strus::DatabaseAdapter dbadapter( database.get(), config.databaseConfig, g_errorBuffer);

			if (argc - argi > 2) throw strus::runtime_error( _TXT("too many arguments (given %u, max %u)"), argc - argi, 2);
			if (argc - argi == 1)
			{
				printSimilarityRelation( dbadapter);
			}
			else if (argv[ argi+1][0] == '#' && isUnsignedInt( argv[ argi+1]+1))
			{
				unsigned int sampleidx = parseUnsignedInt( argv[ argi+1]+1);
				printSimilarityRelation( dbadapter, sampleidx);
			}
			else
			{
				std::string samplename = argv[ argi+1];
				printSimilarityRelation( dbadapter, samplename);
			}
		}
		else
		{
			throw strus::runtime_error(_TXT("unknown program command '%s'"), command.c_str());
		}
		if (g_errorBuffer->hasError())
		{
			throw strus::runtime_error(_TXT("uncaught exception"));
		}
	}
	catch (const std::runtime_error& err)
	{
		if (g_errorBuffer->hasError())
		{
			std::cerr << "ERROR " << err.what() << ": " << g_errorBuffer->fetchError() << std::endl;
		}
		else
		{
			std::cerr << "ERROR " << err.what() << std::endl;
		}
		rt = 1;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "ERROR " << _TXT("out of memory") << std::endl;
		rt = 2;
	}
	return rt;
}



