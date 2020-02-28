/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Vector repository transaction implementation
#include "vectorStorageTransaction.hpp"
#include "vectorStorageClient.hpp"
#include "getSimhashValues.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/string_conv.hpp"
#include <algorithm>

#define MODULENAME   "vector storage"
#define STRUS_DBGTRACE_COMPONENT_NAME "vector"

using namespace strus;

VectorStorageTransaction::VectorStorageTransaction(
		VectorStorageClient* storage_,
		Reference<DatabaseAdapter>& database_,
		ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_storage(storage_)
	,m_database(database_),m_transaction(database_->createTransaction())
	,m_vecar(),m_typetab(errorhnd_),m_nametab(errorhnd_),m_featTypeRelations()
{
	if (errorhnd_->hasError())
	{
		throw strus::runtime_error( _TXT("failed to create transaction: %s"), errorhnd_->fetchError());
	}
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);
}

VectorStorageTransaction::~VectorStorageTransaction()
{
	if (m_debugtrace) delete m_debugtrace;
}

int VectorStorageTransaction::defineType( const std::string& type)
{
	StringConvError err = StringConvOk;
	int tid = m_typetab.getOrCreate(strus::utf8clean( type, err));
	if (err != StringConvOk) throw strus::stringconv_exception( err);
	if (tid <= 0) throw strus::runtime_error( _TXT("failed to get or create type identifier: %s"), m_errorhnd->fetchError());
	int tidx = tid-1;
	if (tidx > (int)m_vecar.size()) throw strus::runtime_error( _TXT("failed to get or create type identifier: %s"), _TXT("data corruption"));
	if (tidx == (int)m_vecar.size())
	{
		m_vecar.push_back( std::vector<VectorDef>());
	}
	return tid;
}

void VectorStorageTransaction::defineElement( const std::string& type, const std::string& name, const WordVector& vec)
{
	StringConvError err = StringConvOk;
	int tid = defineType( type);
	int tidx = tid-1;
	int fid = m_nametab.getOrCreate(strus::utf8clean( name, err));
	if (err != StringConvOk) throw strus::stringconv_exception( err);
	if (fid <= 0) throw strus::runtime_error( _TXT("failed to get or create feature identifier: %s"), m_errorhnd->fetchError());
	if (vec.empty())
	{
		m_vecar[ tidx].push_back( VectorDef( fid));
	}
	else
	{
		m_vecar[ tidx].push_back( VectorDef( vec, m_storage->model().simHash( vec, fid), fid));
	}
	m_featTypeRelations.insert( FeatureTypeRelation( fid, tid));
	if (m_errorhnd->hasError()) throw std::runtime_error( m_errorhnd->fetchError());
}

