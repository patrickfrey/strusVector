/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "databaseAdapter.hpp"
#include "databaseHelpers.hpp"
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

#define VARIABLE_NOF_SAMPLES  "samples"
#define VARIABLE_NOF_FEATURES "feats"


DatabaseAdapter::DatabaseAdapter( const DatabaseInterface* database_, const std::string& config, ErrorBufferInterface* errorhnd_)
	:m_database(database_->createClient(config)),m_errorhnd(errorhnd_)
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

void DatabaseAdapter::checkVersion()
{
	DatabaseKeyBuffer key( KeyVersion);

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
	DatabaseKeyBuffer key( KeyVersion);

	VectorSpaceModelHdr hdr;
	hdr.hton();
	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), (const char*)&hdr, sizeof(hdr));
}

void DatabaseAdapter::writeVariable( const char* name, unsigned int value)
{
	std::string key;
	key.push_back( (char)KeyVariable);
	key.append( name);
	DatabaseValueBuffer valuebuf;
	valuebuf[ (uint64_t)value];

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), valuebuf.c_str(), valuebuf.size());
}

unsigned int DatabaseAdapter::readVariable( const char* name) const
{
	std::string key;
	key.push_back( (char)KeyVariable);
	key.append( name);

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to to read sample vector: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return 0;
		}
	}
	uint64_t rt;
	DatabaseValueScanner scanner( blob);
	scanner[ rt];
	if (!scanner.eof()) throw strus::runtime_error(_TXT("failed to read variable: %s"), "extra characters in stored value");

	return (unsigned int)rt;
}

void DatabaseAdapter::writeVariables( const SampleIndex& nofSamples, const FeatureIndex& nofFeatures)
{
	if (!m_transaction.get()) beginTransaction();
	writeVariable( VARIABLE_NOF_SAMPLES, nofSamples);
	writeVariable( VARIABLE_NOF_FEATURES, nofFeatures);
}

void DatabaseAdapter::writeSample( const SampleIndex& sidx, const std::string& name, const Vector& vec, const SimHash& simHash)
{
	if (!m_transaction.get()) beginTransaction();
	writeSampleIndex( sidx, name);
	writeSampleName( sidx, name);
	writeSampleVector( sidx, vec);
	writeSimhash( KeySampleSimHash, sidx, simHash);
}

enum {SimHashBlockSize=128};

template <typename ScalarType>
static std::vector<ScalarType> vectorFromSerialization( const std::string& blob)
{
	std::vector<ScalarType> rt;
	rt.reserve( blob.size() / sizeof(ScalarType));
	if (blob.size() % sizeof(ScalarType) != 0)
	{
		throw strus::runtime_error(_TXT("corrupt data in vector serialization"));
	}
	ScalarType const* ri = (const ScalarType*)blob.c_str();
	const ScalarType* re = ri + blob.size() / sizeof(ScalarType);
	for (; ri != re; ++ri)
	{
		rt.push_back( ByteOrder<ScalarType>::ntoh( *ri));
	}
	return rt;
}

template <typename ScalarType>
static std::string vectorSerialization( const std::vector<ScalarType>& vec)
{
	std::string rt;
	rt.reserve( vec.size() * sizeof(ScalarType));
	typename std::vector<ScalarType>::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		ScalarType buf = ByteOrder<ScalarType>::hton( *vi);
		rt.append( (const char*)&buf, sizeof(buf));
	}
	return rt;
}

DatabaseAdapter::Vector DatabaseAdapter::readSampleVector( const SampleIndex& sidx) const
{
	DatabaseKeyBuffer key( KeySampleVector);
	key[ sidx+1];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		throw strus::runtime_error(_TXT("failed to to read sample vector: %s"), m_errorhnd->fetchError());
	}
	return vectorFromSerialization<double>( blob);
}

void DatabaseAdapter::writeSampleVector( const SampleIndex& sidx, const Vector& vec)
{
	DatabaseKeyBuffer key( KeySampleVector);
	key[ sidx+1];

	std::string blob( vectorSerialization<double>( vec));
	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

std::string DatabaseAdapter::readSampleName( const SampleIndex& sidx) const
{
	DatabaseKeyBuffer key( KeySampleName);
	key[ sidx+1];

	std::string name;
	if (!m_database->readValue( key.c_str(), key.size(), name, DatabaseOptions().useCache()))
	{
		throw strus::runtime_error(_TXT("failed to read sample name: %s"), m_errorhnd->fetchError());
	}
	return name;
}

void DatabaseAdapter::writeSampleName( const SampleIndex& sidx, const std::string& name)
{
	DatabaseKeyBuffer key( KeySampleName);
	key[ sidx+1];

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), name.c_str(), name.size());
}

