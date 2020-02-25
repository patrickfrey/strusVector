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
#include "strus/base/local_ptr.hpp"
#include "strus/versionVector.hpp"
#include "strus/storage/databaseOptions.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "simHash.hpp"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <limits>
#include <memory>

using namespace strus;

#define MODULENAME   "vector storage"

DatabaseAdapter::DatabaseAdapter( const DatabaseInterface* database_, const std::string& config_, ErrorBufferInterface* errorhnd_)
	:m_database(database_->createClient(config_)),m_errorhnd(errorhnd_)
{
	if (!m_database.get()) throw strus::runtime_error( _TXT("failed to create database client for %s: %s"), MODULENAME, m_errorhnd->fetchError());
}

DatabaseAdapter::DatabaseAdapter( const DatabaseAdapter& o)
	:m_database(o.m_database),m_errorhnd(o.m_errorhnd)
{}

DatabaseAdapter::Transaction::Transaction( DatabaseClientInterface* database, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_transaction( database->createTransaction())
{
	if (!m_transaction.get())
	{
		throw strus::runtime_error( _TXT("vector storage create transaction failed: %s"), m_errorhnd->fetchError());
	}
	
}

bool DatabaseAdapter::Transaction::commit()
{
	return m_transaction->commit();
}

void DatabaseAdapter::Transaction::rollback()
{
	m_transaction->rollback();
}

Index DatabaseAdapter::readIndexValue( const char* keystr, std::size_t keysize, bool errorIfNotFound) const
{
	std::string blob;
	if (!m_database->readValue( keystr, keysize, blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read index value from vector database: %s"), m_errorhnd->fetchError());
		}
		if (errorIfNotFound)
		{
			throw strus::runtime_error(_TXT("required key not found in vector database"));
		}
		return 0;
	}
	DatabaseValueScanner scanner( blob);
	Index rt = 0;
	scanner[ rt];
	return rt;
}

std::string DatabaseAdapter::readStringValue( const char* keystr, std::size_t keysize, bool errorIfNotFound) const
{
	std::string rt;
	if (!m_database->readValue( keystr, keysize, rt, DatabaseOptions()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read string value from vector database: %s"), m_errorhnd->fetchError());
		}
		if (errorIfNotFound)
		{
			throw strus::runtime_error(_TXT("required key not found in vector database"));
		}
		return std::string();
	}
	return rt;
}

template <typename ScalarType>
static std::vector<ScalarType> vectorFromSerialization( const char* blob, std::size_t blobsize)
{
	typedef typename ByteOrder<ScalarType>::net_value_type NetScalarType;

	std::vector<ScalarType> rt;
	rt.reserve( blobsize / sizeof(NetScalarType));
	if (blobsize % sizeof(NetScalarType) != 0)
	{
		throw std::runtime_error( _TXT("corrupt data in vector serialization"));
	}
	NetScalarType const* ri = (const NetScalarType*)blob;
	const NetScalarType* re = ri + blobsize / sizeof(NetScalarType);
	for (; ri != re; ++ri)
	{
		rt.push_back( ByteOrder<ScalarType>::ntoh( *ri));
	}
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
	typedef typename ByteOrder<ScalarType>::net_value_type NetScalarType;

	std::string rt;
	rt.reserve( vec.size() * sizeof(NetScalarType));
	typename std::vector<ScalarType>::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		NetScalarType buf = ByteOrder<ScalarType>::hton( *vi);
		rt.append( (const char*)&buf, sizeof(buf));
	}
	return rt;
}

void DatabaseAdapter::checkVersion()
{
	std::string version = readVariable( "version");
	int major = 0;
	int minor = 0;
	int patch = 0;

	if (3 > std::sscanf( version.c_str(), "%d.%d.%d", &major, &minor, &patch))
	{
		throw strus::runtime_error( _TXT( "failed to read version of vector database: format of starage different"));
	}
	if (STRUS_VECTOR_VERSION_MAJOR != major)
	{
		throw strus::runtime_error( _TXT( "major version of vector database does not match"));
	}
	if (STRUS_VECTOR_VERSION_MINOR < minor)
	{
		throw strus::runtime_error( _TXT( "minor version of vector database is not compatible"));
	}
}

