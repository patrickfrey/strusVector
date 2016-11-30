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
#include "strus/base/string_format.hpp"
#include "strus/base/stdint.h"
#include "strus/versionVector.hpp"
#include "strus/databaseOptions.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "simGroup.hpp"
#include <cstring>
#include <iostream>
#include <sstream>

using namespace strus;

#define VARIABLE_NOF_SAMPLES  "samples"
#define VARIABLE_NOF_CONCEPTS "concepts"
#define VARIABLE_STATE "state"
#define VARIABLE_NOF_SIMRELATIONS "simrelations"

#undef STRUS_LOWLEVEL_DEBUG

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
		std::memcpy( name, "strus standard vector space model\n\0", sizeof(name));
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
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read version from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			throw strus::runtime_error( _TXT( "failed to read non exising version from database"));
		}
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

void DatabaseAdapter::writeVariable( const std::string& name, unsigned int value)
{
	std::string key;
	key.push_back( (char)KeyVariable);
	key.append( name);
	DatabaseValueBuffer valuebuf;
	valuebuf[ (uint64_t)value];

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), valuebuf.c_str(), valuebuf.size());
}

unsigned int DatabaseAdapter::readVariable( const std::string& name) const
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

std::vector<std::pair<std::string,uint64_t> > DatabaseAdapter::readVariables() const
{
	std::vector<std::pair<std::string,uint64_t> > rt;
	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read variables: %s"), m_errorhnd->fetchError());

	DatabaseKeyBuffer key( KeyVariable);
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		std::string name( slice.ptr()+1, slice.size()-1);
		uint64_t val;
		DatabaseCursorInterface::Slice curvalue = cursor->value();
		DatabaseValueScanner value_scanner( curvalue.ptr(), curvalue.size());
		value_scanner[ val];
		rt.push_back( std::pair<std::string,uint64_t>( name, val));
		slice = cursor->seekNext();
	}
	return rt;
}

void DatabaseAdapter::writeNofSamples( const SampleIndex& nofSamples)
{
	if (!m_transaction.get()) beginTransaction();
	writeVariable( VARIABLE_NOF_SAMPLES, nofSamples);
}

void DatabaseAdapter::writeNofSimRelations( const SampleIndex& nofSamples)
{
	if (!m_transaction.get()) beginTransaction();
	return writeVariable( VARIABLE_NOF_SIMRELATIONS, nofSamples);
}

void DatabaseAdapter::writeNofConcepts( const std::string& clname, const ConceptIndex& nofConcepts)
{
	if (!m_transaction.get()) beginTransaction();
	writeVariable( std::string(VARIABLE_NOF_CONCEPTS) + "_" + clname, nofConcepts);
}

void DatabaseAdapter::writeState( unsigned int state)
{
	if (!m_transaction.get()) beginTransaction();
	writeVariable( VARIABLE_STATE, state);
}


void DatabaseAdapter::writeSample( const SampleIndex& sidx, const std::string& name, const Vector& vec, const SimHash& simHash)
{
	if (!m_transaction.get()) beginTransaction();
	writeSampleIndex( sidx, name);
	writeSampleName( sidx, name);
	writeSampleVector( sidx, vec);
	writeSimhash( KeySampleSimHash, std::string(), sidx, simHash);
}

#ifdef STRUS_LOWLEVEL_DEBUG
static void print_value_seq( const void* sq, unsigned int sqlen)
{
	static const char* HEX = "0123456789ABCDEF";
	unsigned char const* si = (const unsigned char*) sq;
	unsigned const char* se = (const unsigned char*) sq + sqlen;
	for (; si != se; ++si)
	{
		unsigned char lo = *si % 16, hi = *si / 16;
		printf( " %c%c", HEX[hi], HEX[lo]);
	}
	printf(" |");
}
#endif

