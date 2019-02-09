/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program for feature vector database based sentence lexer
#include "strus/lib/vector_std.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/lib/error.hpp"
#include "strus/vectorStorageInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "strus/sentenceLexerContextInterface.hpp"
#include "strus/sentenceLexerInstanceInterface.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/local_ptr.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/numstring.hpp"
#include "strus/base/pseudoRandom.hpp"
#include "strus/base/utf8.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <cstring>
#include <limits>
#include <cstdio>
#include <algorithm>

#define VEC_EPSILON 1e-5
static bool g_verbose = false;
static strus::PseudoRandom g_random;
static strus::ErrorBufferInterface* g_errorhnd = 0;

static char g_delimiterSubst = '-';
static char g_spaceSubst = '_';
static std::vector<int> g_delimiters = {'"',0x2019,'`','\'','?','!','/',';',':','.',',','-',0x2014,')','(','[',']','{','}','<','>','_'};
static std::vector<int> g_spaces = {32,'\t',0xA0,0x2008,0x200B};
static std::vector<int> g_alphabet = {'a','b','c','d','e','f','0','1','2','3','4','5','6','7','8','9',0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0x391,0x392,0x393,0x394,0x395,0x396,0x9A0,0x9A1,0x9A2,0x9A3,0x9A4,0x9A5,0x10B0,0x10B1,0x10B2,0x10B3,0x10B4,0x10B5,0x35B0,0x35B1,0x35B2,0x35B3,0x35B4,0x35B5};
strus::Index g_nofTypes = 2;
enum  {MaxNofTypes = 1000};
strus::Index g_nofTerms = 1000;
strus::Index g_nofFeatures = 10000;
int g_maxTermLength = 30;
int g_maxFeatureTerms = 5;

#define DEFAULT_CONFIG "path=vstorage"

struct FeatureTerm
{
	std::string type;
	std::string value;

	FeatureTerm( const std::string& type_, const std::string& value_)
		:type(type_),value(value_){}
	FeatureTerm( const FeatureTerm& o)
		:type(o.type),value(o.value){}
};

static std::string typeString( int tidx)
{
	char buf[ 32];
	if (g_nofTypes >= 100)
	{
		std::snprintf( buf, sizeof(buf), "T%03d", tidx);
	}
	else if (g_nofTypes >= 10)
	{
		std::snprintf( buf, sizeof(buf), "T%02d", tidx);
	}
	else
	{
		std::snprintf( buf, sizeof(buf), "T%d", tidx);
	}
	return std::string(buf);
}

static std::string randomTerm()
{
	std::string rt;
	std::size_t length = g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, g_maxTermLength)));
	if (length == 0) length = 1;
	std::size_t li = 0, le = length;
	for (; li != le; ++li)
	{
		char chrbuf[ 32];
		int32_t chr = g_alphabet[ g_random.get( 0, 1+g_random.get( 0, g_alphabet.size()))];
		std::size_t chrlen = strus::utf8encode( chrbuf, chr);
		rt.append( chrbuf, chrlen);
	}
	return rt;
}

static std::string termListToString( const std::vector<std::string>& terms, char separator, bool seperatorAtStart, bool seperatorAtEnd)
{
	std::string rt;
	if (seperatorAtStart) rt.push_back( separator);
	std::vector<std::string>::const_iterator ti = terms.begin(), te = terms.end();
	for (int tidx=0; ti != te; ++ti,++tidx)
	{
		if (tidx) rt.push_back( separator);
		rt.append( *ti);
	}
	if (seperatorAtEnd) rt.push_back( separator);
	return rt;
}

class TestData
{
public:
	TestData()
	{
		int ti = 0, te = g_nofTerms;
		for (; ti != te; ++ti)
		{
			m_terms.push_back( randomTerm());
		}
		std::sort( m_terms.begin(), m_terms.end());

		std::set< std::vector<int> > fset;
		int nofTries = g_nofFeatures / 10 + 1000;
		while (nofTries > 0 && g_nofFeatures > (int)m_features.size())
		{
			FeatureDef fdef;
			if (fset.insert( fdef.termindices).second != true)
			{
				//... feature is not new
				--nofTries;
				continue;
			}
			nofTries = g_nofFeatures / 10 + 1000;
			m_features.push_back( fdef);
			while (g_nofFeatures > (int)m_features.size() && g_random.get( 0, (int)(g_nofFeatures/10 + 10)) == 1)
			{
				fdef.modify();
				m_features.push_back( fdef);
			}
		}
		if (g_nofFeatures > (int)m_features.size())
		{
			throw std::runtime_error("unable to create random collection, too many collisions, assuming endless loop or too many random features requested");
		}
	}