void DatabaseAdapter::Transaction::writeVersion()
{
	writeVariable( "version", STRUS_VECTOR_VERSION_STRING);
}

void DatabaseAdapter::Transaction::writeVariable( const std::string& name, const std::string& value)
{
	DatabaseKeyBuffer key( KeyVariable);
	key( name);
	m_transaction->write( key.c_str(), key.size(), value.c_str(), value.size());
}

std::string DatabaseAdapter::readVariable( const std::string& name) const
{
	DatabaseKeyBuffer key( KeyVariable);
	key( name);
	return readStringValue( key.c_str(), key.size(), false);
}

std::vector<DatabaseAdapter::VariableDef> DatabaseAdapter::readVariables() const
{
	std::vector<VariableDef> rt;
	DatabaseKeyBuffer keyprefix( KeyVariable);

	strus::local_ptr<strus::DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create database cursor: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice key = cursor->seekFirst( keyprefix.c_str(), keyprefix.size());
	for (; key.defined(); key = cursor->seekNext())
	{
		std::string varname = std::string( key.ptr()+1, key.size()-1);
		rt.push_back( VariableDef( varname, cursor->value()));
	}
	return rt;
}

std::vector<std::string> DatabaseAdapter::readTypes() const
{
	std::vector<std::string> rt;
	DatabaseKeyBuffer keyprefix( KeyFeatureTypePrefix);

	strus::local_ptr<strus::DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create database cursor: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice key = cursor->seekFirst( keyprefix.c_str(), keyprefix.size());
	for (; key.defined(); key = cursor->seekNext())
	{
		std::string type = std::string( key.ptr()+1, key.size()-1);
		rt.push_back( type);
	}
	return rt;
}

void DatabaseAdapter::Transaction::writeType( const std::string& type, const Index& typeno)
{
	{
		DatabaseKeyBuffer key( KeyFeatureTypePrefix);
		key( type);
		DatabaseValueBuffer buffer;
		buffer[ typeno];
		m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
	}{
		DatabaseKeyBuffer key( KeyFeatureTypeInvPrefix);
		key[ typeno];
		DatabaseValueBuffer buffer;
		m_transaction->write( key.c_str(), key.size(), type.c_str(), type.size());
	}
}

Index DatabaseAdapter::readTypeno( const std::string& type) const
{
	DatabaseKeyBuffer key( KeyFeatureTypePrefix);
	key( type);
	return readIndexValue( key.c_str(), key.size(), false);
}

std::string DatabaseAdapter::readTypeName( const Index& typeno) const
{
	DatabaseKeyBuffer key( KeyFeatureTypeInvPrefix);
	key[ typeno];
	return readStringValue( key.c_str(), key.size(), true);
}

void DatabaseAdapter::Transaction::writeFeature( const std::string& feature, const Index& featno)
{
	{
		DatabaseKeyBuffer key( KeyFeatureValuePrefix);
		key( feature);
		DatabaseValueBuffer buffer;
		buffer[ featno];
		m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
	}{
		DatabaseKeyBuffer key( KeyFeatureValueInvPrefix);
		key[ featno];
		DatabaseValueBuffer buffer;
		m_transaction->write( key.c_str(), key.size(), feature.c_str(), feature.size());
	}
}

Index DatabaseAdapter::readFeatno( const std::string& feature) const
{
	DatabaseKeyBuffer key( KeyFeatureValuePrefix);
	key( feature);
	return readIndexValue( key.c_str(), key.size(), false);
}

std::string DatabaseAdapter::readFeatName( const Index& featno) const
{
	DatabaseKeyBuffer key( KeyFeatureValueInvPrefix);
	key[ featno];
	return readStringValue( key.c_str(), key.size(), true);
}

DatabaseAdapter::FeatureCursor::FeatureCursor( const DatabaseClientInterface* database)
	:m_cursor( database->createCursor( DatabaseOptions()))
{}

