/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program for feature vector database based sentence lexer
#include "strus/lib/vector_std.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/lib/error.hpp"
#include "strus/lib/filelocator.hpp"
#include "strus/lib/textproc.hpp"
#include "strus/textProcessorInterface.hpp"
#include "strus/vectorStorageInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/normalizerFunctionInterface.hpp"
#include "strus/normalizerFunctionInstanceInterface.hpp"
#include "strus/tokenizerFunctionInterface.hpp"
#include "strus/tokenizerFunctionInstanceInterface.hpp"
#include "strus/analyzer/token.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/fileLocatorInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "strus/sentenceLexerInstanceInterface.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/local_ptr.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/string_conv.hpp"
#include "strus/base/numstring.hpp"
#include "strus/base/pseudoRandom.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/string_format.hpp"
#include "strus/reference.hpp"
#include "vectorUtils.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstring>
#include <limits>
#include <cstdio>
#include <algorithm>

#define STRUS_DBGTRACE_COMPONENT_NAME "test"
#define VEC_EPSILON 1e-5
static bool g_verbose = false;
static strus::PseudoRandom g_random;
static strus::ErrorBufferInterface* g_errorhnd = 0;
static strus::DebugTraceInterface* g_dbgtrace = 0;
static strus::FileLocatorInterface* g_fileLocator = 0;

enum {NofDelimiters = 15, NofSpaces = 5, NofAlphaCharacters = 46};
static const int g_delimiters[ NofDelimiters] = {'?','!','/',',','-',0x2E31,0x2E31,')','(','[',']','{','}','<','>'};
static const int g_spaces[ NofSpaces] = {32,'\t',0xA0,0x2001,0x2006};
static const int g_alphaCharacters[ NofAlphaCharacters] = {'a','b','c','d','e','f','0','1','2','3','4','5','6','7','8','9',0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0x391,0x392,0x393,0x394,0x395,0x396,0x9A0,0x9A1,0x9A2,0x9A3,0x9A4,0x9A5,0x10B0,0x10B1,0x10B2,0x10B3,0x10B4,0x10B5,0x35B0,0x35B1,0x35B2,0x35B3,0x35B4,0x35B5};

static int g_dimVectors = 300;
static int g_nofTerms = 20;
static int g_nofFeatures = 10;

#define CONFIG_COVERSIM 0.8
#define DEFAULT_CONFIG  "path=vstorage;coversim=0.8"

enum Separator
{
	SeparatorLink,
	SeparatorSpace
};

struct Attributes
{
	Separator separator;
	bool withEnd;
	bool withStart;
	bool withDelimiter;
	bool withPrefix;

	Attributes()
		:separator(SeparatorLink),withEnd(false),withStart(false)
		,withDelimiter(false),withPrefix(false){}
	Attributes( const Attributes& o)
		:separator(o.separator)
		,withEnd(o.withEnd),withStart(o.withStart)
		,withDelimiter(o.withDelimiter),withPrefix(o.withPrefix){}
	explicit Attributes( int seed)
	{
		separator = (seed&1) != 0 ? SeparatorLink : SeparatorSpace;
		withEnd = (seed&2) != 0 ? true : false;
		withStart = (seed&4) != 0 ? true : false;
		withDelimiter = (seed&8) != 0 ? true : false;
		withPrefix = (seed&16) != 0 ? true : false;
	}
};

static void printUnicodeChar( std::string& out, int chr)
{
	char chrbuf[32];
	std::size_t chrlen = strus::utf8encode( chrbuf, chr);
	out.append( chrbuf, chrlen);
}

static void printSeparator( std::string& out, Separator sep)
{
	int sepchr = 0;
	switch (sep)
	{
		case SeparatorLink:
			sepchr = g_delimiters[ g_random.get( 0, NofDelimiters)];
			break;
		case SeparatorSpace:
			sepchr = g_spaces[ g_random.get( 0, NofSpaces)];
			break;
	}
	printUnicodeChar( out, sepchr);
}

static std::vector<int> getPrimeFactors( int number)
{
	std::vector<int> rt;
	while (number && (number & 1) == 0)
	{
		rt.push_back( 2);
		number /= 2;
	}
	int ni = 3, ne = (std::sqrt( number) + std::numeric_limits<float>::epsilon());
	for (; ni <= ne && number > 1; ni += 2)
	{
		while (number % ni == 0)
		{
			rt.push_back( ni);
			number /= ni;
		}
	}
	if (number >= 2)
	{
		rt.push_back( number);
	}
	return rt;
}