SampleIndex DatabaseAdapter::readSampleIndex( const std::string& name) const
{
	std::string key;
	key.push_back( (char)KeySampleName);
	key.append( name);

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read sample name: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return -1;
		}
	}
	uint32_t value = 0;
	DatabaseValueScanner scanner( blob);
	scanner[ value];
	if (!scanner.eof()) throw strus::runtime_error(_TXT("failed to read sample index: %s"), "extra characters in stored value");
	return value;
}

void DatabaseAdapter::writeSampleIndex( const SampleIndex& sidx, const std::string& name)
{
	std::string key;
	key.push_back( (char)KeySampleName);
	key.append( name);

	if (!m_transaction.get()) beginTransaction();
	DatabaseValueBuffer buffer;
	buffer[ sidx];
	m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
}

SampleIndex DatabaseAdapter::readNofSamples() const
{
	return readVariable( VARIABLE_NOF_SAMPLES);
}

std::vector<SimHash> DatabaseAdapter::readSimhashVector( const KeyPrefix& prefix) const
{
	std::vector<SimHash> rt;
	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read sim hash vector: %s"), m_errorhnd->fetchError());

	DatabaseKeyBuffer key( prefix);
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		SampleIndex sidx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner[ sidx];
		if (sidx != (SampleIndex)rt.size()+1) throw strus::runtime_error( _TXT("keys not ascending in '%s' structure"), "simhash");

		SimHash vec = SimHash::createFromSerialization( cursor->value());
		rt.push_back( vec);
		slice = cursor->seekNext();
	}
	return rt;
}

void DatabaseAdapter::writeSimhashVector( const KeyPrefix& prefix, const std::vector<SimHash>& ar)
{
	if (!m_transaction.get()) beginTransaction();

	std::vector<SimHash>::const_iterator si = ar.begin(), se = ar.end();
	for (std::size_t sidx=0; si < se; ++si,++sidx)
	{
		writeSimhash( prefix, sidx, *si);
	}
}

void DatabaseAdapter::writeSimhash( const KeyPrefix& prefix, const SampleIndex& sidx, const SimHash& simHash)
{
	if (!m_transaction.get()) beginTransaction();
	DatabaseKeyBuffer key( prefix);
	key[ sidx+1];

	std::string blob( simHash.serialization());
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

std::vector<SimHash> DatabaseAdapter::readSampleSimhashVector() const
{
	return readSimhashVector( KeySampleSimHash);
}

std::vector<SimHash> DatabaseAdapter::readResultSimhashVector() const
{
	return readSimhashVector( KeyResultSimHash);
}

void DatabaseAdapter::writeResultSimhashVector( const std::vector<SimHash>& ar)
{
	return writeSimhashVector( KeyResultSimHash, ar);
}


VectorSpaceModelConfig DatabaseAdapter::readConfig() const
{
	DatabaseKeyBuffer key( KeyConfig);

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read configuration from database: %s"), m_errorhnd->fetchError());
	}
	return VectorSpaceModelConfig( content, m_errorhnd);
}

void DatabaseAdapter::writeConfig( const VectorSpaceModelConfig& config)
{
	DatabaseKeyBuffer key( KeyConfig);
	std::string content( config.tostring());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
}

LshModel DatabaseAdapter::readLshModel() const
{
	DatabaseKeyBuffer key( KeyLshModel);
	
	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		throw strus::runtime_error( _TXT( "failed to read LSH model from database: %s"), m_errorhnd->fetchError());
	}
	return LshModel::fromSerialization( content);
}

void DatabaseAdapter::writeLshModel( const LshModel& model)
{
	DatabaseKeyBuffer key( KeyLshModel);	
	std::string content( model.serialization());

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
}

SimRelationMap DatabaseAdapter::readSimRelationMap() const
{
	SimRelationMap rt;
	DatabaseKeyBuffer key( KeySimRelationMap);

	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read similarity relation map: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		SampleIndex sidx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner[ sidx];
		--sidx;

		DatabaseCursorInterface::Slice content = cursor->value();
		char const* ci = content.ptr();
		const char* ce = ci + content.size();
		if ((ce - ci) % (sizeof(SampleIndex)+sizeof(unsigned short)) != 0)
		{
			throw strus::runtime_error(_TXT("corrupt row %u of similarity relation map"), sidx);
		}
		std::vector<SimRelationMap::Element> elems;
		for (; ci < ce; ci += sizeof(SampleIndex)+sizeof(unsigned short))
		{
			SampleIndex col;
			unsigned short simdist;
			DatabaseValueScanner value_scanner( ci, (sizeof(SampleIndex)+sizeof(unsigned short)));
			value_scanner[ col][ simdist];
			elems.push_back( SimRelationMap::Element( col, simdist));
		}
		rt.addRow( sidx, elems);
		slice = cursor->seekNext();
	}
	return rt;
}