bool DatabaseAdapter::FeatureCursor::skip( const char* keyptr, std::size_t keysize, std::string& keyfound)
{
	std::string dbkey;
	dbkey.push_back( KeyFeatureValuePrefix);
	dbkey.append( keyptr, keysize);
	DatabaseCursorInterface::Slice reskey( m_cursor->seekUpperBound( dbkey.c_str(), dbkey.size(), 1/*domain key size*/));
	if (!reskey.defined()) return false;
	keyfound.clear();
	keyfound.append( reskey.ptr()+1, reskey.size()-1);
	return true;
}

bool DatabaseAdapter::FeatureCursor::skip( const std::string& key, std::string& keyfound)
{
	return skip( key.c_str(), key.size(), keyfound);
}

bool DatabaseAdapter::FeatureCursor::skipPrefix( const std::string& key, std::string& keyfound)
{
	return skip( key, keyfound) && key.size() <= keyfound.size() && 0==std::memcmp( key.c_str(), keyfound.c_str(), key.size());
}

bool DatabaseAdapter::FeatureCursor::skipPrefix( const char* keyptr, std::size_t keysize, std::string& keyfound)
{
	return skip( keyptr, keysize, keyfound) && keysize <= keyfound.size() && 0==std::memcmp( keyptr, keyfound.c_str(), keysize);
}

bool DatabaseAdapter::FeatureCursor::loadFirst( std::string& key)
{
	std::string dbkey;
	dbkey.push_back( KeyFeatureValuePrefix);
	DatabaseCursorInterface::Slice reskey = m_cursor->seekFirst( dbkey.c_str(), dbkey.size());
	if (!reskey.defined()) return false;
	key.clear();
	key.append( reskey.ptr()+1, reskey.size()-1);
	return true;
}

bool DatabaseAdapter::FeatureCursor::loadNext( std::string& key)
{
	DatabaseCursorInterface::Slice reskey = m_cursor->seekNext();
	if (!reskey.defined()) return false;
	key.clear();
	key.append( reskey.ptr()+1, reskey.size()-1);
	return true;
}

bool DatabaseAdapter::FeatureCursor::loadNextPrefix( const std::string& keyprefix, std::string& key)
{
	return loadNext( key) && keyprefix.size() <= key.size() && 0==std::memcmp( keyprefix.c_str(), key.c_str(), keyprefix.size());
}

std::vector<Index> DatabaseAdapter::readFeatureTypeRelations( const Index& featno) const
{
	DatabaseKeyBuffer key( KeyFeatureTypeRelations);
	key[ featno];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read feature vector: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return std::vector<Index>();
		}
	}
	return vectorFromSerialization<Index>( blob.c_str(), blob.size());
}

void DatabaseAdapter::Transaction::writeFeatureTypeRelations( const Index& featno, const std::vector<Index>& typenolist)
{
	DatabaseKeyBuffer key( KeyFeatureTypeRelations);
	key[ featno];

	std::string blob = vectorSerialization<Index>( typenolist);
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

Index DatabaseAdapter::readFeatnoStart( const Index& typeno, int idx) const
{
	DatabaseKeyBuffer keyprefix( KeyFeatureSimHash);
	keyprefix[ typeno];
	int domainkeysize = keyprefix.size();

	strus::local_ptr<strus::DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create database cursor: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice key = cursor->seekFirst( keyprefix.c_str(), keyprefix.size());
	for (; key.defined() && idx > 0; key = cursor->seekNext(),--idx){}
	if (key.defined())
	{
		Index featno;
		DatabaseKeyScanner key_scanner( key.ptr()+domainkeysize, key.size()-domainkeysize);
		key_scanner[ featno];
		return featno;
	}
	return 0;
}

Index DatabaseAdapter::readNofTypeno() const
{
	DatabaseKeyBuffer key( KeyNofTypeno);
	return readIndexValue( key.c_str(), key.size(), false);
}

void DatabaseAdapter::Transaction::writeNofTypeno( const Index& typeno)
{
	DatabaseKeyBuffer key( KeyNofTypeno);
	DatabaseValueBuffer buffer;
	buffer[ typeno];
	m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
}

Index DatabaseAdapter::readNofFeatno() const
{
	DatabaseKeyBuffer key( KeyNofFeatno);
	return readIndexValue( key.c_str(), key.size(), false);
}

void DatabaseAdapter::Transaction::writeNofFeatno( const Index& featno)
{
	DatabaseKeyBuffer key( KeyNofFeatno);
	DatabaseValueBuffer buffer;
	buffer[ featno];
	m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
}

int DatabaseAdapter::readNofVectors( const Index& typeno) const
{
	DatabaseKeyBuffer key( KeyNofVectors);
	key[ typeno];
	return readIndexValue( key.c_str(), key.size(), false);
}

void DatabaseAdapter::Transaction::writeNofVectors( const Index& typeno, const Index& nofVectors)
{
	DatabaseKeyBuffer key( KeyNofVectors);
	key[ typeno];
	DatabaseValueBuffer buffer;
	buffer[ nofVectors];
	m_transaction->write( key.c_str(), key.size(), buffer.c_str(), buffer.size());
}

WordVector DatabaseAdapter::readVector( const Index& typeno, const Index& featno) const
{
	DatabaseKeyBuffer key( KeyFeatureVector);
	key[ typeno][ featno];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read feature vector: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return WordVector();
		}
	}
	return vectorFromSerialization<float>( blob);
}

