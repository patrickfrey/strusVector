/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program
#include "strus/lib/vector_std.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/lib/error.hpp"
#include "strus/lib/filelocator.hpp"
#include "strus/index.hpp"
#include "strus/wordVector.hpp"
#include "strus/vectorStorageInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/local_ptr.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/numstring.hpp"
#include "strus/base/pseudoRandom.hpp"
#include "vectorStorage.hpp"
#include "lshModel.hpp"
#include "simHash.hpp"
#include "databaseAdapter.hpp"
#include "armadillo"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <iomanip>
#include <cstdlib>
#include <limits>

#define VEC_EPSILON  (1.0E-8)

static void initRandomNumberGenerator()
{
	time_t nowtime;
	struct tm* now;

	::time( &nowtime);
	now = ::localtime( &nowtime);

	unsigned int seed = (now->tm_year+10000) + (now->tm_mon+100) + (now->tm_mday+1);
	std::srand( seed+2);
}

static strus::WordVector convertVectorStd( const arma::fvec& vec)
{
	strus::WordVector rt;
	arma::fvec::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		rt.push_back( *vi);
	}
	return rt;
}

static strus::WordVector getRandomVector( unsigned int dim)
{
	return convertVectorStd( (arma::randu<arma::fvec>( dim) - 0.5) * 2.0); // values between -1.0 and 1.0
}

static strus::ErrorBufferInterface* g_errorhnd = 0;
static strus::FileLocatorInterface* g_fileLocator = 0;

static std::string getFeatureName( unsigned int idx)
{
	return strus::string_format( "F%u", idx);
}

static std::string getTypeName( unsigned int idx)
{
	return strus::string_format( "T%u", idx);
}

template <typename Element>
static void printArray( std::ostream& out, const std::vector<Element>& ar)
{
	typename std::vector<Element>::const_iterator ai = ar.begin(), ae = ar.end();
	for (int aidx=0; ai != ae; ++ai,++aidx)
	{
		if (aidx) out << " ";
		out << *ai;
	}
}

struct TestDataset
{
public:
	TestDataset( unsigned int nofTypes_, unsigned int nofFeatures_, const std::string& configstr)
		:m_nofTypes(nofTypes_)
		,m_nofFeatures(nofFeatures_)
		,m_config()
		,m_vectors()
		,m_nullvec()
		,m_featmap()
	{
		std::string configstrcopy( configstr);
		unsigned int vecsize = 0;
		if (strus::extractUIntFromConfigString( vecsize, configstrcopy, "vecdim", g_errorhnd))
		{
			m_config = strus::VectorStorage::Config( vecsize);
		}
		unsigned int ti = 1, te = m_nofTypes;
		for (; ti <= te; ++ti)
		{
			unsigned int ni = 1, ne = m_nofFeatures;
			for (; ni <= ne; ++ni)
			{
				if (std::rand() % 2 == 1)
				{
					int vecidx = 0;
					if (std::rand() % 2 == 1)
					{
						strus::WordVector vec = getRandomVector( vecsize);
						m_vectors.push_back( vec);
						vecidx = m_vectors.size();
					}
					m_featmap.insert( std::pair<FeatDef,int>( FeatDef( ti, ni), vecidx));
				}
			}
		}
	}

	struct FeatDef
	{
		strus::Index typeno;
		strus::Index featno;

		FeatDef()
			:typeno(0),featno(0){}
		FeatDef( strus::Index typeno_, strus::Index featno_)
			:typeno(typeno_),featno(featno_){}
		FeatDef( const FeatDef& o)
			:typeno(o.typeno),featno(o.featno){}

		bool operator < (const FeatDef& o) const
		{
			return typeno == o.typeno ? (featno < o.featno) : (typeno < o.typeno);
		}
	};

	const strus::Index& nofTypes() const			{return m_nofTypes;}
	const strus::Index& nofFeatures() const			{return m_nofFeatures;}
	const strus::VectorStorage::Config& config() const	{return m_config;}
	const std::vector<strus::WordVector>& vectors() const	{return m_vectors;}
	const std::map<FeatDef,int>& featmap() const		{return m_featmap;}