static bool isPrime( int number)
{
	if ((number & 1) == 0)
	{
		return (number == 2);
	}
	int ni = 3, ne = std::sqrt( number)+1;
	for (; ni < ne; ni += 2)
	{
		if (number % ni == 0) return false;
	}
	return true;
}

struct PrimeAlphaMap
{
	PrimeAlphaMap()
	{
		int lastprime = 3;
		impl[ 2] = 0;
		impl[ 3] = 1;
		int ai = 2, ae = NofAlphaCharacters;
		for (; ai != ae; ++ai)
		{
			for (lastprime+=2;
				!isPrime( lastprime)
				&& lastprime < std::numeric_limits<int>::max()-1; lastprime+=2){}
			impl[ lastprime] = ai;
		}
		maxprime = lastprime;
	}
	int operator[]( int prime) const
	{
		std::map<int,int>::const_iterator ii = impl.find( prime);
		if (ii == impl.end()) throw std::runtime_error( strus::string_format( "undefined prime %d", prime));
		return ii->second;
	}

	int randomPrime() const
	{
		std::map<int,int>::const_iterator ri = impl.lower_bound( g_random.get( 2, maxprime+1));
		if (ri == impl.end()) throw std::runtime_error("logic error calculating random prime");
		return ri->first;
	}

	int maxprime;
	std::map<int,int> impl;
};

PrimeAlphaMap g_primeAlphaMap;

struct FeatureDef
{
	std::string type;
	std::string value;

	FeatureDef( const std::string& type_, const std::string& value_)
		:type(type_),value(value_){}
	FeatureDef( const FeatureDef& o)
		:type(o.type),value(o.value){}
};

struct ProblemSpace
{
public:
	std::vector<int> elements;
	std::map<int,Attributes> attrmap;
	std::map<int,const char*> typemap;
	std::map<int,int> vecmap;
	std::vector<int> solution;
	std::vector<strus::WordVector> vectors;
	std::vector<FeatureDef> features;
	strus::Reference<strus::TokenizerFunctionInstanceInterface> tokenizer;
	strus::Reference<strus::NormalizerFunctionInstanceInterface> normalizer;

	explicit ProblemSpace( const strus::TextProcessorInterface* textproc)
	{
		const strus::TokenizerFunctionInterface* tokenizerFunc = textproc->getTokenizer( "queryfield");
		if (!tokenizerFunc) throw std::runtime_error("undefined normalizer 'queryfield'");
		const strus::NormalizerFunctionInterface* normalizerFunc = textproc->getNormalizer( "entityid");
		if (!normalizerFunc) throw std::runtime_error("undefined normalizer 'entityid'");
		tokenizer.reset( tokenizerFunc->createInstance( std::vector<std::string>(), textproc));
		if (!tokenizer.get()) throw std::runtime_error("failes to create instance of tokenizer 'queryfield'");
		normalizer.reset( normalizerFunc->createInstance( std::vector<std::string>(), textproc));
		if (!normalizer.get()) throw std::runtime_error("failes to create instance of normalizer 'entityid'");

	AGAIN:
	{
		solution = randomSolution();
		std::set<int> sset( solution.begin(), solution.end());
		std::set<int> usset = uniquePrimeFactors( solution);
		if (!primesInSupportedRange( usset)) goto AGAIN;
		std::set<int> cofactors = nonSolutionCandidates( usset, solution);

		elements.insert( elements.end(), cofactors.begin(), cofactors.end());
		elements.insert( elements.end(), sset.begin(), sset.end());
		int ei = 0, ee = elements.size();
		for (; ei != ee; ++ei)
		{
			attrmap[ elements[ ei]] = Attributes( g_random.get( 0, std::numeric_limits<short>::max()));
			const char* type = isPrime( elements[ ei])
					? (elements[ ei] > 15)
						? (elements[ ei] > 30)
							? (elements[ ei] > 45)
								? "N V A"
								: "N V"
							: "N"
						: "V"
					: "E";
			typemap[ elements[ ei]] = type;
		}
		std::vector<int>::const_iterator xi = solution.begin(), xe = solution.end();
		for (int xidx=0; xi != xe; ++xi,++xidx)
		{
			std::vector<int> factors = getPrimeFactors( *xi);
			std::vector<int>::const_iterator fi = factors.begin(), fe = factors.end();
			for (int fidx=0; fi != fe; ++fi,++fidx)
			{
				if (fidx)
				{
					attrmap[ *fi].withStart = false;
				}
				else if (xidx)
				{
					if (!attrmap[ *xi].withDelimiter)
					{
						attrmap[ *fi].withStart = false;
						attrmap[ *xi].withStart = false;
					}
				}
			}
		}
		vectors = strus::test::createDistinctRandomVectors( g_random, g_dimVectors, elements.size(), CONFIG_COVERSIM);
		int si = 0, se = usset.size();
		if (se)
		{
			strus::WordVector rootvec = vectors[ elements.size() - se];
			for (; si != se; ++si)
			{
				strus::WordVector simvec = strus::test::createSimilarVector( g_random, rootvec, 0.85);
				vectors[ elements.size() - se + si] = simvec;
			}
		}
		features = featureDefinitions();
	}}