void VectorStorageTransaction::defineFeatureType( const std::string& type)
{
	try
	{
		defineType( type);
		if (m_debugtrace)
		{
			m_debugtrace->event( "define", "feature type %s", type.c_str());
		}
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error defining feature type in '%s': %s"), MODULENAME, *m_errorhnd);
}

void VectorStorageTransaction::defineFeature( const std::string& type, const std::string& name)
{
	try
	{
		defineElement( type, name, WordVector());
		if (m_debugtrace)
		{
			m_debugtrace->event( "define", "feature type %s name '%s'", type.c_str(), name.c_str());
		}
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error defining feature in '%s': %s"), MODULENAME, *m_errorhnd);
}

void VectorStorageTransaction::defineVector( const std::string& type, const std::string& name, const WordVector& vec)
{
	try
	{
		defineElement( type, name, vec);
		if (m_debugtrace)
		{
			std::string vecstr = vec.tostring( ", ", 10);
			m_debugtrace->event( "define", "feature type %s name '%s' vector %s", type.c_str(), name.c_str(), vecstr.c_str());
		}
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error defining feature vector in '%s': %s"), MODULENAME, *m_errorhnd);
}

void VectorStorageTransaction::clear()
{
	try
	{
		m_transaction->clear();
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error clearing data to '%s': %s"), MODULENAME, *m_errorhnd);
}

void VectorStorageTransaction::reset()
{
	m_vecar.clear();
	m_nametab.clear();
	m_typetab.clear();
	m_featTypeRelations.clear();
}

bool VectorStorageTransaction::commit()
{
	try
	{
		VectorStorageClient::TransactionLock lock( m_storage);
		//... we need a lock because transactions need to be sequentialized

		Index noftypeno = m_database->readNofTypeno();
		Index noffeatno = m_database->readNofFeatno();
		std::set<Index> newtypes;
		std::vector<std::string> typestrings;
		std::vector<int> types;
		{
			int ti=1, te=m_typetab.size();
			for (; ti <= te; ++ti)
			{
				const char* typestr = m_typetab.key( ti);
				Index typeno = m_database->readTypeno( typestr);
				if (!typeno)
				{
					typeno = ++noftypeno;
					m_transaction->writeType( typestr, typeno);
					newtypes.insert( typeno);
				}
				types.push_back( typeno);
				typestrings.push_back( typestr);
			}
		}
		if (types.size() != m_vecar.size()) throw std::runtime_error(_TXT("logic error in vector transaction: array sizes do not match"));

		std::vector<int> features;
		{
			int ni=1, ne=m_nametab.size();
			for (; ni <= ne; ++ni)
			{
				const char* featstr = m_nametab.key( ni);
				Index featno = m_database->readFeatno( featstr);
				if (!featno)
				{
					featno = ++noffeatno;
					m_transaction->writeFeature( featstr, featno);
				}
				features.push_back( featno);
			}
		}
		m_transaction->writeNofTypeno( noftypeno);
		m_transaction->writeNofFeatno( noffeatno);

		std::vector<int>::const_iterator ti = types.begin(), te = types.end();
		std::vector<std::vector<VectorDef> >::iterator vvi = m_vecar.begin(), vve = m_vecar.end();
		for (; ti != te && vvi != vve; ++vvi,++ti)
		{
			const Index typeno = *ti;
			Index nofvec = newtypes.find( typeno) == newtypes.end() ? m_database->readNofVectors( typeno) : 0;
			std::set<Index> featset;
			std::vector<VectorDef>& var = *vvi;
			std::vector<VectorDef>::iterator vi = var.begin(), ve = var.end();
			for (; vi != ve; ++vi)
			{
				Index featno = features[ vi->id()-1];
				vi->setId( featno);
				if (!vi->vec().empty())
				{
					if (m_database->readVector( typeno, featno).empty())
					{
						// ... is new vector
						featset.insert( featno);
					}
					m_transaction->writeVector( typeno, featno, vi->vec());
					m_transaction->writeSimHash( typeno, featno, vi->lsh());
				}
			}
			m_transaction->writeNofVectors( typeno, nofvec + featset.size());
		}
		std::set<FeatureTypeRelation>::const_iterator ri = m_featTypeRelations.begin(), re = m_featTypeRelations.end();
		while (ri != re)
		{
			Index featnoidx = ri->featno;
			Index featno = features[ featnoidx-1];
			std::vector<Index> typenoar = m_database->readFeatureTypeRelations( featno);
			for (; ri != re && ri->featno == featnoidx; ++ri)
			{
				Index typeno = types[ ri->typeno-1];
				if (std::find( typenoar.begin(), typenoar.end(), typeno) == typenoar.end())
				{
					typenoar.push_back( typeno);
				}
			}
			m_transaction->writeFeatureTypeRelations( featno, typenoar);
		}
		m_storage->resetSimHashMapTypes( typestrings);
		if (m_transaction->commit())
		{
			if (m_debugtrace) m_debugtrace->event( "commit", "types %d features %d", noftypeno, noffeatno);
			reset();
			return true;
		}
		else
		{
			return false;
		}
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in commit of '%s' transaction: %s"), MODULENAME, *m_errorhnd, false);
}

void VectorStorageTransaction::rollback()
{
	try
	{
		m_transaction->rollback();
		if (m_debugtrace) m_debugtrace->event( "rollback", "%s", "");
		reset();
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in rollback of '%s' transaction: %s"), MODULENAME, *m_errorhnd);
}