template <typename ScalarType>
static std::vector<ScalarType> vectorFromSerialization( const char* blob, std::size_t blobsize)
{
#ifdef STRUS_LOWLEVEL_DEBUG
	printf("read vector");
#endif
	std::vector<ScalarType> rt;
	rt.reserve( blobsize / sizeof(ScalarType));
	if (blobsize % sizeof(ScalarType) != 0)
	{
		throw strus::runtime_error(_TXT("corrupt data in vector serialization"));
	}
	ScalarType const* ri = (const ScalarType*)blob;
	const ScalarType* re = ri + blobsize / sizeof(ScalarType);
	for (; ri != re; ++ri)
	{
		rt.push_back( ByteOrder<ScalarType>::ntoh( *ri));
#ifdef STRUS_LOWLEVEL_DEBUG
		print_value_seq( &rt.back(), sizeof(ScalarType));
#endif
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	printf("\n");
#endif
	return rt;
}

template <typename ScalarType>
static std::vector<ScalarType> vectorFromSerialization( const std::string& blob)
{
	return vectorFromSerialization<ScalarType>( blob.c_str(), blob.size());
}

template <typename ScalarType>
static std::string vectorSerialization( const std::vector<ScalarType>& vec)
{
#ifdef STRUS_LOWLEVEL_DEBUG
	printf("write vector");
#endif
	std::string rt;
	rt.reserve( vec.size() * sizeof(ScalarType));
	typename std::vector<ScalarType>::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
#ifdef STRUS_LOWLEVEL_DEBUG
		print_value_seq( &*vi, sizeof(ScalarType));
#endif
		ScalarType buf = ByteOrder<ScalarType>::hton( *vi);
		rt.append( (const char*)&buf, sizeof(buf));
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	printf("\n");
#endif
	return rt;
}

static std::vector<std::string> stringListFromSerialization( const std::string& blob)
{
	std::vector<std::string> rt;
	char const* ci = blob.c_str();
	char const* ce = std::strchr( ci, '\n');
	for (; ce; ci = ce+1,ce=std::strchr( ci, '\n'))
	{
		rt.push_back( std::string( ci, ce-ci));
	}
	rt.push_back( ci);
	return rt;
}

static std::string stringListSerialization( const std::vector<std::string>& list)
{
	std::string rt;
	std::vector<std::string>::const_iterator ci = list.begin(), ce = list.end();
	for (; ci != ce; ++ci)
	{
		if (!rt.empty()) rt.push_back('\n');
		rt.append(*ci);
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
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read sample vector: %s"), m_errorhnd->fetchError());
		}
		else
		{
			throw strus::runtime_error(_TXT("try to to read sample vector (index %d) that does not exist"), sidx);
		}
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
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read sample name: %s"), m_errorhnd->fetchError());
		}
		else
		{
			throw strus::runtime_error(_TXT("try to to read sample name (index %d) that does not exist"), sidx);
		}
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
	key.push_back( (char)KeySampleNameInv);
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
	key.push_back( (char)KeySampleNameInv);
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

SampleIndex DatabaseAdapter::readNofSimRelations() const
{
	return readVariable( VARIABLE_NOF_SIMRELATIONS);
}

ConceptIndex DatabaseAdapter::readNofConcepts( const std::string& clname) const
{
	return readVariable( std::string(VARIABLE_NOF_CONCEPTS) + "_" + clname);
}

unsigned int DatabaseAdapter::readState() const
{
	return readVariable( VARIABLE_STATE);
}

std::vector<std::string> DatabaseAdapter::readConceptClassNames() const
{
	DatabaseKeyBuffer key( KeyConceptClassNames);

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read concept class names: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<std::string>();
		}
	}
	return stringListFromSerialization( blob);
}

void DatabaseAdapter::writeConceptClassNames( const std::vector<std::string>& clnames)
{
	DatabaseKeyBuffer key( KeyConceptClassNames);
	if (!m_transaction.get()) beginTransaction();
	std::string buffer = stringListSerialization( clnames);
	m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
}