	void insert( strus::VectorStorageClientInterface* storage)
	{
		strus::local_ptr<strus::VectorStorageTransactionInterface> transaction( storage->createTransaction());
		if (!transaction.get()) throw std::runtime_error( "failed to create vector storage transaction");
		if (g_verbose) std::cerr << "inserting feature definitions ... " << std::endl;

		transaction->defineFeatureType( "N");
		transaction->defineFeatureType( "V");
		transaction->defineFeatureType( "E");

		std::vector<FeatureDef>::const_iterator fi = features.begin(), fe = features.end();
		for (int fidx=0; fi != fe; ++fi,++fidx)
		{
			if (g_verbose) std::cerr << strus::string_format( "insert %s '%s'", fi->type.c_str(), fi->value.c_str()) << std::endl;
			transaction->defineVector( fi->type, fi->value, vectors[ fidx]);
		}
		if (!transaction->commit())
		{
			throw std::runtime_error("vectore storage transaction failed");
		}
	}

	std::vector<std::string> normalizeQueryString( const std::string& qrystr) const
	{
		std::vector<strus::analyzer::Token> fieldtok = tokenizer->tokenize( qrystr.c_str(), qrystr.size());
		std::vector<std::string> rt;
		std::vector<strus::analyzer::Token>::const_iterator ti = fieldtok.begin(), te = fieldtok.end();
		for (; ti != te; ++ti)
		{
			const char* qstr = qrystr.c_str() + ti->origpos().ofs();
			std::size_t qlen = ti->origsize();
			std::string normval = normalizer->normalize( qstr, qlen);
			if (!normval.empty() && normval[0] == 0)
			{
				throw std::runtime_error("multipart results are nopt supported");
			}
			rt.push_back( normval);
		}
		return rt;
	}

	std::vector<strus::SentenceGuess> runQuery( strus::VectorStorageClientInterface* storage) const
	{
		strus::local_ptr<strus::SentenceLexerInstanceInterface> lexer( storage->createSentenceLexer());
		if (!lexer.get()) throw std::runtime_error( "failed to create vector storage transaction");

		std::string qrystr = queryString();
		std::vector<std::string> fields = normalizeQueryString( qrystr);
		if (g_verbose)
		{
			std::string solutionstr = queryElementString();
			std::string anstring = annotatedQueryString();

			std::cerr << strus::string_format( "running query: %s", solutionstr.c_str()) << std::endl;
			std::cerr << strus::string_format( "as string: [%s]", qrystr.c_str()) << std::endl;
			std::cerr << strus::string_format( "annotated: %s", anstring.c_str()) << std::endl;
			std::vector<std::string>::const_iterator fi = fields.begin(), fe = fields.end();
			for (; fi != fe; ++fi)
			{
				std::cerr << strus::string_format( "\tfield [%s]", fi->c_str()) << std::endl;
			}
			std::vector<int>::const_iterator si = solution.begin(), se = solution.end();
			for (; si != se; ++si)
			{
				std::string fstr = featureString( *si);
				std::cerr << strus::string_format( "\telement [%s]", fstr.c_str()) << std::endl;
			}
		}
		std::vector<strus::SentenceGuess> rt = lexer->call( fields, 20/*maxNofResults*/, 0.8/*minWeight*/);

		if (g_verbose)
		{
			std::vector<strus::SentenceGuess>::const_iterator
				ri = rt.begin(), re = rt.end();
			for (int ridx=1; ri != re; ++ri,++ridx)
			{
				std::cerr << strus::string_format( "answer %d:", ridx);
				strus::SentenceTermList::const_iterator
					ti = ri->terms().begin(), te = ri->terms().end();
				for (; ti != te; ++ti)
				{
					if (ti->type().empty())
					{
						std::cerr << strus::string_format( " ? '%s'", ti->value().c_str());
					}
					else
					{
						std::cerr << strus::string_format( " %s '%s'", ti->type().c_str(), ti->value().c_str());
					}
				}
				std::cerr << strus::string_format( " = %5f", ri->weight()) << std::endl;
			}
		}
		return rt;
	}