	int nofFeatures() const
	{
		return m_features.size();
	}
	std::vector<std::string> featureTerms( int featureIdx) const
	{
		std::vector<std::string> rt;
		const FeatureDef& fdef = m_features[ featureIdx];
		std::vector<int>::const_iterator ti = fdef.termindices.begin(), te = fdef.termindices.end();
		for (; ti != te; ++ti)
		{
			rt.push_back( m_terms[  *ti]);
		}
		return rt;
	}
	std::string featureString( int featureIdx) const
	{
		const FeatureDef& fdef = m_features[ featureIdx];
		return termListToString( featureTerms( featureIdx), g_delimiterSubst, fdef.seperatorAtStart, fdef.seperatorAtEnd);
	}
	std::vector<std::string> featureTypes( int featureIdx) const
	{
		std::vector<std::string> rt;
		const FeatureDef& fdef = m_features[ featureIdx];
		std::vector<int>::const_iterator ti = fdef.typeindices.begin(), te = fdef.typeindices.end();
		for (; ti != te; ++ti)
		{
			rt.push_back( typeString( *ti));
		}
		return rt;
	}

private:
	struct FeatureDef
	{
		FeatureDef( const FeatureDef& o)
			:termindices(o.termindices),typeindices(o.typeindices),seperatorAtStart(o.seperatorAtStart),seperatorAtEnd(o.seperatorAtEnd){}
		FeatureDef()
		{
			int fi = 0, fe = g_random.get( 0, 1+g_random.get( 0, g_maxFeatureTerms));
			for (; fi != fe; ++fi)
			{
				termindices.push_back( g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, g_nofTerms))));
			}
			std::set<int> typeset;
			int ti = 0, te = g_random.get( 0, 1+g_random.get( 0, 1+g_nofTypes));
			for (; ti != te; ++ti)
			{
				typeset.insert( g_random.get( 0, g_nofTypes));
			}
			typeindices.insert( typeindices.end(), typeset.begin(), typeset.end());
			seperatorAtStart = g_random.get( 1, 100 + g_nofFeatures / 100) == 1;
			seperatorAtEnd = g_random.get( 1, 100 + g_nofFeatures / 20) == 1;
		}
		void modify()
		{
			int rndidx;
			do
			{
				switch (g_random.get( 0, 4))
				{
					case 0:
						seperatorAtStart ^= true;
						break;
					case 1:
						seperatorAtEnd ^= true;
						break;
					case 2:
						rndidx = g_random.get( 0, termindices.size());
						termindices[ rndidx] = g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, g_nofTerms)));
						break;
					default:
						termindices.push_back( g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, g_nofTerms))));
						break;
				}
			}
			while (g_random.get( 0, 4) == 1);
		}

		std::vector<int> termindices;
		std::vector<int> typeindices;
		bool seperatorAtStart;
		bool seperatorAtEnd;
	};
	struct QueryDef
	{
		
	};

private:
	std::vector<std::string> m_terms;
	std::vector<FeatureDef> m_features;
};

void instantiateLexer( strus::SentenceLexerInstanceInterface* lexer)
{
	std::vector<int>::const_iterator di = g_delimiters.begin(), de = g_delimiters.end();
	for (; di != de; ++di)
	{
		lexer->addLink( *di, g_delimiterSubst, 1);
	}
	di = g_spaces.begin(), de = g_spaces.end();
	{
		lexer->addLink( *di, g_spaceSubst, 0);
	}
	lexer->addSeparator( '"');
	lexer->addSeparator( '\t');
	lexer->addSeparator( ';');
}

void insertTestData( strus::VectorStorageClientInterface* storage, const TestData& testdata)
{
	strus::local_ptr<strus::VectorStorageTransactionInterface> transaction( storage->createTransaction());
	if (!transaction.get()) throw std::runtime_error( "failed to create vector storage transaction");
	{
		int nofFeatureDefinitions = 0;
		if (g_verbose) std::cerr << "inserting test data (feature definitions) ... " << std::endl;
		int fi = 0, fe = testdata.nofFeatures();
		for (; fi != fe; ++fi)
		{
			std::string feat = testdata.featureString( fi);
			std::vector<std::string> types = testdata.featureTypes( fi);
			std::vector<std::string>::const_iterator ti = types.begin(), te = types.end();
			for (; ti != te; ++ti)
			{
				++nofFeatureDefinitions;
				transaction->defineFeature( *ti, feat);
			}
		}
		transaction->commit();
		if (g_verbose) std::cerr << nofFeatureDefinitions << " features inserted" << std::endl;
	}
}