SimHash DatabaseAdapter::readSimhash( const KeyPrefix& prefix, const std::string& clname, const SampleIndex& sidx) const
{
	DatabaseKeyBuffer key( prefix);
	key( clname)[ sidx+1];

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read sim hash: %s"), m_errorhnd->fetchError());
		}
		else
		{
			throw strus::runtime_error(_TXT("accessing sim hash value of undefined feature"));
		}
	}
	return SimHash::fromSerialization( content);
}

std::vector<SimHash> DatabaseAdapter::readSimhashVector( const KeyPrefix& prefix, const std::string& clname) const
{
	std::vector<SimHash> rt;
	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read sim hash vector: %s"), m_errorhnd->fetchError());

	DatabaseKeyBuffer key( prefix);
	key( clname);
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		char const* cl;
		std::size_t clsize;
		SampleIndex sidx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner(cl,clsize)[ sidx];
		if (sidx != (SampleIndex)rt.size()+1) throw strus::runtime_error( _TXT("keys not ascending in '%s' structure"), "simhash");

		SimHash vec = SimHash::fromSerialization( cursor->value());
		rt.push_back( vec);
		slice = cursor->seekNext();
	}
	return rt;
}

void DatabaseAdapter::writeSimhashVector( const KeyPrefix& prefix, const std::string& clname, const std::vector<SimHash>& ar)
{
	if (!m_transaction.get()) beginTransaction();

	std::vector<SimHash>::const_iterator si = ar.begin(), se = ar.end();
	for (std::size_t sidx=0; si < se; ++si,++sidx)
	{
		writeSimhash( prefix, clname, sidx, *si);
	}
}

void DatabaseAdapter::writeSimhash( const KeyPrefix& prefix, const std::string& clname, const SampleIndex& sidx, const SimHash& simHash)
{
	if (!m_transaction.get()) beginTransaction();
	DatabaseKeyBuffer key( prefix);
	key(clname)[ sidx+1];

	std::string blob( simHash.serialization());
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

SimHash DatabaseAdapter::readSampleSimhash( const SampleIndex& sidx) const
{
	return readSimhash( KeySampleSimHash, std::string(), sidx);
}

std::vector<SimHash> DatabaseAdapter::readSampleSimhashVector() const
{
	return readSimhashVector( KeySampleSimHash, std::string());
}

std::vector<SimHash> DatabaseAdapter::readConceptSimhashVector( const std::string& clname) const
{
	return readSimhashVector( KeyConceptSimhash, clname);
}

void DatabaseAdapter::writeConceptSimhashVector( const std::string& clname, const std::vector<SimHash>& ar)
{
	return writeSimhashVector( KeyConceptSimhash, clname, ar);
}


VectorSpaceModelConfig DatabaseAdapter::readConfig() const
{
	DatabaseKeyBuffer key( KeyConfig);

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read configuration from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			throw strus::runtime_error( _TXT( "failed to read non existing configuration from database"));
		}
	}
	return VectorSpaceModelConfig( content, m_errorhnd);
}

void DatabaseAdapter::writeConfig( const VectorSpaceModelConfig& config)
{
	DatabaseKeyBuffer key( KeyConfig);
	std::string content( config.tostring( false));

	if (!m_transaction.get()) beginTransaction();
	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
}

bool DatabaseAdapter::isempty()
{
	DatabaseKeyBuffer key( KeyConfig);

	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read configuration from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return true;
		}
	}
	return false;
}