	std::string pureSentenceTermString( const strus::SentenceTerm& term)
	{
		std::string rt;
		char const* vi = term.value().c_str();
		for (; *vi; ++vi)
		{
			if (*vi == '_' || *vi == '-' || *vi == '-')
			{
				if (!rt.empty() && rt[ rt.size()-1] != ' ')
				{
					rt.push_back( ' ');
				}
			}
			else
			{
				rt.push_back( *vi);
			}
		}
		return rt;
	}

	void verifyResult( const std::vector<strus::SentenceGuess>& result)
	{
		if (result.empty()) throw std::runtime_error( "sentence guess result is empty");
		std::vector<strus::SentenceGuess>::const_iterator
			ri = result.begin(), re = result.end();
		std::string expectedstr = pureQueryString();
		if (g_verbose)
		{
			std::cerr << "expected (pure): " << expectedstr << std::endl;
		}
		for (; ri != re && ri->weight() >= 1.0 - std::numeric_limits<double>::epsilon(); ++ri){}
		re = ri;
		ri = result.begin();
		for (; ri != re; ++ri)
		{
			std::string resultstr;
			std::vector<strus::SentenceTerm>::const_iterator
				ti = ri->terms().begin(), te = ri->terms().end();
			for (; ti != te; ++ti)
			{
				if (ti->type().empty())
				{
					throw std::runtime_error( "result not completely typed as expected");
				}
				if (!resultstr.empty() && resultstr[ resultstr.size()-1] != ' ')
				{
					resultstr.push_back( ' ');
				}
				resultstr.append( pureSentenceTermString( *ti));
			}
			resultstr = strus::string_conv::trim( resultstr);
			if (resultstr != expectedstr)
			{
				throw std::runtime_error(
					strus::string_format(
						"best result match not as expected '%s' != '%s'",
						resultstr.c_str(), expectedstr.c_str()));
			}
			else if (g_verbose)
			{
				std::cerr << "result (pure): " << resultstr << std::endl;
			}
			if (ri->terms().size() != solution.size())
			{
				throw std::runtime_error( strus::string_format( "result not covering the proposed solution one to one, result size = %d, expected size = %d", (int)ri->terms().size(), (int)solution.size()));
			}
		}
	}

private:
	static bool primesInSupportedRange( const std::set<int>& primes)
	{
		std::set<int>::const_iterator pi = primes.begin(), pe = primes.end();
		for (; pi != pe; ++pi)
		{
			if (*pi > g_primeAlphaMap.maxprime) return false;
		}
		return true;
	}

	static std::vector<int> randomSolution()
	{
		std::vector<int> rt;
		int si = 0, se = g_random.get( 1, g_nofFeatures);
		for (; si != se; ++si)
		{
			rt.push_back( g_random.get( 2, g_nofTerms));
		}
		return rt;
	}

	static std::set<int> uniquePrimeFactors( const std::vector<int>& ar)
	{
		std::set<int> rt;
		std::vector<int>::const_iterator
			xi = ar.begin(), xe = ar.end();
		for (; xi != xe; ++xi)
		{
			std::vector<int> factors = getPrimeFactors( *xi);
			rt.insert( factors.begin(), factors.end());
		}
		return rt;
	}