int main( int argc, const char** argv)
{
	try
	{
		int rt = 0;
		strus::DebugTraceInterface* dbgtrace = strus::createDebugTrace_standard( 2);
		if (!dbgtrace) throw std::runtime_error("failed to create debug trace interface");
		g_errorhnd = strus::createErrorBuffer_standard( 0, 1, dbgtrace);
		if (!g_errorhnd) throw std::runtime_error("failed to create error buffer structure");

		std::string configstr( DEFAULT_CONFIG);
		std::string workdir = "./";
		bool printUsageAndExit = false;

		// Parse parameters:
		int argidx = 1;
		bool finished_options = false;
		while (!finished_options && argc > argidx && argv[argidx][0] == '-')
		{
			if (0==std::strcmp( argv[argidx], "-h"))
			{
				printUsageAndExit = true;
			}
			else if (0==std::strcmp( argv[argidx], "-V"))
			{
				g_verbose = true;
			}
			else if (0==std::strcmp( argv[argidx], "-s"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -s needs argument (configuration string)" << std::endl;
					printUsageAndExit = true;
				}
				configstr = argv[++argidx];
			}
			else if (0==std::strcmp( argv[argidx], "-G"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -G needs argument (configuration string)" << std::endl;
					printUsageAndExit = true;
				}
				if (!dbgtrace->enable( argv[++argidx]))
				{
					std::cerr << "too many debug trace items enabled" << std::endl;
					printUsageAndExit = true;
				}
			}
			else if (0==std::strcmp( argv[argidx], "--"))
			{
				finished_options = true;
			}
			++argidx;
		}
		if (argc > argidx)
		{
			workdir = argv[ argidx++];
		}
		if (argc > argidx)
		{
			g_nofTypes = strus::numstring_conv::touint( argv[ argidx++], MaxNofTypes);
			if (g_nofTypes == 0) throw std::runtime_error("nof types argument is 0");
		}
		if (argc > argidx)
		{
			g_nofTerms = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<strus::Index>::max());
			if (g_nofTerms == 0) throw std::runtime_error("nof terms argument is 0");
		}
		if (argc > argidx)
		{
			g_nofFeatures = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<strus::Index>::max());
			if (g_nofFeatures == 0) throw std::runtime_error("nof features argument is 0");
		}
		if (argc > argidx)
		{
			g_maxFeatureTerms = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
			if (g_maxFeatureTerms == 0) throw std::runtime_error("max feature terms argument is 0");
		}
		if (argc > argidx)
		{
			g_maxTermLength = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
			if (g_maxTermLength == 0) throw std::runtime_error("max term length argument is 0");
		}
		if (argc > argidx)
		{
			std::cerr << "too many arguments (maximum 5 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}

		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<options>] [<workdir>] [<number of types> [<number of terms> [<number of features> [<max feature terms> [<max term length>]]]]]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h                     : print this usage" << std::endl;
			std::cerr << "-V                     : verbose output to stderr" << std::endl;
			std::cerr << "-s <CONFIG>            :specify test configuration string as <CONFIG>" << std::endl;
			std::cerr << "-G <DEBUG>             :enable debug trace for <DEBUG>" << std::endl;
			std::cerr << "<workdir>              :working directory, default './'" << std::endl;
			std::cerr << "<number of types>      :number of types, default 2" << std::endl;
			std::cerr << "<number of terms>      :number of terms, default 1000" << std::endl;
			std::cerr << "<number of features>   :number of features, default 10000" << std::endl;
			std::cerr << "<max feature terms>    :maximum number of terms in a feature, default 5" << std::endl;
			std::cerr << "<max term length>      :maximum length of a term, default 30" << std::endl;
			return rt;
		}
		if (g_errorhnd->hasError()) throw std::runtime_error("error in test configuration");

		if (g_verbose)
		{
			std::cerr << "model config: " << configstr << std::endl;
			std::cerr << "number of types: " << g_nofTypes  << std::endl;
			std::cerr << "number of terms: " << g_nofTerms  << std::endl;
			std::cerr << "number of features: " << g_nofFeatures  << std::endl;
			std::cerr << "maximum number of feature terms: " << g_maxFeatureTerms  << std::endl;
			std::cerr << "maximum length of a term: " << g_maxTermLength  << std::endl;
		}

		// Build all objects:
		strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( workdir, g_errorhnd));
		strus::local_ptr<strus::VectorStorageInterface> sti( strus::createVectorStorage_std( workdir, g_errorhnd));
		if (!dbi.get() || !sti.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());

		// Remove traces of old test model before creating a new one:
		if (g_verbose) std::cerr << "create test repository ..." << std::endl;
		if (!dbi->destroyDatabase( configstr))
		{
			(void)g_errorhnd->fetchError();
		}
		// Creating the vector storage:
		if (!sti->createStorage( configstr, dbi.get()))
		{
			throw std::runtime_error( g_errorhnd->fetchError());
		}
		strus::local_ptr<strus::VectorStorageClientInterface> storage( sti->createClient( configstr, dbi.get()));
		if (!storage.get()) throw std::runtime_error( g_errorhnd->fetchError());

		// Insert the feature definitions:
		TestData testdata;
		insertTestData( storage.get(), testdata);

		// Creating the lexer:
		strus::local_ptr<strus::SentenceLexerInstanceInterface> lexer( storage->createSentenceLexer());
		if (!lexer.get()) throw std::runtime_error( "failed to create sentence lexer of vector storage");
		instantiateLexer( lexer.get());

		// Debug output dump:
		if (!strus::dumpDebugTrace( dbgtrace, NULL/*filename (stderr)*/))
		{
			throw std::runtime_error( "failed to dump the debug trace");
		}
		std::cerr << "done" << std::endl;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;
		return 1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		return 2;
	}
	catch (const std::logic_error& err)
	{
		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;
		return 3;
	}
}



