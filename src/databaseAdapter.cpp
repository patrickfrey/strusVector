/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "databaseAdapter.hpp"
#include "strus/base/hton.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/stdint.h"
#include "strus/versionVector.hpp"
#include "strus/databaseOptions.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include <cstring>

using namespace strus;

using namespace strus;

DatabaseAdapter::DatabaseAdapter( DatabaseInterface* database, const std::string& config, ErrorBufferInterface* errorhnd_)
	:m_database(database->createClient(config)),m_errorhnd(errorhnd_)
{
	if (!m_database.get()) throw strus::runtime_error( _TXT("failed to create database client for standard vector space model: %s"), m_errorhnd->fetchError());
}

void DatabaseAdapter::commit()
{
	if (m_transaction.get())
	{
		if (!m_transaction->commit())
		{
			m_transaction.reset();
			throw strus::runtime_error( _TXT("standard vector space model transaction commit failed: %s"), m_errorhnd->fetchError());
		}
		m_transaction.reset();
	}
}

void DatabaseAdapter::beginTransaction()
{
	if (m_transaction.get())
	{
		m_transaction->rollback();
	}
	m_transaction.reset( m_database->createTransaction());
	if (!m_transaction.get())
	{
		throw strus::runtime_error( _TXT("standard vector space model create transaction failed: %s"), m_errorhnd->fetchError());
	}
}


struct VectorSpaceModelHdr
{
	enum {FILEID=0x3ff3};
	char name[58];
	unsigned short _id;
	unsigned short version_major;
	unsigned short version_minor;

	VectorSpaceModelHdr()
	{
		std::memcpy( name, "strus standard vector space model bin file\n\0", sizeof(name));
		_id = FILEID;
		version_major = STRUS_VECTOR_VERSION_MAJOR;
		version_minor = STRUS_VECTOR_VERSION_MINOR;
	}

	void check()
	{
		if (_id != FILEID) throw strus::runtime_error(_TXT("unknown file type, expected strus standard vector space model binary file"));
		if (version_major != STRUS_VECTOR_VERSION_MAJOR) throw strus::runtime_error(_TXT("major version (%u) of loaded strus standard vector space model binary file does not match"), version_major);
		if (version_minor > STRUS_VECTOR_VERSION_MINOR) throw strus::runtime_error(_TXT("minor version (%u) of loaded strus standard vector space model binary file does not match (newer than your version of strus)"), version_minor);
	}

	void hton()
	{
		_id = ByteOrder<unsigned short>::hton( _id);
		version_major = ByteOrder<unsigned short>::hton( version_major);
		version_minor = ByteOrder<unsigned short>::hton( version_minor);
	}

	void ntoh()
	{
		_id = ByteOrder<unsigned short>::ntoh( _id);
		version_major = ByteOrder<unsigned short>::ntoh( version_major);
		version_minor = ByteOrder<unsigned short>::ntoh( version_minor);
	}
};


enum DatabaseKeyPrefix
{
	KeyVersion = 'V',
	KeySampleVectors = '$',
	KeySampleSimHashVector = 'S',
	KeyResultSimHashVector = 'R',
	KeyConfig = 'C',
	KeyLshModel = 'L',
	KeySimRelationMap = 'M',
	KeySampleFeatureIndexMap = 'f',
	KeyFeatureSampleIndexMap = 's'
};

struct DatabaseKey
{
public:
	explicit DatabaseKey( const DatabaseKeyPrefix& prefix)
		:m_itr(1)
	{
		m_buf[0] = (char)prefix;
		m_buf[1] = '\0';
	}
	DatabaseKey()
		:m_itr(0)
	{
		m_buf[0] = '\0';
	}
	template <typename ScalarType>
	DatabaseKey& operator[]( const ScalarType& val)
	{
		if (8 + m_itr > MaxKeySize) throw strus::runtime_error(_TXT("array bound write in database key buffer"));
		m_itr += utf8encode( m_buf + m_itr, val);
		m_buf[ m_itr] = 0;
		return *this;
	}

	const char* c_str() const	{return m_buf;}
	std::size_t size() const	{return m_itr;}

private:
	enum {MaxKeySize=256};
	char m_buf[ MaxKeySize];
	std::size_t m_itr;
};

struct DatabaseValue
{
public:
	DatabaseValue()
		:m_itr(0)
	{
		m_buf[0] = '\0';
	}
	template <typename ScalarType>
	DatabaseValue& operator[]( const ScalarType& val)
	{
		if (sizeof(ScalarType) + m_itr > MaxValueSize) throw strus::runtime_error(_TXT("array bound write in database value buffer"));
		ScalarType hostval = ByteOrder<ScalarType>( val);
		std::memcpy( m_buf + m_itr, &hostval, sizeof(hostval));
		m_itr += sizeof(hostval);
		return *this;
	}