LshModel DatabaseAdapter::readLshModel() const
{
	DatabaseKeyBuffer key( KeyLshModel);
	
	std::string content;
	if (!m_database->readValue( key.c_str(), key.size(), content, DatabaseOptions()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read LSH model from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			throw strus::runtime_error( _TXT( "failed to read non existing LSH model from database"));
		}
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

		DatabaseCursorInterface::Slice curvalue = cursor->value();
		DatabaseValueScanner value_scanner( curvalue.ptr(), curvalue.size());
		std::vector<SimRelationMap::Element> elems;
		while (!value_scanner.eof())
		{
			SampleIndex col;
			unsigned short simdist;
			value_scanner[ col][ simdist];
			elems.push_back( SimRelationMap::Element( col, simdist));
		}
		rt.addRow( sidx, elems);
		slice = cursor->seekNext();
	}
	return rt;
}

SampleIndex DatabaseAdapter::readLastSimRelationIndex() const
{
	DatabaseKeyBuffer key( KeySimRelationMap);

	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read similarity relation map: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice slice = cursor->seekLast( key.c_str(), key.size());
	if (slice.defined())
	{
		SampleIndex sidx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner[ sidx];
		--sidx;
		return sidx;
	}
	else
	{
		return -1;
	}
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
		DatabaseValueScanner value_scanner( content.c_str(), content.size());
		std::vector<SimRelationMap::Element> rt;
		while (!value_scanner.eof())
		{
			SampleIndex col;
			unsigned short simdist;
			value_scanner[ col][ simdist];
			rt.push_back( SimRelationMap::Element( col, simdist));
		}
		return rt;
	}
}

void DatabaseAdapter::writeSimRelationRow( const SampleIndex& sidx, const SimRelationMap::Row& row)
{
	DatabaseKeyBuffer key( KeySimRelationMap);
	key[ sidx+1];

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

void DatabaseAdapter::writeSimRelationMap( const SimRelationMap& simrelmap)
{
	SampleIndex si = simrelmap.startIndex(), se = simrelmap.endIndex();
	for (; si != se; ++si)
	{
		SimRelationMap::Row row = simrelmap.row( si);
		if (row.begin() == row.end()) continue;

		writeSimRelationRow( si, row);
	}
}

std::vector<SampleIndex> DatabaseAdapter::readSimSingletons() const
{
	std::vector<SampleIndex> rt;
	DatabaseKeyBuffer key( KeySimRelationMap);

	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read similarity relation map: %s"), m_errorhnd->fetchError());

	SampleIndex prev_sidx = 0;
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		SampleIndex sidx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner[ sidx];
		--sidx;

		for (SampleIndex si=prev_sidx; si < sidx; ++si)
		{
			rt.push_back( si);
		}
		DatabaseCursorInterface::Slice curvalue = cursor->value();
		DatabaseValueScanner value_scanner( curvalue.ptr(), curvalue.size());
		if (value_scanner.eof())
		{
			rt.push_back( sidx);
		}
		prev_sidx = sidx;
		slice = cursor->seekNext();
	}
	SampleIndex end_sidx = readNofSamples();
	for (SampleIndex si=prev_sidx; si < end_sidx; ++si)
	{
		rt.push_back( si);
	}
	return rt;
}

std::vector<ConceptIndex> DatabaseAdapter::readSampleConceptIndices( const std::string& clname, const SampleIndex& sidx) const
{
	DatabaseKeyBuffer key( KeySampleConceptIndexMap);
	key(clname)[ sidx+1];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read sample to concept index map from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<ConceptIndex>();
		}
	}
	return vectorFromSerialization<ConceptIndex>( blob);
}

void DatabaseAdapter::writeSampleConceptIndexMap( const std::string& clname, const SampleConceptIndexMap& sfmap)
{
	SampleIndex si = 0, se = sfmap.maxkey()+1;
	for (; si != se; ++si)
	{
		if (!m_transaction.get()) beginTransaction();
		std::vector<ConceptIndex> concepts = sfmap.getValues( si);
		if (concepts.empty()) continue;
		std::string blob = vectorSerialization<ConceptIndex>( concepts);

		DatabaseKeyBuffer key( KeySampleConceptIndexMap);
		key(clname)[ si+1];
		m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
	}
}

