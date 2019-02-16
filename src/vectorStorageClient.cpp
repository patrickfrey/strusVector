/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage client interface
#include "vectorStorageClient.hpp"
#include "vectorStorageTransaction.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/valueIteratorInterface.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/string_conv.hpp"
#include "simHashReader.hpp"
#include "simHashRankList.hpp"
#include "sentenceLexerInstance.hpp"
#include "armautils.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"
#include <cstring>
#include <algorithm>
#include <limits>

#define MODULENAME   "vector storage"
#define STRUS_DBGTRACE_COMPONENT_NAME "vector"

using namespace strus;

VectorStorageClient::VectorStorageClient( const DatabaseInterface* database_, const std::string& configstring_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_database(),m_model(),m_simHashMapMap(),m_inMemoryTypes(),m_transaction_mutex()
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);

	std::string configstring = configstring_;
	unsigned int value;
	std::string stringvalue;
	int simdist = -1;
	int probsimdist = -1;
	if (strus::extractUIntFromConfigString( value, configstring, "simdist", m_errorhnd))
	{
		if (m_debugtrace) m_debugtrace->event( "param", "simdist %d", value);
		simdist = value;
	}
	if (strus::extractUIntFromConfigString( value, configstring, "probsimdist", m_errorhnd))
	{
		if (m_debugtrace) m_debugtrace->event( "param", "probsimdist %d", value);
		probsimdist = value;
	}
	if (strus::extractStringArrayFromConfigString( m_inMemoryTypes, configstring, "memtypes", ',', m_errorhnd))
	{
		if (m_debugtrace)
		{
			std::vector<std::string>::const_iterator mi = m_inMemoryTypes.begin(), me = m_inMemoryTypes.end();
			for (; mi != me; ++mi) m_debugtrace->event( "param", "in memory lsh for feature %s", mi->c_str());
		}
	}
	m_database.reset( new DatabaseAdapter( database_,configstring,m_errorhnd));
	m_database->checkVersion();
	m_model = m_database->readLshModel();
	if (simdist >= 0) m_model.set_simdist( simdist);
	if (probsimdist >= 0) m_model.set_probsimdist( probsimdist);
}

VectorStorageClient::~VectorStorageClient()
{
	if (m_debugtrace) delete m_debugtrace;
}