void DatabaseAdapter::Transaction::writeVector( const Index& typeno, const Index& featno, const WordVector& vec)
{
	DatabaseKeyBuffer key( KeyFeatureVector);
	key[ typeno][ featno];

	std::string blob( vectorSerialization<float>( vec));
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

SimHash DatabaseAdapter::readSimHash( const Index& typeno, const Index& featno) const
{
	DatabaseKeyBuffer key( KeyFeatureSimHash);
	key[ typeno][ featno];

	std::string blob;
	if (!m_database->readValue( key.c_str(), key.size(), blob, DatabaseOptions().useCache()))
	{
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("failed to read feature vector: %s"), m_errorhnd->fetchError());
		}
		else
		{
			return SimHash();
		}
	}
	SimHash rt = SimHash::fromSerialization( blob);
	if (rt.id() != featno)
	{
		throw strus::runtime_error(_TXT("corrupt data in vector stored for type %d, value %d"), typeno, featno);
	}
	return rt;
}

std::vector<SimHash> DatabaseAdapter::readSimHashVector( const Index& typeno, const Index& featnostart, int numberOfResults) const
{
	DatabaseKeyBuffer keyprefix( KeyFeatureSimHash);
	keyprefix[ typeno];
	unsigned int domainkeysize = keyprefix.size();
	keyprefix[ featnostart];

	std::vector<SimHash> rt;

	strus::local_ptr<strus::DatabaseCursorInterface> cursor( m_database->createCursor( DatabaseOptions()));
	if (!cursor.get()) throw strus::runtime_error(_TXT("failed to create database cursor: %s"), m_errorhnd->fetchError());

	DatabaseCursorInterface::Slice key = cursor->seekUpperBound( keyprefix.c_str(), keyprefix.size(), domainkeysize);
	for (; key.defined() && numberOfResults > 0; key = cursor->seekNext(),--numberOfResults)
	{
		rt.push_back( SimHash::fromSerialization( cursor->value()));
		Index featno;
		DatabaseKeyScanner key_scanner( key.ptr()+domainkeysize, key.size()-domainkeysize);
		key_scanner[ featno];

		if (rt.back().id() != featno)
		{
			throw strus::runtime_error(_TXT("corrupt data in vector stored for type %d, value %d"), typeno, featno);
		}
	}
	return rt;
}

std::vector<SimHash> DatabaseAdapter::readSimHashVector( const Index& typeno) const
{
	return readSimHashVector( typeno, 1, std::numeric_limits<int>::max());
}

void DatabaseAdapter::Transaction::writeSimHash( const Index& typeno, const Index& featno, const SimHash& hash)
{
	if (hash.id() != featno)
	{
		throw strus::runtime_error(_TXT("try to store SimHash for type %d, value %d with key for value %d"), typeno, hash.id(), featno);
	}
	DatabaseKeyBuffer key( KeyFeatureSimHash);
	key[ typeno][ featno];

	std::string blob = hash.serialization();
	m_transaction->write( key.c_str(), key.size(), blob.c_str(), blob.size());
}