std::vector<SampleIndex> DatabaseAdapter::readConceptSampleIndices( const std::string& clname, const ConceptIndex& cidx) const
{
	if (cidx == 0)
	{
		throw strus::runtime_error(_TXT("illegal key (null) for concept"));
	}
	DatabaseKeyBuffer key( KeyConceptSampleIndexMap);
	key(clname)[ cidx];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read sample to concept index map from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<SampleIndex>();
		}
	}
	return vectorFromSerialization<SampleIndex>( blob);
}

SampleConceptIndexMap DatabaseAdapter::readSampleConceptIndexMap( const std::string& clname) const
{
	return readIndexListMap( KeySampleConceptIndexMap, clname);
}

void DatabaseAdapter::writeConceptSampleIndexMap( const std::string& clname, const ConceptSampleIndexMap& csmap)
{
	ConceptIndex ci = 1, ce = csmap.maxkey()+1;
	for (; ci != ce; ++ci)
	{
		if (!m_transaction.get()) beginTransaction();
		std::vector<SampleIndex> members = csmap.getValues( ci);
		if (members.empty()) continue;
		std::string blob = vectorSerialization<SampleIndex>( members);

		DatabaseKeyBuffer key( KeyConceptSampleIndexMap);
		key(clname)[ ci];
		m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
	}
}

ConceptSampleIndexMap DatabaseAdapter::readConceptSampleIndexMap( const std::string& clname) const
{
	return readIndexListMap( KeyConceptSampleIndexMap, clname);
}

IndexListMap<strus::Index,strus::Index> DatabaseAdapter::readIndexListMap( const KeyPrefix& prefix, const std::string& clname) const
{
	IndexListMap<strus::Index,strus::Index> rt;
	DatabaseKeyBuffer key( prefix);
	key(clname);

	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read index map: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		const char* cl;
		std::size_t clsize;
		strus::Index idx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner(cl,clsize)[ idx];
		if (prefix == KeySampleConceptIndexMap) --idx;

		DatabaseCursorInterface::Slice content = cursor->value();
		char const* ci = content.ptr();
		const char* ce = ci + content.size();
		if ((ce - ci) % (sizeof(strus::Index)) != 0)
		{
			throw strus::runtime_error(_TXT("corrupt row of index list map"));
		}
		std::vector<strus::Index> elems;
		for (; ci < ce; ci += sizeof(strus::Index))
		{
			strus::Index val;
			DatabaseValueScanner value_scanner( ci, (sizeof(strus::Index)));
			value_scanner[ val];
			elems.push_back( val);
		}
		rt.add( idx, elems);
		slice = cursor->seekNext();
	}
	return rt;
}

std::vector<SampleIndex> DatabaseAdapter::readConceptSingletons( const std::string& clname) const
{
	std::vector<SampleIndex> rt;
	DatabaseKeyBuffer key( KeySampleConceptIndexMap);
	key(clname);

	std::auto_ptr<DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create cursor to read similarity relation map: %s"), m_errorhnd->fetchError());

	SampleIndex prev_sidx = 0;
	DatabaseCursorInterface::Slice slice = cursor->seekFirst( key.c_str(), key.size());
	while (slice.defined())
	{
		const char* cl;
		std::size_t clsize;
		SampleIndex sidx;
		DatabaseKeyScanner key_scanner( slice.ptr()+1, slice.size()-1);
		key_scanner(cl,clsize)[ sidx];
		--sidx;

		for (SampleIndex si=prev_sidx; si < sidx; ++si)
		{
			rt.push_back( si);
		}
		DatabaseCursorInterface::Slice curvalue = cursor->value();
		DatabaseValueScanner value_scanner( curvalue.ptr(), curvalue.size());
		if (value_scanner.eof())
		{
			rt.push_back( sidx);
		}
		prev_sidx = sidx;
		slice = cursor->seekNext();
	}
	SampleIndex end_sidx = readNofSamples();
	for (SampleIndex si=prev_sidx; si < end_sidx; ++si)
	{
		rt.push_back( si);
	}
	return rt;
}