	const strus::WordVector& vector( int typeno, int featno) const
	{
		std::map<FeatDef,int>::const_iterator fi = m_featmap.find( FeatDef( typeno, featno));
		if (fi == m_featmap.end() || fi->second == 0) return m_nullvec;
		else return m_vectors[ fi->second-1];
	}
	int nofVectors( int typeno) const
	{
		int rt = 0;
		std::map<FeatDef,int>::const_iterator fi = m_featmap.begin(), fe = m_featmap.end();
		for (; fi != fe; ++fi)
		{
			if (fi->first.typeno == typeno && fi->second != 0) ++rt;
		}
		return rt;
	}

private:
	strus::Index m_nofTypes;
	strus::Index m_nofFeatures;
	strus::VectorStorage::Config m_config;
	std::vector<strus::WordVector> m_vectors;
	strus::WordVector m_nullvec;
	std::map<FeatDef,int> m_featmap;
};

struct VariableDef
{
	const char* type;
	const char* value;
};

static const VariableDef g_variables[] = {{"Variable1", "Value1"},{"Variable2", "Value2"},{0,0}};

static void writeDatabase( const std::string& testdir, const std::string& configstr, const TestDataset& dataset, const strus::LshModel& model)
{
	strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_fileLocator, g_errorhnd));
	if (dbi->exists( configstr))
	{
		if (!dbi->destroyDatabase( configstr))
		{
			throw std::runtime_error("could not destroy old test database");
		}
	}
	if (!dbi->createDatabase( configstr))
	{
		throw std::runtime_error("could not create new test database");
	}
	strus::DatabaseAdapter database( dbi.get(), configstr, g_errorhnd);
	strus::Reference<strus::DatabaseAdapter::Transaction> transaction( database.createTransaction());

	transaction->writeVersion();
	const VariableDef* vi = g_variables;
	for (; vi->type; ++vi)
	{
		transaction->writeVariable( vi->type, vi->value);
	}
	int ti = 1, te = dataset.nofTypes();
	for (; ti <= te; ++ti)
	{
		transaction->writeType( getTypeName( ti), ti);
	}
	transaction->writeNofTypeno( dataset.nofTypes());

	int ni = 1, ne = dataset.nofFeatures();
	for (; ni <= ne; ++ni)
	{
		transaction->writeFeature( getFeatureName( ni), ni);
	}
	transaction->writeNofFeatno( dataset.nofFeatures());

	std::map<strus::Index,std::vector<strus::Index> > featureTypeRelations;
	std::map<TestDataset::FeatDef,int>::const_iterator mi = dataset.featmap().begin(), me = dataset.featmap().end();
	for (; mi != me; ++mi)
	{
		std::map<strus::Index,std::vector<strus::Index> >::iterator ri = featureTypeRelations.find( mi->first.featno);
		if (ri == featureTypeRelations.end())
		{
			std::vector<strus::Index> elems;
			elems.push_back( mi->first.typeno);
			featureTypeRelations.insert( std::pair<strus::Index,std::vector<strus::Index> >( mi->first.featno, elems) );
		}
		else
		{
			ri->second.push_back( mi->first.typeno);
		}
	}
	std::map<strus::Index,std::vector<strus::Index> >::iterator ri = featureTypeRelations.begin(), re = featureTypeRelations.end();
	for (; ri != re; ++ri)
	{
		transaction->writeFeatureTypeRelations( ri->first, ri->second);
	}
	transaction->writeLshModel( model);
	for (ti=1; ti <= te; ++ti)
	{
		transaction->writeNofVectors( ti, dataset.nofVectors( ti));
		for (ni = 1; ni <= ne; ++ni)
		{
			strus::WordVector vec = dataset.vector( ti, ni);
			if (!vec.empty())
			{
				strus::SimHash lsh = model.simHash( vec, ni);
				transaction->writeSimHash( ti, ni, lsh);
				transaction->writeVector( ti, ni, vec);
			}
		}
	}
	if (!transaction->commit()) throw strus::runtime_error( "%s", _TXT("vector storage transaction failed"));
}

