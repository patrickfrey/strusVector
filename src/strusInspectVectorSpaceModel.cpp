/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/error.hpp"
#include "strus/versionVector.hpp"
#include "strus/versionBase.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "utils.hpp"
#include "vectorSpaceModelConfig.hpp"
#include "vectorSpaceModelFiles.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>

#define STRUS_LOWLEVEL_DEBUG

static void printUsage()
{
	std::cout << "strusInspectVectorSpaceModel [options] <cmd> <args...>" << std::endl;
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

static void printSimilarityRelationMapWithWords( const strus::VectorSpaceModelConfig& config)
{
	strus::SimRelationMap simrelmap( strus::readSimRelationMapFromFile( config.path + strus::dirSeparator() + SIMRELFILE));
	strus::StringList sampleNames = strus::readSampleNamesFromFile( config.path + strus::dirSeparator() + VECNAMEFILE);

	strus::SampleIndex si=0, se=simrelmap.nofSamples();
	for (; si != se; ++si)
	{
		strus::SimRelationMap::Row row = simrelmap.row( si);
		strus::SimRelationMap::Row::const_iterator ri = row.begin(), re = row.end();
		for (; ri != re; ++ri)
		{
			std::cout << sampleNames[si] << " " << sampleNames[ri->index] << " " << ri->simdist << std::endl;
		}
	}
}

static void printSimilarityRelationMap( const strus::VectorSpaceModelConfig& config)
{
	strus::SimRelationMap simrelmap( strus::readSimRelationMapFromFile( config.path + strus::dirSeparator() + SIMRELFILE));

	strus::SampleIndex si=0, se=simrelmap.nofSamples();
	for (; si != se; ++si)
	{
		strus::SimRelationMap::Row row = simrelmap.row( si);
		strus::SimRelationMap::Row::const_iterator ri = row.begin(), re = row.end();
		for (; ri != re; ++ri)
		{
			std::cout << si << " " << ri->index << " " << ri->simdist << std::endl;
		}
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
		if (argc - argi > 1) throw strus::runtime_error( _TXT("too many arguments (given %u, at least required %u)"), argc - argi, 1);

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
			printSimilarityRelationMap( config);
		}
		else if (command == "simrelterms")
		{
			printSimilarityRelationMapWithWords( config);
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