void DatabaseAdapter::close()
{
	m_database->compactDatabase();
	m_database->close();
}

void DatabaseAdapter::compaction()
{
	m_database->compactDatabase();
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
			throw std::runtime_error( _TXT( "failed to read non existing LSH model from database"));
		}
	}
	return LshModel::fromSerialization( content);
}

void DatabaseAdapter::Transaction::writeLshModel( const LshModel& model)
{
	DatabaseKeyBuffer key( KeyLshModel);	
	std::string content( model.serialization());

	m_transaction->write( key.c_str(), key.size(), content.c_str(), content.size());
}

void DatabaseAdapter::Transaction::deleteSubTree( const KeyPrefix& prefix)
{
	DatabaseKeyBuffer key( prefix);
	m_transaction->removeSubTree( key.c_str(), key.size());
}

void DatabaseAdapter::Transaction::clear()
{
	// Delete all data except the variables and LSH model (that are created when constructing the database)
	deleteSubTree( KeyFeatureTypePrefix);
	deleteSubTree( KeyFeatureValuePrefix);
	deleteSubTree( KeyFeatureTypeInvPrefix);
	deleteSubTree( KeyFeatureValueInvPrefix);
	deleteSubTree( KeyFeatureVector);
	deleteSubTree( KeyFeatureSimHash);
	deleteSubTree( KeyNofVectors);
	deleteSubTree( KeyNofTypeno);
	deleteSubTree( KeyNofFeatno);
	deleteSubTree( KeyFeatureTypeRelations);
}

struct DatabaseKeyNameTab
{
	const char* ar[ 96];
	DatabaseKeyNameTab()
	{
		std::memset( ar, 0, sizeof(ar));
		ar[ DatabaseAdapter::KeyVariable - 32] = "variable";
		ar[ DatabaseAdapter::KeyFeatureTypePrefix - 32] = "type";
		ar[ DatabaseAdapter::KeyFeatureValuePrefix - 32] = "value";
		ar[ DatabaseAdapter::KeyFeatureTypeInvPrefix - 32] = "typeinv";
		ar[ DatabaseAdapter::KeyFeatureValueInvPrefix - 32] = "valueinv";
		ar[ DatabaseAdapter::KeyFeatureVector - 32] = "vector";
		ar[ DatabaseAdapter::KeyFeatureSimHash - 32] = "simhash";
		ar[ DatabaseAdapter::KeyNofVectors - 32] = "nofvec";
		ar[ DatabaseAdapter::KeyNofTypeno - 32] = "noftypeno";
		ar[ DatabaseAdapter::KeyNofFeatno - 32] = "noffeatno";
		ar[ DatabaseAdapter::KeyLshModel - 32] = "lshmodel";
		ar[ DatabaseAdapter::KeyFeatureTypeRelations - 32] = "firel";
	}
	const char* operator[]( DatabaseAdapter::KeyPrefix i) const
	{
		return ar[i-32];
	}
};

static const DatabaseKeyNameTab g_keyNameTab;