static bool compare( const strus::WordVector& v1, const strus::WordVector& v2)
{
	strus::WordVector::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	strus::WordVector::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (unsigned int vidx=0; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2,++vidx)
	{
		float diff = (*vi1 > *vi2)?(*vi1 - *vi2):(*vi2 - *vi1);
		if (diff > VEC_EPSILON)
		{
			for (unsigned int vv=0; vv != vidx; ++vv)
			{
				std::cerr << strus::string_format("%u: %f == %f", vv, v1[vv], v2[vv]) << std::endl;
			}
			std::cerr << strus::string_format("diff of %u: %f and %f is bigger than %f", vidx, *vi1, *vi2, VEC_EPSILON) << std::endl;
			return false;
		}
	}
	if (vi1 == ve1 && vi2 == ve2)
	{
		return true;
	}
	else
	{
		std::cerr << strus::string_format( "vectors have different length %u != %u", (unsigned int)v1.size(), (unsigned int)v2.size()) << std::endl;
		return false;
	}
}

static bool compare( const std::vector<strus::SimHash>& v1, const std::vector<strus::SimHash>& v2)
{
	std::vector<strus::SimHash>::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	std::vector<strus::SimHash>::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2)
	{
		if (*vi1 != *vi2 || vi1->id() != vi2->id()) return false;
	}
	return vi1 == ve1 && vi2 == ve2;
}

static bool compare( const std::vector<strus::Index>& v1, const std::vector<strus::Index>& v2)
{
	std::vector<strus::Index>::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	std::vector<strus::Index>::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2)
	{
		if (*vi1 != *vi2) return false;
	}
	return vi1 == ve1 && vi2 == ve2;
}

static bool compare( const strus::LshModel& m1, const strus::LshModel& m2)
{
	return m1.isequal( m2);
}