std::vector<ConceptIndex> DatabaseAdapter::readConceptDependencies( const std::string& clname, const ConceptIndex& cidx, const std::string& depclname) const
{
	DatabaseKeyBuffer key( KeyConceptDependency);
	key(clname)(depclname)[ cidx];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error( _TXT( "failed to read concept cover dependencies from database: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<ConceptIndex>();
		}
	}
	return vectorFromSerialization<ConceptIndex>( blob);
}

void DatabaseAdapter::writeConceptDependencies( const std::string& clname, const ConceptIndex& cidx, const std::string& depclname, const std::vector<ConceptIndex>& deplist)
{
	std::string blob = vectorSerialization<SampleIndex>( deplist);
	DatabaseKeyBuffer key( KeyConceptDependency);
	key(clname)(depclname)[ cidx];
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

void DatabaseAdapter::deleteSubTree( const KeyPrefix& prefix)
{
	if (!m_transaction.get()) beginTransaction();
	DatabaseKeyBuffer key( prefix);
	m_transaction->removeSubTree( key.c_str(), key.size());
}

void DatabaseAdapter::clear()
{
	deleteConfig();
	deleteVariables();
	deleteConceptClassNames();
	deleteSamples();
	deleteSampleSimhashVectors();
	deleteConceptSimhashVectors();
	deleteLshModel();
	deleteSimRelationMap();
	deleteSampleConceptIndexMaps();
	deleteConceptSampleIndexMaps();
	deleteConceptDependencies();
	writeState( 0);
}

void DatabaseAdapter::deleteConfig()
{
	deleteSubTree( KeyVariable);
}

void DatabaseAdapter::deleteVariables()
{
	deleteSubTree( KeyVariable);
}

void DatabaseAdapter::deleteConceptClassNames()
{
	deleteSubTree( KeyConceptClassNames);
}

void DatabaseAdapter::deleteSamples()
{
	deleteSubTree( KeySampleVector);
	deleteSubTree( KeySampleName);
	deleteSubTree( KeySampleNameInv);
}

void DatabaseAdapter::deleteSampleSimhashVectors()
{
	deleteSubTree( KeySampleSimHash);
}

void DatabaseAdapter::deleteConceptSimhashVectors()
{
	deleteSubTree( KeyConceptSimhash);
}

void DatabaseAdapter::deleteSimRelationMap()
{
	deleteSubTree( KeySimRelationMap);
}

void DatabaseAdapter::deleteSampleConceptIndexMaps()
{
	deleteSubTree( KeySampleConceptIndexMap);
}

void DatabaseAdapter::deleteConceptSampleIndexMaps()
{
	deleteSubTree( KeyConceptSampleIndexMap);
}

void DatabaseAdapter::deleteConceptDependencies()
{
	deleteSubTree( KeyConceptDependency);
}

void DatabaseAdapter::deleteLshModel()
{
	deleteSubTree( KeyLshModel);
}

struct DatabaseKeyNameTab
{
	const char* ar[ 96];
	DatabaseKeyNameTab()
	{
		std::memset( ar, 0, sizeof(ar));
		ar[ DatabaseAdapter::KeyVersion - 32] = "version";
		ar[ DatabaseAdapter::KeyVariable - 32] = "variable";
		ar[ DatabaseAdapter::KeyConceptClassNames - 32] = "classname";
		ar[ DatabaseAdapter::KeySampleVector - 32] = "featvec";
		ar[ DatabaseAdapter::KeySampleName - 32] = "featname";

		ar[ DatabaseAdapter::KeySampleNameInv - 32] = "featidx";
		ar[ DatabaseAdapter::KeySampleSimHash - 32] = "featlsh";
		ar[ DatabaseAdapter::KeyConceptSimhash - 32] = "conlsh";
		ar[ DatabaseAdapter::KeyConfig - 32] = "config";
		ar[ DatabaseAdapter::KeyLshModel - 32] = "lshmodel";

		ar[ DatabaseAdapter::KeySimRelationMap - 32] = "simrel";
		ar[ DatabaseAdapter::KeySampleConceptIndexMap - 32] = "featcon";
		ar[ DatabaseAdapter::KeyConceptSampleIndexMap - 32] = "confeat";
		ar[ DatabaseAdapter::KeyConceptDependency -32] = "condep";
	}
	const char* operator[]( DatabaseAdapter::KeyPrefix i) const
	{
		return ar[i-32];
	}
};

static const DatabaseKeyNameTab keyNameTab;

void DatabaseAdapter::dumpKeyValue( std::ostream& out, const strus::DatabaseCursorInterface::Slice& key, const strus::DatabaseCursorInterface::Slice& value)
{
	out << keyNameTab[ (KeyPrefix)key.ptr()[0]] << ": ";
	switch ((KeyPrefix)key.ptr()[0])
	{
		case DatabaseAdapter::KeyVersion:
		{
			VectorSpaceModelHdr hdr;
			if (value.size() < sizeof(hdr)) throw strus::runtime_error(_TXT("unknown version format"));
			std::memcpy( &hdr, value.ptr(), sizeof(hdr));
			hdr.ntoh();
			out << hdr.name << " " << hdr.version_major << "." << hdr.version_minor << std::endl;
			break;
		}
		case DatabaseAdapter::KeyVariable:
		{
			uint64_t varvalue;
			DatabaseValueScanner scanner( value.ptr(), value.size());
			scanner[ varvalue];
			out << std::string(key.ptr()+1,key.size()-1) << " " << varvalue << std::endl;
			break;
		}
		case DatabaseAdapter::KeyConceptClassNames:
		{
			std::vector<std::string> list( stringListFromSerialization( value));
			std::vector<std::string>::const_iterator ci = list.begin(), ce = list.end();
			for (; ci != ce; ++ci)
			{
				out << " " << *ci;
			}
			break;
		}
		case DatabaseAdapter::KeySampleVector:
		{
			SampleIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner[ sidx];
			out << (sidx-1);
			std::vector<double> vec = vectorFromSerialization<double>( value.ptr(), value.size());
			std::vector<double>::const_iterator vi = vec.begin(), ve = vec.end();
			for (; vi != ve; ++vi)
			{
				out << " " << *vi;
			}
			out << std::endl;
			break;
		}
		case DatabaseAdapter::KeySampleName:
		{
			SampleIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner[ sidx];
			out << (sidx-1) << " " << std::string(value.ptr(),value.size()) << std::endl;
			break;
		}
		case DatabaseAdapter::KeySampleNameInv:
		{
			uint32_t sidx;
			DatabaseValueScanner scanner(value.ptr(), value.size());
			scanner[ sidx];
			out << std::string(key.ptr()+1,key.size()-1) << " " << (sidx-1) << std::endl;
			break;
		}
		case DatabaseAdapter::KeySampleSimHash:
		{
			const char* cl;
			std::size_t clsize;
			SampleIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner(cl,clsize)[ sidx];
			SimHash sh = SimHash::fromSerialization( value.ptr(), value.size());
			out << std::string(cl,clsize) << " " << (sidx-1) << " " << sh.tostring() << std::endl;
			break;
		}
		case DatabaseAdapter::KeyConceptSimhash:
		{
			char const* cl;
			std::size_t clsize;
			SampleIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner(cl,clsize)[ sidx];
			SimHash sh = SimHash::fromSerialization( value.ptr(), value.size());
			out << std::string(cl,clsize) << " " << sidx << " " << sh.tostring() << std::endl;
			break;
		}
		case DatabaseAdapter::KeyConfig:
		{
			out << std::string(value);
			break;
		}
		case DatabaseAdapter::KeyLshModel:
		{
			LshModel lshmodel = LshModel::fromSerialization( value.ptr(), value.size());
			out << std::endl << lshmodel.tostring() << std::endl;
			break;
		}
		case DatabaseAdapter::KeySimRelationMap:
		{
			SampleIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner[ sidx];
			out << (sidx-1);
	
			DatabaseValueScanner value_scanner( value.ptr(), value.size());
			std::vector<SimRelationMap::Element> rt;
			while (!value_scanner.eof())
			{
				SampleIndex col;
				unsigned short simdist;
				value_scanner[ col][ simdist];
				out << " " << col << ":" << simdist;
			}
			out << std::endl;
			break;
		}
		case DatabaseAdapter::KeySampleConceptIndexMap:
		{
			const char* cl;
			std::size_t clsize;
			SampleIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner(cl,clsize)[ sidx];
			out << std::string(cl,clsize) << " " << (sidx-1);

			std::vector<ConceptIndex> far = vectorFromSerialization<ConceptIndex>( value);
			std::vector<ConceptIndex>::const_iterator fi = far.begin(), fe = far.end();
			for (; fi != fe; ++fi)
			{
				out << " " << *fi;
			}
			out << std::endl;
			break;
		}
		case DatabaseAdapter::KeyConceptSampleIndexMap:
		{
			const char* cl;
			std::size_t clsize;
			ConceptIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner(cl,clsize)[ sidx];
			out << std::string(cl,clsize) << " " << sidx;

			std::vector<SampleIndex> far = vectorFromSerialization<SampleIndex>( value);
			std::vector<SampleIndex>::const_iterator fi = far.begin(), fe = far.end();
			for (; fi != fe; ++fi)
			{
				out << " " << *fi;
			}
			out << std::endl;
			break;
		}
		case DatabaseAdapter::KeyConceptDependency:
		{
			const char* cl;
			std::size_t clsize;
			const char* depcl;
			std::size_t depclsize;
			ConceptIndex sidx;
			DatabaseKeyScanner scanner( key.ptr()+1, key.size()-1);
			scanner(cl,clsize)(depcl,depclsize)[ sidx];
			std::string clstr( cl,clsize);
			std::string depclstr( depcl,depclsize);
			out << (clsize?clstr:std::string("_")) << " " << (depclsize?depclstr:std::string("_")) << " " << sidx;

			std::vector<ConceptIndex> far = vectorFromSerialization<ConceptIndex>( value);
			std::vector<ConceptIndex>::const_iterator fi = far.begin(), fe = far.end();
			for (; fi != fe; ++fi)
			{
				out << " " << *fi;
			}
			out << std::endl;
			break;
		}
		default:
		{
			throw strus::runtime_error( _TXT( "illegal data base key prefix '%c' for this vector space model storage"), (char)((KeyPrefix)key.ptr()[0]));
		}
	}
}

bool DatabaseAdapter::dumpFirst( std::ostream& out, const std::string& keyprefix)
{
	m_cursor.reset( m_database->createCursor( DatabaseOptions()));
	if (!m_cursor.get()) throw strus::runtime_error(_TXT("error creating database cursor"));
	m_cursorkey = keyprefix;
	strus::DatabaseCursorInterface::Slice key = m_cursor->seekFirst( m_cursorkey.c_str(), m_cursorkey.size());
	if (!key.defined()) return false;
	dumpKeyValue( out, key, m_cursor->value());
	return true;
}

bool DatabaseAdapter::dumpNext( std::ostream& out)
{
	strus::DatabaseCursorInterface::Slice key = m_cursor->seekNext();
	if (!key.defined()) return false;
	std::string val( m_cursor->value());
	dumpKeyValue( out, key, m_cursor->value());
	return true;
}