	const char* c_str() const	{return m_buf;}
	std::size_t size() const	{return m_itr;}

private:
	enum {MaxValueSize=1024};
	char m_buf[ 256];
	std::size_t m_itr;
};


void DatabaseAdapter::checkVersion()
{
	DatabaseKey key( KeyVersion);

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read version from database: %s"), m_errorhnd->fetchError());
	}
	VectorSpaceModelHdr hdr;
	if (content.size() < sizeof(hdr)) throw strus::runtime_error(_TXT("unknown version format"));
	std::memcpy( &hdr, content.c_str(), sizeof(hdr));
	hdr.ntoh();
	hdr.check();
}

void DatabaseAdapter::writeVersion()
{
	DatabaseKey key( KeyVersion);

	VectorSpaceModelHdr hdr;
	hdr.hton();
	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), (const char*)&hdr, sizeof(hdr));
}

enum {SimHashBlockSize=128};

static void verifyAscIndex( const DatabaseKey& key, unsigned int expected_idx, const char* recname)
{
	if (expected_idx != (unsigned int)utf8decode( key.c_str()+1, key.size()-1))
	{
		throw strus::runtime_error( _TXT("keys not ascending in '%s' structure"), recname);
	}
}

static DatabaseAdapter::Vector createVectorFromSerialization( const std::string& blob)
{
	DatabaseAdapter::Vector rt;
	rt.reserve( blob.size() / sizeof(double));
	double const* ri = (const double*)blob.c_str();
	const double* re = ri + blob.size() / sizeof(double);
	for (; ri != re; ++ri)
	{
		rt.push_back( ByteOrder<double>::ntoh( *ri));
	}
	return rt;
}

static std::string vectorSerialization( const DatabaseAdapter::Vector& vec)
{
	std::string rt;
	rt.reserve( vec.size() * sizeof(double));
	DatabaseAdapter::Vector::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		rt.push_back( ByteOrder<double>::hton( *vi));
	}
	return rt;
}

std::vector<DatabaseAdapter::Vector> DatabaseAdapter::readSampleVectors() const
{
	std::vector<Vector> rt;
	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read sample vectors: %s"), m_errorhnd->fetchError());

	DatabaseKey key( KeySampleVectors);
	unsigned int idx = 0;
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		verifyAscIndex( key, ++idx, "samplevec");

		Vector vec = createVectorFromSerialization( cursor->value());
		rt.push_back( vec);
		slice = cursor->seekNext();
	}
	return rt;
}

void DatabaseAdapter::writeSampleVectors( const std::vector<DatabaseAdapter::Vector>& ar)
{
	if (!m_transaction.get()) beginTransaction();
	unsigned int aidx = 0;
	std::vector<Vector>::const_iterator ai = ar.begin(), ae = ar.end();
	for (; ai != ae; ++ai)
	{
		DatabaseKey key( KeySampleVectors);
		key[ ++aidx];

		std::string blob( vectorSerialization( *ai));
		m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
		if (aidx % 10000 == 0) commit();
	}
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing sample vectors failed: %s"), m_errorhnd->fetchError());
	}
}

DatabaseAdapter::Vector DatabaseAdapter::readSampleVector( const SampleIndex& sidx) const
{
	DatabaseKey key( KeySampleVectors);
	key[ sidx+1];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		throw strus::runtime_error(_TXT("failed to create cursor to read sample vector: %s"), m_errorhnd->fetchError());
	}
	return createVectorFromSerialization( blob);
}

void DatabaseAdapter::writeSampleVector( const SampleIndex& sidx, const Vector& vec)
{
	DatabaseKey key( KeySampleVectors);
	key[ sidx+1];

	std::string blob( vectorSerialization( vec));
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

std::vector<SimHash> DatabaseAdapter::readSimhashVector( char prefix) const
{
	std::vector<SimHash> rt;
	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read sim hash vector: %s"), m_errorhnd->fetchError());

	DatabaseKey key( (DatabaseKeyPrefix)prefix);
	unsigned int idx = 0;
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		verifyAscIndex( key, ++idx, "simhash");

		std::vector<SimHash> vec = SimHash::createFromSerialization( cursor->value());
		rt.insert( rt.end(), vec.begin(), vec.end());
		slice = cursor->seekNext();
		if (vec.size() != SimHashBlockSize && slice.defined())
		{
			throw strus::runtime_error( _TXT("blocks not complete in '%s' structure"), "simhash");
		}
	}
	return rt;
}

