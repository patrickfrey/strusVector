/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Database abstraction (wrapper) for the standard strus vector storage
#ifndef _STRUS_VECTOR_STORAGE_DATABASE_ADAPTER_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_DATABASE_ADAPTER_HPP_INCLUDED
#include "strus/databaseInterface.hpp"
#include "strus/databaseClientInterface.hpp"
#include "strus/databaseTransactionInterface.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/wordVector.hpp"
#include "strus/reference.hpp"
#include "strus/index.hpp"
#include "strus/base/stdint.h"
#include "simHash.hpp"
#include "lshModel.hpp"
#include "stringList.hpp"
#include <vector>
#include <string>
#include <iostream>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

class DatabaseAdapter
{
public:
	DatabaseAdapter( const DatabaseInterface* database_, const std::string& config, ErrorBufferInterface* errorhnd_);
	DatabaseAdapter( const DatabaseAdapter& o);
	~DatabaseAdapter(){}

	void checkVersion();

	typedef std::pair<std::string,std::string> VariableDef;
	std::vector<VariableDef> readVariables() const;
	std::string readVariable( const std::string& name) const;

	std::vector<std::string> readTypes() const;

	Index readNofTypeno() const;
	Index readNofFeatno() const;
	Index readTypeno( const std::string& type) const;
	Index readFeatno( const std::string& feature) const;
	std::string readTypeName( const Index& typeno) const;
	std::string readFeatName( const Index& featno) const;

	class FeatureCursor
	{
	public:
		explicit FeatureCursor( const DatabaseClientInterface* database_);
		FeatureCursor( const FeatureCursor& o)
			:m_cursor(o.m_cursor){}
		bool skip( const char* keyptr, std::size_t keysize, std::string& keyfound);
		bool skip( const std::string& key, std::string& keyfound);
		bool skipPrefix( const char* keyptr, std::size_t keysize, std::string& keyfound);
		bool skipPrefix( const std::string& key, std::string& keyfound);
		bool loadFirst( std::string& key);
		bool loadNext( std::string& key);
		bool loadNextPrefix( const std::string& keyprefix, std::string& key);

	private:
		Reference<DatabaseCursorInterface> m_cursor;
	};

	std::vector<Index> readFeatureTypeRelations( const Index& featno) const;
	int readNofVectors( const Index& typeno) const;
	Index readFeatnoStart( const Index& typeno, int idx) const;

	WordVector readVector( const Index& typeno, const Index& featno) const;
	SimHash readSimHash( const Index& typeno, const Index& featno) const;
	std::vector<SimHash> readSimHashVector( const Index& typeno, const Index& featnostart, int numberOfResults) const;
	std::vector<SimHash> readSimHashVector( const Index& typeno) const;

	LshModel readLshModel() const;

	void close();
	void compaction();

private:
	Index readIndexValue( const char* keystr, std::size_t keysize, bool errorIfNotFound) const;
	std::string readStringValue( const char* keystr, std::size_t keysize, bool errorIfNotFound) const;

public:
	enum KeyPrefix
	{
		KeyVariable = 'A',			///< [variable string]         ->  [value string]
		KeyFeatureTypePrefix='T',		///< [type string]             ->  [typeno]
		KeyFeatureValuePrefix='I',		///< [feature string]          ->  [featno]
		KeyFeatureTypeInvPrefix='t',		///< [typeno]                  ->  [type string]
		KeyFeatureValueInvPrefix='i',		///< [featno]                  ->  [feature string]
		KeyFeatureVector='V',			///< [typeno,featno]           ->  [float...]
		KeyFeatureSimHash='H',			///< [typeno,featno]           ->  [SimHash]
		KeyNofVectors='N',			///< [typeno]                  ->  [nof]
		KeyNofTypeno='Y',			///< []                        ->  [nof]
		KeyNofFeatno='Z',			///< []                        ->  [nof]
		KeyLshModel = 'L',			///< []                        ->  [dim,bits,variations,matrix...]
		KeyFeatureTypeRelations = 'R' 		///< [featno]                  ->  [typeno...]
	};

	class Transaction
	{
	public:
		Transaction( DatabaseClientInterface* database, ErrorBufferInterface* errorhnd_);

		void writeVersion();
		void writeVariable( const std::string& name, const std::string& value);

		void writeType( const std::string& type, const Index& typeno);
		void writeFeature( const std::string& feature, const Index& featno);
		void writeFeatureTypeRelations( const Index& featno, const std::vector<Index>& typenolist);

		void writeNofTypeno( const Index& typeno);
		void writeNofFeatno( const Index& featno);
		void writeNofVectors( const Index& typeno, const Index& nofVectors);

		void writeVector( const Index& typeno, const Index& featno, const WordVector& vec);
		void writeSimHash( const Index& typeno, const Index& featno, const SimHash& hash);

		void writeLshModel( const LshModel& model);

		void clear();

		bool commit();
		void rollback();

	private:
		void deleteSubTree( const KeyPrefix& prefix);

	private:
		ErrorBufferInterface* m_errorhnd;
		Reference<DatabaseTransactionInterface> m_transaction;
	};

	Transaction* createTransaction()
	{
		return new Transaction( m_database.get(), m_errorhnd);
	}

	class DumpIterator
	{
	public:
		DumpIterator( const DatabaseClientInterface* database, ErrorBufferInterface* errorhnd_);

		bool dumpNext( std::ostream& out);

	private:
		void dumpKeyValue( std::ostream& out, const strus::DatabaseCursorInterface::Slice& key, const strus::DatabaseCursorInterface::Slice& value);

	private:
		ErrorBufferInterface* m_errorhnd;
		Reference<DatabaseCursorInterface> m_cursor;
		int m_keyidx;
		bool m_first;
	};

	DumpIterator* createDumpIterator() const
	{
		return new DumpIterator( m_database.get(), m_errorhnd);
	}
	const DatabaseClientInterface* database() const
	{
		return m_database.get();
	}

private:
	Reference<DatabaseClientInterface> m_database;
	ErrorBufferInterface* m_errorhnd;
};


} //namespace
#endif