	static std::set<int> nonSolutionCandidates( const std::set<int>& base, const std::vector<int>& solution_)
	{
		std::set<int> rt = base;
		int qi = 0, qe = g_random.get( 0, g_nofFeatures * 10);
		for (; qi != qe; ++qi)
		{
			std::set<int>::const_iterator c1 = rt.begin(), c2 = rt.begin();
			int cidx1 = g_random.get( 1, rt.size());
			int cidx2 = g_random.get( 1, rt.size());
			while (cidx1--) {++c1;}
			while (cidx2--) {++c2;}
			
			int factor = (*c1) * (*c2);
			if (factor < 0
				|| factor > g_primeAlphaMap.maxprime * g_primeAlphaMap.maxprime
				|| g_random.get( 0,2) == 0)
			{
				rt.insert( g_primeAlphaMap.randomPrime());
			}
			else
			{
				std::vector<int>::const_iterator xi = solution_.begin(), xe = solution_.end();
				for (; xi != xe && *xi > 1 && (factor % *xi) != 0; ++xi) {}
				if (xi == xe)
				{
					rt.insert( factor);
				}
				else if (*xi <= 1)
				{
					throw std::runtime_error("logic error in test: illegal element in solution");
				}
			}
		}
		return rt;
	}

	std::string featureString( int elementid) const
	{
		std::string rt;
		std::map<int,Attributes>::const_iterator ai = attrmap.find( elementid);
		if (ai == attrmap.end()) throw std::runtime_error("undefined element (attribute map)");
		const Attributes& attr = ai->second;

		if (attr.withStart) printSeparator( rt, SeparatorLink);
		std::vector<int> factors = getPrimeFactors( elementid);
		std::vector<int>::const_iterator fi = factors.begin(), fe = factors.end();
		for (int fidx=0; fi != fe; ++fi,++fidx)
		{
			if (fidx) printSeparator( rt, attr.separator);
			int chr = g_alphaCharacters[ g_primeAlphaMap[ *fi]];
			printUnicodeChar( rt, chr);
		}
		if (attr.withEnd) printSeparator( rt, SeparatorLink);
		return rt;
	}

	std::string pureFeatureString( int elementid) const
	{
		std::string rt;
		std::vector<int> factors = getPrimeFactors( elementid);
		std::vector<int>::const_iterator fi = factors.begin(), fe = factors.end();
		for (int fidx=0; fi != fe; ++fi,++fidx)
		{
			if (fidx) rt.push_back(' ');
			int chr = g_alphaCharacters[ g_primeAlphaMap[ *fi]];
			printUnicodeChar( rt, chr);
		}
		return rt;
	}

	std::string pureQueryString() const
	{
		std::string rt;
		std::vector<int>::const_iterator si = solution.begin(), se = solution.end();
		for (int sidx=0; si != se; ++si,++sidx)
		{
			if (sidx) rt.push_back(' ');
			rt.append( pureFeatureString( *si));
		}
		return strus::string_conv::trim( rt);
	}

	static std::vector<std::string> splitSpaces( const char* src)
	{
		std::vector<std::string> rt;
		char const* si = src;
		char const* sn = std::strchr( si, ' ');
		for (; sn; si=sn+1,sn = std::strchr( si, ' '))
		{
			rt.push_back( std::string( si, sn-si));
		}
		rt.push_back( si);
		return rt;
	}

	std::vector<std::string> typeStrings( int elementid) const
	{
		std::map<int,const char*>::const_iterator ti = typemap.find( elementid);
		if (ti == typemap.end()) throw std::runtime_error("undefined element (type map)");
		return splitSpaces( ti->second);
	}

	std::vector<FeatureDef> featureDefinitions() const
	{
		std::vector<FeatureDef> rt;
		rt.reserve( elements.size());
		int ei = 0, ee = elements.size();
		for (; ei != ee; ++ei)
		{
			std::vector<std::string> typestrlist = typeStrings( elements[ ei]);
			std::string featurestr = featureString( elements[ ei]);

			std::vector<std::string>::const_iterator ti = typestrlist.begin(), te = typestrlist.end();
			for (; ti != te; ++ti)
			{
				rt.push_back( FeatureDef( *ti, normalizer->normalize( featurestr.c_str(), featurestr.size())));
			}
		}
		return rt;
	}

	std::string queryElementString() const
	{
		std::string rt;
		std::vector<int>::const_iterator si = solution.begin(), se = solution.end();
		for (int sidx=0; si != se; ++si,++sidx)
		{
			if (sidx) rt.push_back(' ');
			rt.append( strus::string_format("%d {", *si));

			std::vector<int> pfac = getPrimeFactors( *si);
			std::vector<int>::const_iterator pi = pfac.begin(), pe = pfac.end();
			for (int pidx=0; pi != pe; ++pi,++pidx)
			{
				if (pidx) rt.push_back(',');
				rt.append( strus::string_format("%d", *pi));
			}
			rt.push_back('}');
		}
		return rt;
	}