void VectorStorageClient::prepareSearch( const std::string& type)
{
	try
	{
		(void)getOrCreateTypeSimHashMap( type);
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in client interface of '%s' preparing data structures for the vector search: %s"), MODULENAME, *m_errorhnd);
}

std::vector<VectorQueryResult> VectorStorageClient::simHashToVectorQueryResults( const std::vector<SimHashQueryResult>& res, int maxNofResults, double minSimilarity) const
{
	std::vector<VectorQueryResult> rt;
	std::vector<SimHashQueryResult>::const_iterator ri = res.begin(), re = res.end();
	for (int ridx=0; ri != re && ridx < maxNofResults && ri->weight() > minSimilarity; ++ri,++ridx)
	{
		rt.push_back( VectorQueryResult( m_database->readFeatName( ri->featno()), ri->weight()));
	}
	return rt;
}

std::vector<VectorQueryResult> VectorStorageClient::findSimilar( const std::string& type, const WordVector& vec, int maxNofResults, double minSimilarity, bool realVecWeights) const
{
	try
	{
		std::vector<SimHashQueryResult> res;
		strus::Reference<SimHashMap> simHashMap = getOrCreateTypeSimHashMap( type);
		SimHashMap::Stats stats;

		int simdist = SimHashRankList::lshSimDistFromWeight( m_model.vectorBits(), minSimilarity);
		if (simdist > m_model.vectorBits()) simdist = m_model.vectorBits();
		int probsimdist = ((double)m_model.probsimdist() / (double)m_model.simdist()) * simdist;
		if (probsimdist > m_model.vectorBits()) probsimdist = m_model.vectorBits();

		if (realVecWeights)
		{
			if (minSimilarity < 0.0 || minSimilarity > 1.0)
			{
				throw std::runtime_error( _TXT( "min similarity parameter out of range"));
			}
			int maxNofSimResults = maxNofResults * 2 + 10;
			if (maxNofSimResults > SimHashRankList::MaxSize)
			{
				if (maxNofResults > SimHashRankList::MaxSize)
				{
					throw strus::runtime_error( "%s",  _TXT( "maximum number of ranks is out of range"));
				}
				maxNofSimResults = SimHashRankList::MaxSize;
			}
			res.reserve( maxNofSimResults);

			arma::fvec vv = arma::fvec( vec);
			SimHash needle( m_model.simHash( strus::normalizeVector( vec), 0));
			if (m_debugtrace)
			{
				res = simHashMap->findSimilarWithStats( stats, needle, simdist, probsimdist, maxNofSimResults);
			}
			else
			{
				res = simHashMap->findSimilar( needle, simdist, probsimdist, maxNofSimResults);
			}
			std::vector<SimHashQueryResult>::iterator ri = res.begin(), re = res.end();
			for (; ri != re; ++ri)
			{
				arma::fvec resvv( m_database->readVector( simHashMap->typeno(), ri->featno()));
				ri->setWeight( arma::norm_dot( vv, resvv));
			}
			std::sort( res.begin(), res.end(), std::greater<SimHashQueryResult>());
		}
		else
		{
			SimHash needle( m_model.simHash( strus::normalizeVector( vec), 0));
			if (m_debugtrace)
			{
				res = simHashMap->findSimilarWithStats( stats, needle, simdist, probsimdist, maxNofResults);
			}
			else
			{
				res = simHashMap->findSimilar( needle, simdist, probsimdist, maxNofResults);
			}
		}
		if (m_debugtrace)
		{
			std::string vecstr = vec.tostring(", ", 10);
			m_debugtrace->event( "findsim", "%s {%s,...}", type.c_str(), vecstr.c_str());
			m_debugtrace->open( "search");
			m_debugtrace->event( "param", _TXT("LSH simdist %d"), simdist);
			m_debugtrace->event( "param", _TXT("LSH prob simdist %d"), probsimdist);
			m_debugtrace->event( "param", _TXT("feature type %s"), type.c_str());
			m_debugtrace->event( "param", _TXT("max results %d"), maxNofResults);
			m_debugtrace->event( "param", _TXT("min similarity %.5f"), minSimilarity);
			m_debugtrace->event( "param", _TXT("use real weights %s"), realVecWeights ? "yes":"no");

			m_debugtrace->event( "stats", _TXT("benches %d"), stats.nofBenches);
			m_debugtrace->event( "stats", _TXT("values %d"), stats.nofValues);
			m_debugtrace->event( "stats", _TXT("database reads %d"), stats.nofDatabaseReads);
			m_debugtrace->event( "stats", _TXT("partial prob sum %d"), stats.probSum);
			m_debugtrace->event( "stats", _TXT("best filter samples max dist %d"), stats.samplesMaxDist);
			m_debugtrace->event( "stats", _TXT("results %d"), stats.nofResults);
			for (int bi=0; bi<stats.nofBenches; ++bi)
			{
				m_debugtrace->event( "stats", "candidates[%d] %d", bi, stats.nofCandidates[bi]);
			}
			std::vector<SimHashQueryResult>::const_iterator ri = res.begin(), re = res.end();
			for (; ri != re; ++ri)
			{
				m_debugtrace->event( "result", _TXT("featno %d, simdist %d, weight %.5f"), ri->featno(), ri->simdist(), ri->weight());
			}
			m_debugtrace->close();
		}
		std::vector<VectorQueryResult> rt = simHashToVectorQueryResults( res, maxNofResults, minSimilarity);

		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("vector search failed: %s"), m_errorhnd->fetchError());
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' in find similar: %s"), MODULENAME, *m_errorhnd, std::vector<VectorQueryResult>());
}

VectorStorageTransactionInterface* VectorStorageClient::createTransaction()
{
	try
	{
		return new VectorStorageTransaction( this, m_database, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' creating transaction: %s"), MODULENAME, *m_errorhnd, 0);
}

std::vector<std::string> VectorStorageClient::types() const
{
	try
	{
		return m_database->readTypes();
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the types defined: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
}

class FeatureValueIterator
	:public ValueIteratorInterface
{
public:
	FeatureValueIterator( const DatabaseClientInterface* database_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_dbcursor( database_),m_value(),m_keyprefix(),m_hasValue(false),m_hasInit(false){}
	~FeatureValueIterator(){}

	virtual void skip( const char* value, std::size_t size)
	{
		try
		{
			m_keyprefix.clear();
			m_hasValue = m_dbcursor.skip( std::string( value, size), m_value);
			m_hasInit = true;
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error in client interface of '%s' iterating on features, skipping to key: %s"), MODULENAME, *m_errorhnd);
	}

	virtual void skipPrefix( const char* value, std::size_t size)
	{
		try
		{
			m_keyprefix = std::string( value, size);
			m_hasValue = m_dbcursor.skipPrefix( m_keyprefix, m_value);
			m_hasInit = true;
		}
		CATCH_ERROR_ARG1_MAP( _TXT("error in client interface of '%s' iterating on features, skipping to key with prefix: %s"), MODULENAME, *m_errorhnd);
	}

	virtual std::vector<std::string> fetchValues( std::size_t maxNofElements)
	{
		try
		{
			std::vector<std::string> rt;
			std::string key;
			if (!m_hasInit)
			{
				if (m_dbcursor.loadFirst( key))
				{
					rt.push_back( key);
				}
				m_hasInit = true;
			}
			else if (m_hasValue)
			{
				rt.push_back( m_value);
				m_value.clear();
				m_hasValue = false;
			}
			if (m_keyprefix.empty())
			{
				while (rt.size() < maxNofElements && m_dbcursor.loadNext( key))
				{
					rt.push_back( key);
				}
			}
			else
			{
				while (rt.size() < maxNofElements && m_dbcursor.loadNextPrefix( m_keyprefix, key))
				{
					rt.push_back( key);
				}
			}
			return rt;
		}
		CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' iterating on features, fetching values: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
	}

private:
	ErrorBufferInterface* m_errorhnd;
	DatabaseAdapter::FeatureCursor m_dbcursor;
	std::string m_value;
	std::string m_keyprefix;
	bool m_hasValue;
	bool m_hasInit;
};

ValueIteratorInterface* VectorStorageClient::createFeatureValueIterator() const
{
	try
	{
		return new FeatureValueIterator( m_database->database(), m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' creating a feature value iterator: %s"), MODULENAME, *m_errorhnd, NULL);
}

std::vector<std::string> VectorStorageClient::featureTypes( const std::string& featureValue) const
{
	try
	{
		std::vector<std::string> rt;
		Index featno = m_database->readFeatno( featureValue);
		if (featno)
		{
			std::vector<Index> typenos = m_database->readFeatureTypeRelations( featno);
			std::vector<Index>::const_iterator ti = typenos.begin(), te = typenos.end();
			for (; ti != te; ++ti)
			{
				rt.push_back( m_database->readTypeName( *ti));
			}
		}
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT("inconsistency in database: %s"), m_errorhnd->fetchError());
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the types defined for a feature value: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
}

int VectorStorageClient::nofVectors( const std::string& type) const
{
	try
	{
		int rt;
		Index typeno = m_database->readTypeno( type);
		if (typeno)
		{
			rt = m_database->readNofVectors( typeno);
		}
		else
		{
			rt = 0;
		}
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT("inconsistency in database: %s"), m_errorhnd->fetchError());
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the number of features defined for a type: %s"), MODULENAME, *m_errorhnd, 0);
}

WordVector VectorStorageClient::featureVector( const std::string& type, const std::string& featureValue) const
{
	try
	{
		Index typeno = m_database->readTypeno( type);
		Index featno = m_database->readFeatno( featureValue);
		if (!typeno || !featno) return WordVector();
		WordVector rt = m_database->readVector( typeno, featno);
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT("inconsistency in database: %s"), m_errorhnd->fetchError());
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the vector assigned to a feature of a specific type: %s"), MODULENAME, *m_errorhnd, WordVector());
}

double VectorStorageClient::vectorSimilarity( const WordVector& v1, const WordVector& v2) const
{
	try
	{
		if (v1.size() != v2.size()) throw std::runtime_error(_TXT("invalid arguments (vector sizes do not match"));
		arma::fvec vv1 = arma::fvec( v1);
		arma::fvec vv2 = arma::fvec( v2);
		return arma::norm_dot( vv1, vv2);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' calculating the similarity of two vectors: %s"), MODULENAME, *m_errorhnd, std::numeric_limits<double>::quiet_NaN());
}

WordVector VectorStorageClient::normalize( const WordVector& vec) const
{
	try
	{
		WordVector rt;
		rt.reserve( vec.size());
		arma::fvec vv = strus::normalizeVector( vec);
		arma::fvec::const_iterator vi = vv.begin(), ve = vv.end();
		for (; vi != ve; ++vi)
		{
			rt.push_back( *vi);
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' normalizing a vector: %s"), MODULENAME, *m_errorhnd, WordVector());
}

SentenceLexerInstanceInterface* VectorStorageClient::createSentenceLexer() const
{
	try
	{
		return new SentenceLexerInstance( this, m_database->database(), m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the configuration string this storage was built with: %s"), MODULENAME, *m_errorhnd, NULL);
}

std::string VectorStorageClient::config() const
{
	try
	{
		return m_database->readVariable( "config");
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' getting the configuration string this storage was built with: %s"), MODULENAME, *m_errorhnd, std::string());
}

void VectorStorageClient::close()
{
	try
	{
		m_database->close();
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in client interface of '%s' closing this storage client: %s"), MODULENAME, *m_errorhnd);
}

void VectorStorageClient::resetSimHashMapTypes( const std::vector<std::string>& types_)
{
	if (!m_simHashMapMap.get()) return;

	SimHashMapMapRef simHashMapMapRef = m_simHashMapMap;
	std::vector<std::string>::const_iterator ti = types_.begin(), te = types_.end();
	for (; ti != te; ++ti)
	{
		SimHashMapMap::const_iterator mi = simHashMapMapRef->find( *ti);
		if (mi != simHashMapMapRef->end()) break;
	}
	if (ti != te)
	{
		SimHashMapMapRef simHashMapMapCopy( new SimHashMapMap( *simHashMapMapRef));
		for (ti = types_.begin(); ti != te; ++ti)
		{
			simHashMapMapCopy->erase( *ti);
		}
		m_simHashMapMap = simHashMapMapCopy;
	}
}

strus::Reference<SimHashMap> VectorStorageClient::getSimHashMap( const std::string& type) const
{
	if (!m_simHashMapMap.get()) return strus::Reference<SimHashMap>();

	SimHashMapMapRef simHashMapMapRef = m_simHashMapMap;
	SimHashMapMap::const_iterator mi = simHashMapMapRef->find( type);

	if (mi == simHashMapMapRef->end())
	{
		return strus::Reference<SimHashMap>();
	}
	else
	{
		return mi->second;
	}
}

strus::Reference<SimHashMap> VectorStorageClient::getOrCreateTypeSimHashMap( const std::string& type) const
{
	strus::Reference<SimHashMap> rt = getSimHashMap( type);
	if (rt.get()) return rt;
	strus::Index typeno = m_database->readTypeno( type);
	if (!typeno) throw strus::runtime_error(_TXT("queried type is not defined: %s"), type.c_str());

	strus::Reference<SimHashReaderInterface> reader;
	const char* readerClass = "database";
	if (std::find( m_inMemoryTypes.begin(), m_inMemoryTypes.end(), type) != m_inMemoryTypes.end())
	{
		readerClass = "in memory";
		reader.reset( new SimHashReaderDatabase( m_database.get(), type));
	}
	else
	{
		reader.reset( new SimHashReaderMemory( m_database.get(), type));
	}
	strus::Reference<SimHashMap> simHashMapRef( new SimHashMap( reader, typeno));
	simHashMapRef->load();

	if (!m_simHashMapMap.get())
	{
		SimHashMapMapRef simHashMapMapCopy( new SimHashMapMap());
		simHashMapMapCopy->insert( SimHashMapMap::value_type( type, simHashMapRef));
		m_simHashMapMap = simHashMapMapCopy;
	}
	else
	{
		SimHashMapMapRef simHashMapMapRef = m_simHashMapMap;
		SimHashMapMapRef simHashMapMapCopy( new SimHashMapMap( *simHashMapMapRef));
		simHashMapMapCopy->insert( SimHashMapMap::value_type( type, simHashMapRef));
		m_simHashMapMap = simHashMapMapCopy;
	}
	if (m_debugtrace) m_debugtrace->event( "simhash", _TXT("created searcher (%s) for vectors of the feature type %s"), readerClass, type.c_str());
	return simHashMapRef;
}