void DatabaseAdapter::DumpIterator::dumpKeyValue( std::ostream& out, const strus::DatabaseCursorInterface::Slice& key, const strus::DatabaseCursorInterface::Slice& value)
{
	out << g_keyNameTab[ (KeyPrefix)key.ptr()[0]] << ": ";
	switch ((KeyPrefix)key.ptr()[0])
	{
		case DatabaseAdapter::KeyVariable:
		{
			out << std::string( key.ptr()+1, key.size()-1) << " " << value.tostring() << std::endl;
			break;
		}
		case DatabaseAdapter::KeyFeatureTypePrefix:
		case DatabaseAdapter::KeyFeatureValuePrefix:
		{
			DatabaseValueScanner scanner( value);
			Index no = 0;
			scanner[ no];
			out << std::string( key.ptr()+1,key.size()-1) << " " << no << std::endl;
			break;
		}
		case DatabaseAdapter::KeyFeatureTypeInvPrefix:
		case DatabaseAdapter::KeyFeatureValueInvPrefix:
		{
			DatabaseKeyScanner scanner( key.ptr()+1,key.size()-1);
			Index no = 0;
			scanner[ no];
			out << no << " " << value.tostring() << std::endl;
			break;
		}

		case DatabaseAdapter::KeyFeatureVector:
		{
			DatabaseKeyScanner scanner( key.ptr()+1,key.size()-1);
			Index typeno, featno;
			scanner[ typeno][ featno];
			WordVector vec = vectorFromSerialization<float>( value.ptr(), value.size());
			WordVector::const_iterator vi = vec.begin(), ve = vec.end();
			for (int vidx=0; vi!=ve; ++vi,++vidx)
			{
				if (vidx) out << ' ';
				char buf[ 100];
				std::snprintf( buf, sizeof(buf), "%.6f", *vi);
				out << buf;
			}
			out << std::endl;
			break;
		}
		case DatabaseAdapter::KeyFeatureSimHash:
		{
			DatabaseKeyScanner scanner( key.ptr()+1,key.size()-1);
			Index typeno, featno;
			scanner[ typeno][ featno];
			SimHash hash = SimHash::fromSerialization( value.ptr(), value.size());
			out << typeno << " " << featno << " " << hash.tostring() << std::endl;
			break;
		}
		case DatabaseAdapter::KeyNofVectors:
		{
			DatabaseKeyScanner keyscanner( key.ptr()+1,key.size()-1);
			Index no;
			keyscanner[ no];
			DatabaseValueScanner valscanner( value);
			Index idx = 0;
			valscanner[ idx];
			out << no << " " << idx << std::endl;
			break;
		}
		case DatabaseAdapter::KeyNofTypeno:
		case DatabaseAdapter::KeyNofFeatno:
		{
			DatabaseValueScanner valscanner( value);
			Index idx = 0;
			valscanner[ idx];
			out << idx << std::endl;
			break;
		}
		case DatabaseAdapter::KeyLshModel:
		{
			LshModel lshmodel = LshModel::fromSerialization( value.ptr(), value.size());
			out << std::endl << lshmodel.tostring() << std::endl;
			break;
		}
		case DatabaseAdapter::KeyFeatureTypeRelations:
		{
			DatabaseKeyScanner keyscanner( key.ptr()+1,key.size()-1);
			Index featno;
			keyscanner[ featno];
			std::vector<Index> vec = vectorFromSerialization<Index>( value.ptr(), value.size());
			std::vector<Index>::const_iterator vi = vec.begin(), ve = vec.end();
			for (int vidx=0; vi!=ve; ++vi,++vidx)
			{
				if (vidx) out << ' ';
				out << *vi;
			}
			out << std::endl;
			break;
		}
		default:
		{
			throw strus::runtime_error( _TXT( "unknown data base key prefix '%c' for this vector storage storage"), (char)((KeyPrefix)key.ptr()[0]));
		}
	}
}

DatabaseAdapter::DumpIterator::DumpIterator( const DatabaseClientInterface* database, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_cursor(database->createCursor( DatabaseOptions())),m_keyidx(0),m_first(true)
{
	if (!m_cursor.get()) throw std::runtime_error( _TXT("error creating database cursor"));
}

bool DatabaseAdapter::DumpIterator::dumpNext( std::ostream& out)
{
	enum {NofKeyPrefixes=12};
	static const KeyPrefix order[NofKeyPrefixes] = {
						KeyVariable,KeyFeatureTypePrefix,KeyFeatureValuePrefix,KeyFeatureTypeInvPrefix,
						KeyFeatureValueInvPrefix,KeyFeatureVector,KeyFeatureSimHash,KeyNofVectors,
						KeyNofTypeno,KeyNofFeatno,KeyLshModel,KeyFeatureTypeRelations
					};
	for (;;)
	{
		strus::DatabaseCursorInterface::Slice key;
		if (m_first)
		{
			if (m_keyidx >= NofKeyPrefixes) return false;

			char cursorkey = order[ m_keyidx];
			key = m_cursor->seekFirst( &cursorkey, 1);
		}
		else
		{
			key = m_cursor->seekNext();
		}
		if (!key.defined())
		{
			++m_keyidx;
			m_first = true;
			continue;
		}
		dumpKeyValue( out, key, m_cursor->value());
		break;
	}
	return true;
}