	std::string queryString() const
	{
		std::string rt;
		std::vector<int>::const_iterator si = solution.begin(), se = solution.end();
		for (int sidx=0; si != se; ++si,++sidx)
		{
			std::map<int,Attributes>::const_iterator ai = attrmap.find( *si);
			if (ai == attrmap.end()) throw std::runtime_error("undefined element (attribute map)");
			const Attributes& attr = ai->second;

			if (sidx)
			{
				if (attr.withDelimiter)
				{
					rt.push_back( '"');
				}
				else
				{
					printUnicodeChar( rt, g_spaces[ g_random.get( 0, NofSpaces)]);
				}
			}
			rt.append( featureString( *si));
		}
		return rt;
	}

	std::string annotatedQueryString() const
	{
		std::string rt;
		std::vector<int>::const_iterator si = solution.begin(), se = solution.end();
		for (int sidx=0; si != se; ++si,++sidx)
		{
			if (sidx) rt.push_back( '|');

			std::map<int,Attributes>::const_iterator ai = attrmap.find( *si);
			if (ai == attrmap.end()) throw std::runtime_error("undefined element (attribute map)");
			const Attributes& attr = ai->second;

			if (sidx)
			{
				if (attr.withDelimiter)
				{
					rt.push_back( '"');
				}
				else
				{
					rt.push_back( ' ');
				}
			}
			if (attr.withStart) rt.push_back( '-');
			std::vector<int> factors = getPrimeFactors( *si);
			std::vector<int>::const_iterator fi = factors.begin(), fe = factors.end();
			for (int fidx=0; fi != fe; ++fi,++fidx)
			{
				if (fidx) rt.push_back( attr.separator == SeparatorLink ? '-' : '_');
				int chr = g_alphaCharacters[ g_primeAlphaMap[ *fi]];
				printUnicodeChar( rt, chr);
				rt.append( strus::string_format( "{%d}", *fi));
			}
			if (attr.withStart) rt.push_back( '-');
		}
		return rt;
	}
};


static void printUsage( std::ostream& out)
{
	out << "Usage testSentenceLexer: [<options>] [<workdir>] <number of terms> <number of features>" << std::endl;
	out << "options:" << std::endl;
	out << "-h                     :print this usage" << std::endl;
	out << "-V                     :verbose output to stderr" << std::endl;
	out << "-W <SEED>              :sepecify pseudo random number generator seed to be <SEED> (int)" << std::endl;
	out << "-s <CONFIG>            :specify test configuration string as <CONFIG>" << std::endl;
	out << "-T <QIDX>              :evaluate only test case with index <QIDX> starting with 0" << std::endl;
	out << "-G <DEBUG>             :enable debug trace for <DEBUG>" << std::endl;
	out << "<workdir>              :working directory, default './'" << std::endl;
	out << "<number of terms>      :number of terms" << std::endl;
	out << "<number of features>   :number of features" << std::endl;
}