void DatabaseAdapter::writeSimhashVector( char prefix, const std::vector<SimHash>& ar)
{
	if (!m_transaction.get()) beginTransaction();
	unsigned int idx = 0;
	SimHash const* ai = ar.data();
	const SimHash* ae = ai + ar.size();
	for (; ai < ae; ai += SimHashBlockSize)
	{
		DatabaseKey key( (DatabaseKeyPrefix)prefix);
		key[ ++idx];

		std::size_t blobsize = (ai+SimHashBlockSize) >= ae ? (ae-ai):SimHashBlockSize;
		std::string blob( SimHash::serialization( ai, blobsize));
		m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
		if (idx % 10000 == 0) commit();
	}
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing simhash vectors failed: %s"), m_errorhnd->fetchError());
	}
}

std::vector<SimHash> DatabaseAdapter::readSampleSimhashVector() const
{
	return readSimhashVector( KeySampleSimHashVector);
}

std::vector<SimHash> DatabaseAdapter::readResultSimhashVector() const
{
	return readSimhashVector( KeyResultSimHashVector);
}

void DatabaseAdapter::writeSampleSimhashVector( const std::vector<SimHash>& ar)
{
	return writeSimhashVector( KeySampleSimHashVector, ar);
}

void DatabaseAdapter::writeResultSimhashVector( const std::vector<SimHash>& ar)
{
	return writeSimhashVector( KeyResultSimHashVector, ar);
}


VectorSpaceModelConfig DatabaseAdapter::readConfig() const
{
	DatabaseKey key( KeyConfig);

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read configuration from database: %s"), m_errorhnd->fetchError());
	}
	return VectorSpaceModelConfig( content, m_errorhnd);
}

void DatabaseAdapter::writeConfig( const VectorSpaceModelConfig& config)
{
	DatabaseKey key( KeyConfig);
	std::string content( config.tostring());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing configuration failed: %s"), m_errorhnd->fetchError());
	}
}

LshModel DatabaseAdapter::readLshModel() const
{
	DatabaseKey key( KeyLshModel);
	
	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read LSH model from database: %s"), m_errorhnd->fetchError());
	}
	return LshModel::fromSerialization( content);
}

void DatabaseAdapter::writeLshModel( const LshModel& model)
{
	DatabaseKey key( KeyLshModel);	
	std::string content( model.serialization());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing LSH model failed: %s"), m_errorhnd->fetchError());
	}
	commit();
}

SimRelationMap DatabaseAdapter::readSimRelationMap() const
{
	DatabaseKey key( KeySimRelationMap);
	
	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read sim relation map from database: %s"), m_errorhnd->fetchError());
	}
	return SimRelationMap::fromSerialization( content);
}

void DatabaseAdapter::writeSimRelationMap( const SimRelationMap& simrelmap)
{
	DatabaseKey key( KeySimRelationMap);	
	std::string content( simrelmap.serialization());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing sim relation map failed: %s"), m_errorhnd->fetchError());
	}
	commit();
}

SampleFeatureIndexMap DatabaseAdapter::readSampleFeatureIndexMap() const
{
	DatabaseKey key( KeySampleFeatureIndexMap);
	
	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read sample to feature index map from database: %s"), m_errorhnd->fetchError());
	}
	return SampleFeatureIndexMap::fromSerialization( content);
}

void DatabaseAdapter::writeSampleFeatureIndexMap( const SampleFeatureIndexMap& simrelmap)
{
	DatabaseKey key( KeySampleFeatureIndexMap);	
	std::string content( simrelmap.serialization());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing sample to feature index map failed: %s"), m_errorhnd->fetchError());
	}
	commit();
}

FeatureSampleIndexMap DatabaseAdapter::readFeatureSampleIndexMap() const
{
	DatabaseKey key( KeyFeatureSampleIndexMap);
	
	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read sample to feature index map from database: %s"), m_errorhnd->fetchError());
	}
	return FeatureSampleIndexMap::fromSerialization( content);
}

void DatabaseAdapter::writeFeatureSampleIndexMap( const FeatureSampleIndexMap& fsmap)
{
	DatabaseKey key( KeyFeatureSampleIndexMap);	
	std::string content( fsmap.serialization());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
	if (m_errorhnd->hasError())
	{
		throw strus::runtime_error(_TXT("storing sample to feature index map failed: %s"), m_errorhnd->fetchError());
	}
	commit();
}

/*
StringList strus::readSampleNamesFromFile( const std::string& filename)
{
	return StringList();
}

void strus::writeSampleNamesToFile( const StringList& , const std::string& filename)
{
}
*/