static void readAndCheckDatabase( const std::string& testdir, const std::string& configstr, const TestDataset& dataset, const strus::LshModel& model)
{
	strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_fileLocator, g_errorhnd));
	strus::DatabaseAdapter database( dbi.get(), configstr, g_errorhnd);

	std::size_t nofVariables = 0;
	database.checkVersion();
	{
		std::cerr << "checking read/write of variables ..." << std::endl;
		const VariableDef* vi = g_variables;
		for (; vi->type; ++vi)
		{
			std::string value = database.readVariable( vi->type);
			if (value != vi->value) throw std::runtime_error("a variable value does not match");
			++nofVariables;
		}
	}{
		std::vector<std::pair<std::string,std::string> > variables = database.readVariables();
		std::vector<std::pair<std::string,std::string> >::const_iterator di = variables.begin(), de = variables.end();
		for (; di != de; ++di)
		{
			if (0==std::strcmp( di->first.c_str(), "version")) continue;
			if (0==std::strcmp( di->first.c_str(), "config")) continue;

			const VariableDef* vi = g_variables;
			for (; vi->type; ++vi)
			{
				if (0==std::strcmp( di->first.c_str(), vi->type)) break;
			}
			if (!vi->type) throw strus::runtime_error("a variable value of '%s' is undefined", di->first.c_str());
			if (di->second != vi->value) throw std::runtime_error("a variable value does not match");
		}
	}{
		std::cerr << "checking read/write of feature types ..." << std::endl;
		int ti = 1, te = dataset.nofTypes();
		for (; ti <= te; ++ti)
		{
			std::string typestr = getTypeName( ti);
			if (ti != database.readTypeno( typestr) || typestr != database.readTypeName( ti))
			{
				throw std::runtime_error("stored type definitions do not match");
			}
		}
		if (database.readNofTypeno() != dataset.nofTypes())
		{
			throw std::runtime_error("stored number of types does not match");
		}
	}{
		std::cerr << "checking read/write of feature names ..." << std::endl;
		int ni = 1, ne = dataset.nofFeatures();
		for (; ni <= ne; ++ni)
		{
			std::string featstr = getFeatureName( ni);
			if (ni != database.readFeatno( featstr) || featstr != database.readFeatName( ni))
			{
				throw std::runtime_error("stored feature definitions do not match");
			}
		}
		if (database.readNofFeatno() != dataset.nofFeatures())
		{
			throw std::runtime_error("stored number of features does not match");
		}
	}{
		std::cerr << "checking read/write of feature type relations ..." << std::endl;
		std::map<strus::Index,std::vector<strus::Index> > featureTypeRelations;
		{
			std::map<TestDataset::FeatDef,int>::const_iterator mi = dataset.featmap().begin(), me = dataset.featmap().end();
			for (; mi != me; ++mi)
			{
				std::map<strus::Index,std::vector<strus::Index> >::iterator ri = featureTypeRelations.find( mi->first.featno);
				if (ri == featureTypeRelations.end())
				{
					std::vector<strus::Index> elems;
					elems.push_back( mi->first.typeno);
					featureTypeRelations.insert( std::pair<strus::Index,std::vector<strus::Index> >( mi->first.featno, elems) );
				}
				else
				{
					ri->second.push_back( mi->first.typeno);
				}
			}
		}
		int nofRelations = 0;
		strus::Index ti = 1, te = dataset.nofTypes();
		for (ti=1; ti <= te; ++ti)
		{
			strus::Index ni = 1, ne = dataset.nofFeatures();
			for (; ni <= ne; ++ni)
			{
				std::vector<strus::Index> elems1;
				std::map<strus::Index,std::vector<strus::Index> >::iterator ri = featureTypeRelations.find( ni);
				if (ri != featureTypeRelations.end())
				{
					elems1 = ri->second;
				}
				nofRelations += elems1.size();
				std::vector<strus::Index> elems2 = database.readFeatureTypeRelations( ni);
				if (!compare( elems1, elems2))
				{
					std::cerr << "expected feature type relations for " << ni << ": ";
					printArray( std::cerr, elems1);
					std::cerr << std::endl;
					std::cerr << "got feature type relations for " << ni << ": ";
					printArray( std::cerr, elems2);
					std::cerr << std::endl;
					throw std::runtime_error("stored feature relations do not match");
				}
			}
		}
		std::cerr << "number of features " << dataset.nofFeatures() << ", number of types " << dataset.nofTypes() << ", number of relations " << nofRelations << std::endl;
	}{
		strus::LshModel stored_model = database.readLshModel();
		if (!compare( model, stored_model))
		{
			throw std::runtime_error("stored LSH model does not match");
		}
	}{
		std::cerr << "checking read/write of vectors ..." << std::endl;
		strus::Index ti = 1, te = dataset.nofTypes();
		for (ti=1; ti <= te; ++ti)
		{
			if (dataset.nofVectors( ti) != database.readNofVectors( ti))
			{
				std::cerr << "expected number of vectors for " << ti << " are " << dataset.nofVectors( ti) << ", got " << database.readNofVectors( ti) << std::endl;
				throw std::runtime_error("number of vectors does not match");
			}
			strus::Index ni = 1, ne = dataset.nofFeatures();
			int nofVectors = 0;
			for (; ni <= ne; ++ni)
			{
				strus::WordVector vec1 = dataset.vector( ti, ni);
				strus::WordVector vec2 = database.readVector( ti, ni);
				if (!compare( vec1, vec2))
				{
					std::cerr << "expected vector for " << ti << "/" << ni << ":" << std::endl;
					printArray( std::cerr, vec1);
					std::cerr << "read vector for " << ti << "/" << ni << ":" << std::endl;
					printArray( std::cerr, vec2);
					throw std::runtime_error("stored vectors do not match");
				}
				if (!vec1.empty())
				{
					nofVectors += 1;
					strus::SimHash lsh = database.readSimHash( ti, ni);
					if (lsh != model.simHash( vec1, ni))
					{
						std::cerr << "expected LSH for " << ti << "/" << ni << ": " << model.simHash( vec1, ni).tostring() << std::endl;
						std::cerr << "read LSH for " << ti << "/" << ni << ": " << lsh.tostring() << std::endl;
						throw std::runtime_error("stored LSH values do not match");
					}
				}
			}
			std::cerr << "number of features " << ne << ", number of vectors " << nofVectors << std::endl;
		}
	}{
		std::cerr << "checking read of LSH blob ..." << std::endl;
		strus::Index ti = 1, te = dataset.nofTypes();
		for (ti=1; ti <= te; ++ti)
		{
			std::vector<strus::SimHash> ar1 = database.readSimHashVector( ti);
			std::vector<strus::SimHash> ar2;
			strus::Index ni = 1, ne = dataset.nofFeatures();
			for (; ni <= ne; ++ni)
			{
				strus::WordVector vec = database.readVector( ti, ni);
				if (!vec.empty())
				{
					ar2.push_back( model.simHash( vec, ni));
				}
			}
			std::cerr << "number of elements: " << ar1.size() << std::endl;
			if (!compare( ar1, ar2))
			{
				throw std::runtime_error("stored LSH value arrays do not match");
			}
		}
	}
}