std::vector<SimRelationMap::Element> DatabaseAdapter::readSimRelations( const SampleIndex& sidx) const
{
	DatabaseKeyBuffer key( KeySimRelationMap);
	key[ sidx+1];

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read sim relations: %s"), m_errorhnd->fetchError());
		}
		return std::vector<SimRelationMap::Element>();
	}
	else
	{
		char const* ci = content.c_str();
		const char* ce = ci + content.size();
		if ((ce - ci) % (sizeof(SampleIndex)+sizeof(unsigned short)) != 0)
		{
			throw strus::runtime_error(_TXT("corrupt row %u of similarity relation map"), sidx);
		}
		std::vector<SimRelationMap::Element> rt;
		for (; ci < ce; ci += sizeof(SampleIndex)+sizeof(unsigned short))
		{
			SampleIndex col;
			unsigned short simdist;
			DatabaseValueScanner value_scanner( ci, (sizeof(SampleIndex)+sizeof(unsigned short)));
			value_scanner[ col][ simdist];
			rt.push_back( SimRelationMap::Element( col, simdist));
		}
		return rt;
	}
}

void DatabaseAdapter::writeSimRelationMap( const SimRelationMap& simrelmap)
{
	SampleIndex si = 0, se = simrelmap.nofSamples();
	for (; si != se; ++si)
	{
		SimRelationMap::Row row = simrelmap.row( si);
		if (row.begin() == row.end()) continue;

		DatabaseKeyBuffer key( KeySimRelationMap);	
		key[ si+1];

		std::string content;
		SimRelationMap::Row::const_iterator ri = row.begin(), re = row.end();
		for (; ri != re; ++ri)
		{
			DatabaseValueBuffer buffer;
			buffer[ ri->index][ ri->simdist];
			content.append( buffer.c_str(), buffer.size());
		}
		if (!m_transaction.get()) beginTransaction();
		m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
	}
}

std::vector<FeatureIndex> DatabaseAdapter::readSampleFeatureIndices( const SampleIndex& sidx) const
{
	DatabaseKeyBuffer key( KeySampleFeatureIndexMap);
	key[ sidx+1];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read sample to feature index map from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<SampleIndex>();
		}
	}
	return vectorFromSerialization<FeatureIndex>( blob);
}

void DatabaseAdapter::writeSampleFeatureIndexMap( const SampleFeatureIndexMap& sfmap)
{
	SampleIndex si = 0, se = sfmap.maxkey()+1;
	for (; si != se; ++si)
	{
		if (!m_transaction.get()) beginTransaction();
		std::vector<FeatureIndex> features = sfmap.getValues( si);
		if (features.empty()) continue;
		std::string blob = vectorSerialization<FeatureIndex>( features);

		DatabaseKeyBuffer key( KeySampleFeatureIndexMap);
		key[ si+1];
		m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
	}
}

std::vector<SampleIndex> DatabaseAdapter::readFeatureSampleIndices( const FeatureIndex& fidx) const
{
	if (fidx == 0) throw strus::runtime_error(_TXT("illegal key (null) for feature"));

	DatabaseKeyBuffer key( KeyFeatureSampleIndexMap);
	key[ fidx];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read sample to feature index map from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<SampleIndex>();
		}
	}
	return vectorFromSerialization<SampleIndex>( blob);
}

void DatabaseAdapter::writeFeatureSampleIndexMap( const FeatureSampleIndexMap& fsmap)
{
	SampleIndex fi = 1, fe = fsmap.maxkey();
	for (; fi != fe; ++fi)
	{
		if (!m_transaction.get()) beginTransaction();
		std::vector<SampleIndex> members = fsmap.getValues( fi);
		if (members.empty()) continue;
		std::string blob = vectorSerialization<SampleIndex>( members);

		DatabaseKeyBuffer key( KeyFeatureSampleIndexMap);
		key[ fi];
		m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
	}
}

void DatabaseAdapter::deleteSubTree( const KeyPrefix& prefix)
{
	if (!m_transaction.get()) beginTransaction();
	DatabaseKeyBuffer key( KeySimRelationMap);
	m_transaction->removeSubTree( key.c_str(), key.size());
}

void DatabaseAdapter::deleteVariables()
{
	deleteSubTree( KeyVariable);
}

void DatabaseAdapter::deleteSamples()
{
	deleteSubTree( KeySampleVector);
	deleteSubTree( KeySampleName);
	deleteSubTree( KeySampleNameInv);
}

void DatabaseAdapter::deleteSampleSimhashVector()
{
	deleteSubTree( KeySampleSimHash);
}

void DatabaseAdapter::deleteResultSimhashVector()
{
	deleteSubTree( KeyResultSimHash);
}

void DatabaseAdapter::deleteSimRelationMap()
{
	deleteSubTree( KeySimRelationMap);
}


void DatabaseAdapter::deleteSampleFeatureIndexMap()
{
	deleteSubTree( KeySampleFeatureIndexMap);
}

void DatabaseAdapter::deleteFeatureSampleIndexMap()
{
	deleteSubTree( KeyFeatureSampleIndexMap);
}


