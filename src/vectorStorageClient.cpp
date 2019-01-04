/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage client interface
#include "vectorStorageClient.hpp"
#include "vectorStorageSearch.hpp"
#include "vectorStorageTransaction.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/valueIteratorInterface.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"

#define MODULENAME   "vector storage"
#define MAIN_CONCEPT_CLASSNAME ""

using namespace strus;

VectorStorageClient::VectorStorageClient( const DatabaseInterface* database_, const std::string& configstring_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_database(),m_model(),m_transaction_mutex()
{
	m_database.reset( new DatabaseAdapter( database_,configstring_,m_errorhnd));
	m_database->checkVersion();
	m_model = m_database->readLshModel();
}

VectorStorageSearchInterface* VectorStorageClient::createSearcher( const std::string& type, int indexPart, int nofParts, bool realVecWeights) const
{
	try
	{
		return new VectorStorageSearch( m_database, m_model, type, indexPart, nofParts, realVecWeights, m_errorhnd);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' creating searcher: %s"), MODULENAME, *m_errorhnd, 0);
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
		arma::fvec vv1 = arma::fvec( v1);
		arma::fvec vv2 = arma::fvec( v2);
		return arma::norm_dot( vv1, vv2);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' calculating the similarity of two vectors: %s"), MODULENAME, *m_errorhnd, 0.0);
}

WordVector VectorStorageClient::normalize( const WordVector& vec) const
{
	try
	{
		WordVector rt;
		rt.reserve( vec.size());
		arma::fvec vv = arma::normalise( arma::fvec( vec));
		arma::fvec::const_iterator vi = vv.begin(), ve = vv.end();
		for (; vi != ve; ++vi)
		{
			rt.push_back( *vi);
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in client interface of '%s' normalizing a vector: %s"), MODULENAME, *m_errorhnd, WordVector());
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