#define DEFAULT_CONFIG \
	"path=vsmodel;"\
	"vecdim=121;"\
	"bits=7;"\
	"variations=13;"

int main( int argc, const char** argv)
{
	int rt = 0;
	g_errorhnd = strus::createErrorBuffer_standard( 0, 0, NULL/*debug trace interface*/);
	if (!g_errorhnd) {std::cerr << "FAILED " << "strus::createErrorBuffer_standard" << std::endl; return -1;}
	g_fileLocator = strus::createFileLocator_std( g_errorhnd);
	if (!g_fileLocator) {std::cerr << "FAILED " << "strus::createFileLocator_std" << std::endl; return -1;}

	try
	{
		initRandomNumberGenerator();
		std::string configstr( DEFAULT_CONFIG);
		strus::Index nofTypes = 2;
		strus::Index nofFeatures = 1000;
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
			else if (0==std::strcmp( argv[argidx], "-s"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -s needs argument (configuration string)" << std::endl;
					printUsageAndExit = true;
				}
				configstr = argv[++argidx];
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
			nofFeatures = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
		}
		if (argc > argidx)
		{
			nofTypes = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
		}
		if (argc > argidx)
		{
			std::cerr << "too many arguments (maximum 3 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}
		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<options>] [<workdir>] [<number of features>] [<number of types>]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h           : print this usage" << std::endl;
			std::cerr << "-s <CONFIG>            :specify test configuration string as <CONFIG>" << std::endl;
			std::cerr << "<workdir>              :working directory, default './'" << std::endl;
			std::cerr << "<number of features>   :number of features, default 1000" << std::endl;
			std::cerr << "<number of types>      :number of types, default 1" << std::endl;
			return rt;
		}
		if (g_errorhnd->hasError()) throw std::runtime_error("error in test configuration");

		TestDataset dataset( nofTypes, nofFeatures, configstr);
		strus::LshModel model( dataset.config().vecdim, dataset.config().bits, dataset.config().variations);

		static const char* vectorconfigkeys[] = {"memtypes","lexprun","vecdim","bits","variations",0};
		std::string dbconfigstr( configstr);
		removeKeysFromConfigString( dbconfigstr, vectorconfigkeys, g_errorhnd);

		writeDatabase( workdir, dbconfigstr, dataset, model);
		readAndCheckDatabase( workdir, dbconfigstr, dataset, model);

		strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_fileLocator, g_errorhnd));
		if (dbi.get())
		{
			(void)dbi->destroyDatabase( dbconfigstr);
		}
		if (g_errorhnd->hasError())
		{
			throw std::runtime_error( "uncaught exception");
		}
		std::cerr << "done" << std::endl;
		delete g_fileLocator;
		delete g_errorhnd;
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
		delete g_fileLocator;
		delete g_errorhnd;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		delete g_fileLocator;
		delete g_errorhnd;
		return -2;
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
		return -3;
	}
}