int main( int argc, const char** argv)
{
	try
	{
		int rt = 0;
		g_dbgtrace = strus::createDebugTrace_standard( 2);
		if (!g_dbgtrace) {std::cerr << "FAILED " << "strus::createDebugTrace_standard" << std::endl; return -1;}
		g_errorhnd = strus::createErrorBuffer_standard( 0, 1, g_dbgtrace);
		if (!g_errorhnd) {std::cerr << "FAILED " << "strus::createErrorBuffer_standard" << std::endl; return -1;}
		g_fileLocator = strus::createFileLocator_std( g_errorhnd);
		if (!g_fileLocator) {std::cerr << "FAILED " << "strus::createFileLocator_std" << std::endl; return -1;}

		std::string configstr( DEFAULT_CONFIG);
		std::string workdir = "./";
		bool printUsageAndExit = false;

		// Parse parameters:
		int argidx = 1;
		int testidx = -1;
		bool finished_options = false;
		while (!finished_options && argc > argidx && argv[argidx][0] == '-')
		{
			if (0==std::strcmp( argv[argidx], "-h"))
			{
				printUsage( std::cout);
				delete g_fileLocator;
				delete g_errorhnd;
				return 0;
			}
			else if (0==std::strcmp( argv[argidx], "-V"))
			{
				g_verbose = true;
			}
			else if (std::strcmp( argv[argidx], "-W") == 0)
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -W needs argument (seed)" << std::endl;
					printUsageAndExit = true;
				}
				try
				{
					g_random.init( strus::numstring_conv::touint( argv[++argidx], std::numeric_limits<int>::max()));
				}
				catch (const std::runtime_error& err)
				{
					std::cerr << "error parsing option -W: " << err.what() << std::endl;
				}
			}
			else if (0==std::strcmp( argv[argidx], "-T"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -T needs argument (test index)" << std::endl;
					printUsageAndExit = true;
				}
				try
				{
					testidx = strus::numstring_conv::touint( argv[++argidx], std::numeric_limits<int>::max());
				}
				catch (const std::runtime_error& err)
				{
					std::cerr << "error parsing option -T: " << err.what() << std::endl;
				}
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
				if (!g_dbgtrace->enable( argv[++argidx]))
				{
					std::cerr << "too many debug trace items enabled" << std::endl;
					printUsageAndExit = true;
				}
			}
			else if (0==std::strcmp( argv[argidx], "--"))
			{
				finished_options = true;
			}
			else
			{
				std::cerr << "unknown option " << argv[argidx] << std::endl;
				printUsageAndExit = true;
			}
			++argidx;
		}
		if (argc > argidx)
		{
			workdir = argv[ argidx++];
		}
		if (argc > argidx)
		{
			g_nofTerms = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<strus::Index>::max());
			if (g_nofTerms == 0) throw std::runtime_error("nof terms argument is 0");
			if (g_nofTerms < 3) throw std::runtime_error("nof terms argument is smaller than 3");
		}
		if (argc > argidx)
		{
			g_nofFeatures = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<strus::Index>::max());
			if (g_nofFeatures == 0) throw std::runtime_error("nof features argument is 0");
		}
		if (argc > argidx)
		{
			std::cerr << "too many arguments (maximum 3 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}
		if (printUsageAndExit)
		{
			printUsage( std::cerr);
			return rt;
		}
		if (g_errorhnd->hasError()) throw std::runtime_error("error in test configuration");

		if (g_verbose)
		{
			std::cerr << "model config: " << configstr << std::endl;
			std::cerr << "number of terms: " << g_nofTerms  << std::endl;
			std::cerr << "number of features: " << g_nofFeatures  << std::endl;
			std::cerr << "random seed: " << g_random.seed() << std::endl;
		}
		if (testidx >= 0 && testidx >= g_nofFeatures)
		{
			throw std::runtime_error( "test index selected with option -T is out of range");
		}
		// Build all objects:
		strus::local_ptr<strus::TextProcessorInterface>
			textproc( strus::createTextProcessor( g_fileLocator, g_errorhnd));
		strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_fileLocator, g_errorhnd));
		strus::local_ptr<strus::VectorStorageInterface> sti( strus::createVectorStorage_std( g_fileLocator, g_errorhnd));
		if (!dbi.get() || !sti.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());

		static const char* vectorconfigkeys[] = {"memtypes","vecdim","bits","variations",0};
		std::string dbconfigstr( configstr);
		removeKeysFromConfigString( dbconfigstr, vectorconfigkeys, g_errorhnd);

		// Remove traces of old test model before creating a new one:
		if (g_verbose) std::cerr << "create test repository ..." << std::endl;
		if (!dbi->destroyDatabase( dbconfigstr))
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

		// Run the test:
		if (g_verbose) std::cerr << "create test data ..." << std::endl;
		ProblemSpace testData( textproc.get());
		if (g_verbose) std::cerr << "insert test data ..." << std::endl;
		testData.insert( storage.get());
		if (g_verbose) std::cerr << "run test query ... " << std::endl;
		std::vector<strus::SentenceGuess> result = testData.runQuery( storage.get());

		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "sentence lexer failed");
		}
		// Debug output dump:
		if (!strus::dumpDebugTrace( g_dbgtrace, NULL/*filename (stderr)*/))
		{
			throw std::runtime_error( "failed to dump the debug trace");
		}
		if (g_verbose) std::cerr << "verify results ..." << std::endl;
		testData.verifyResult( result);

		std::cerr << "NOTE: the verification of the test is weak" << std::endl;
		std::cerr << "OK" << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		if (g_dbgtrace) (void)strus::dumpDebugTrace( g_dbgtrace, NULL/*filename (stderr)*/);

		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
		return 1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
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

		delete g_fileLocator;
		delete g_errorhnd;
		return 3;
	}
}



