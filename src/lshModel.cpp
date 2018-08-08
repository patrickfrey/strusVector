/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LSH Similarity model structure
#include "lshModel.hpp"
#include "internationalization.hpp"
#include "strus/base/stdint.h"
#include "strus/base/hton.hpp"
#include <cstdlib>
#include <limits>

using namespace strus;
#define VEC_EPSILON  (1.0E-11)


LshModel::LshModel()
	:m_dim(0),m_nofbits(0),m_variations(0)
	,m_modelMatrix(),m_rotations()
{}

LshModel::LshModel( const LshModel& o)
	:m_dim(o.m_dim),m_nofbits(o.m_nofbits),m_variations(o.m_variations)
	,m_modelMatrix(o.m_modelMatrix),m_rotations(o.m_rotations)
{}

LshModel::LshModel( std::size_t dim_, std::size_t nofbits_, std::size_t variations_)
	:m_dim(dim_),m_nofbits(nofbits_),m_variations(variations_)
	,m_modelMatrix( createModelMatrix( dim_, nofbits_)),m_rotations()
{
	std::size_t wi=0, we=variations_;
	for (; wi != we; ++wi)
	{
		arma::fmat rot( (arma::randu<arma::fmat>( m_dim, m_dim) -0.5) * 2.0);
		if (arma::rank( rot, std::numeric_limits<float>::epsilon()) < m_dim)
		{
			--wi;
			continue;
		}
		m_rotations.push_back( rot);
	}
}

LshModel::LshModel( std::size_t dim_, std::size_t nofbits_, std::size_t variations_, const arma::fmat& modelMatrix_, const std::vector<arma::fmat>& rotations_)
	:m_dim(dim_),m_nofbits(nofbits_),m_variations(variations_)
	,m_modelMatrix(modelMatrix_),m_rotations(rotations_)
{
	std::vector<arma::fmat>::const_iterator ri=m_rotations.begin(), re=m_rotations.end();
	for (; ri != re; ++ri)
	{
		if (arma::rank( *ri, std::numeric_limits<float>::epsilon()) < m_dim)
		{
			throw std::runtime_error( _TXT( "illegal rotation matrix in model"));
		}
	}
}

static bool mat_isequal( const arma::fmat& m1, const arma::fmat& m2)
{
	arma::fmat::const_iterator
		mi1 = m1.begin(), me1 = m1.end(),
		mi2 = m2.begin(), me2 = m2.end();
	for (; mi1 != me1 && mi2 != me2; ++mi1,++mi2)
	{
		double diff = *mi1 > *mi2 ? (*mi1 - *mi2):(*mi2 - *mi1);
		if (diff > VEC_EPSILON) return false;
	}
	return (mi1 == me1 && mi2 == me2);
}

bool LshModel::isequal( const LshModel& o) const
{
	if (m_dim != o.m_dim) return false;
	if (m_nofbits != o.m_nofbits) return false;
	if (m_variations != o.m_variations) return false;
	if (!mat_isequal( m_modelMatrix, o.m_modelMatrix)) return false;
	std::vector<arma::fmat>::const_iterator
		ri = m_rotations.begin(), re = m_rotations.end(),
		roi = o.m_rotations.begin(), roe = o.m_rotations.end();
	for (; ri != re && roi != roe; ++ri,++roi)
	{
		if (!mat_isequal( *ri, *roi)) return false;
	}
	return ri == re && roi == roe;
}

arma::fmat LshModel::createModelMatrix( std::size_t dim_, std::size_t nofbits_)
{
	if (dim_ <= 0 || nofbits_ <= 0)
	{
		throw std::runtime_error( "illegal dimension or nofbits");
	}
	if (dim_ < nofbits_)
	{
		throw std::runtime_error( "dimension must be at least two times bigger than nofbits");
	}
	double step = (float) dim_ / (float) nofbits_;
	arma::fmat rt = arma::fmat( nofbits_, dim_);
	std::size_t ri = 0, re = nofbits_;
	for (; ri != re; ++ri)
	{
		unsigned int ci = (unsigned int)(ri * step);
		unsigned int ce = (unsigned int)((ri+1) * step);
		if (ce > dim_) ce = dim_;
		if ((ri+1) == re) ce = dim_;
		rt.row( ri).fill( -1.0 / (dim_ - (ce - ci)));
		double val = 1.0 / (ce - ci);
		for (; ci < ce; ++ci)
		{
			rt( ri, ci) = val;
		}
	}
	return rt;
}

std::string LshModel::tostring() const
{
	std::ostringstream rt;
	rt << "d=" << m_dim << ", n=" << m_nofbits << ", v=" << m_variations << std::endl;
	std::vector<arma::fmat>::const_iterator roti = m_rotations.begin(), rote = m_rotations.end();
	for (; roti != rote; ++roti)
	{
		rt << *roti << std::endl;
	}
	rt << m_modelMatrix << std::endl;
	return rt.str();
}

SimHash LshModel::simHash( const arma::fvec& vec) const
{
	std::vector<bool> rt;
	if (m_dim != vec.size())
	{
		throw strus::runtime_error( _TXT("vector must have dimension of model: dim=%u != vector=%u"), (unsigned int)m_dim, (unsigned int)vec.size());
	}
	std::vector<arma::fmat>::const_iterator roti = m_rotations.begin(), rote = m_rotations.end();
	for (; roti != rote; ++roti)
	{
		arma::fvec res = m_modelMatrix * (*roti) * vec;
		arma::fvec::const_iterator resi = res.begin(), rese = res.end();
		for (; resi != rese; ++resi)
		{
			rt.push_back( *resi >= 0.0);
		}
	}
	return SimHash( rt);
}

union PackedFloat
{
	float float_;
	uint32_t u32_;
};

struct DumpStructHeader
{
	DumpStructHeader()
		:dim(0),nofbits(0),variations(0){}
	DumpStructHeader( std::size_t dim_, std::size_t nofbits_, std::size_t variations_)
		:dim(dim_),nofbits(nofbits_),variations(variations_){}
	DumpStructHeader( const DumpStructHeader& o)
		:dim(o.dim),nofbits(o.nofbits),variations(o.variations){}

	uint32_t dim;
	uint32_t nofbits;
	uint32_t variations;

	void conv_hton()
	{
		dim = ByteOrder<uint32_t>::hton(dim);
		nofbits = ByteOrder<uint32_t>::hton(nofbits);
		variations = ByteOrder<uint32_t>::hton(variations);
	}

	void conv_ntoh()
	{
		dim = ByteOrder<uint32_t>::ntoh(dim);
		nofbits = ByteOrder<uint32_t>::ntoh(nofbits);
		variations = ByteOrder<uint32_t>::ntoh(variations);
	}

	void printSerialization( std::string& buf)
	{
		uint32_t ar[3];
		ar[0] = ByteOrder<uint32_t>::hton(dim);
		ar[1] = ByteOrder<uint32_t>::hton(nofbits);
		ar[2] = ByteOrder<uint32_t>::hton(variations);
		buf.append( (char*)(void*)ar, 3*sizeof(uint32_t));
	}
	void initFromSerialization( const char*& ser)
	{
		dim = ByteOrder<uint32_t>::ntoh(*(uint32_t*)(void*)ser); ser += sizeof(uint32_t);
		nofbits = ByteOrder<uint32_t>::ntoh(*(uint32_t*)(void*)ser); ser += sizeof(uint32_t);
		variations = ByteOrder<uint32_t>::ntoh(*(uint32_t*)(void*)ser); ser += sizeof(uint32_t);
	}
};

struct DumpStruct
	:public DumpStructHeader
{
	DumpStruct( std::size_t dim_, std::size_t nofbits_, std::size_t variations_)
		:DumpStructHeader(dim_,nofbits_,variations_),ar(0),arsize(0)
	{
		std::size_t nofFloats = (dim * nofbits) + (dim * dim * variations);
		arsize = nofFloats;
		ar = (uint32_t*)std::malloc( arsize * sizeof(ar[0]));
		if (!ar) throw std::bad_alloc();
	}

	float getValue( std::size_t aidx)
	{
		PackedFloat rt;
		rt.u32_ = ar[ aidx];
		return rt.float_;
	}

	void setValue( std::size_t aidx, double val)
	{
		PackedFloat rt;
		rt.float_ = val;
		ar[ aidx] = rt.u32_;
	}

	~DumpStruct()
	{
		if (ar) std::free( ar);
	}

	std::size_t contentAllocSize() const
	{
		return arsize * sizeof(ar[0]);
	}

	std::size_t nofValues() const
	{
		return arsize;
	}

	void printSerialization( std::string& buf)
	{
		DumpStructHeader::printSerialization( buf);
		uint32_t ai = 0, ae = arsize;
		for (; ai != ae; ++ai)
		{
			uint32_t elem = ByteOrder<uint32_t>::hton( ar[ ai]);
			buf.append( (char*)&elem, sizeof(uint32_t));
		}
	}
	void initValuesFromSerialization( const char*& ser)
	{
		loadValues( ser);
		ser += sizeof(uint32_t) * arsize;
	}

private:
	const void* getValuePtr() const
	{
		return (const void*)ar;
	}
	std::size_t getValuePtrSize() const
	{
		std::size_t nofFloats = (dim * nofbits) + (dim * dim * variations);
		if (arsize != nofFloats * 2) throw std::runtime_error( _TXT( "LSH model structure is corrupt"));
		return arsize * sizeof(ar[0]);
	}

	void loadValues( const void* ar_)
	{
		const uint32_t* ua = (const uint32_t*)ar_;
		std::size_t ai=0, ae=arsize;
		for (; ai != ae; ++ai)
		{
			ar[ ai] = ByteOrder<uint32_t>::ntoh( ua[ ai]);
		}
	}

	void conv_hton()
	{
		std::size_t ai=0, ae=arsize;
		DumpStructHeader::conv_hton();
		for (; ai != ae; ++ai)
		{
			ar[ ai] = ByteOrder<uint32_t>::hton( ar[ ai]);
		}
	}

	void conv_ntoh()
	{
		DumpStructHeader::conv_ntoh();
		std::size_t ai=0, ae=arsize;
		for (; ai != ae; ++ai)
		{
			ar[ ai] = ByteOrder<uint32_t>::ntoh( ar[ ai]);
		}
	}

private:
	uint32_t* ar;
	uint32_t arsize;

private:
	DumpStruct( const DumpStruct&){} //non copyable, because we do not know LE of BE conversion state
};


std::string LshModel::serialization() const
{
	std::string rt;
	DumpStruct st( m_dim, m_nofbits, m_variations);

	std::size_t aidx = 0;
	std::vector<arma::fmat>::const_iterator roti = m_rotations.begin(), rote = m_rotations.end();
	for (unsigned int ridx=0; roti != rote; ++roti,++ridx)
	{
		arma::fmat::const_iterator ri = roti->begin(), re = roti->end();
		for (; ri != re; ++ri)
		{
			st.setValue( aidx++, *ri);
		}
	}
	arma::fmat::const_iterator mi = m_modelMatrix.begin(), me = m_modelMatrix.end();
	for (; mi != me; ++mi)
	{
		st.setValue( aidx++, *mi);
	}
	st.printSerialization( rt);
	return rt;
}

LshModel LshModel::fromSerialization( const char* blob, std::size_t blobsize)
{
	DumpStructHeader hdr;
	char const* src = blob;
	if (blobsize < sizeof( DumpStructHeader)) throw std::runtime_error( _TXT("lsh model dump is corrupt (dump header too small)"));
	hdr.initFromSerialization( src);

	DumpStruct st( hdr.dim, hdr.nofbits, hdr.variations);
	if (st.contentAllocSize() > (blobsize - (src - blob)))
	{
		throw std::runtime_error( _TXT("LSH model dump is corrupt (dump too small)"));
	}
	st.initValuesFromSerialization( src);
	std::size_t ai=0, ae=st.nofValues();

	arma::fmat modelMatrix( hdr.nofbits, hdr.dim);
	std::vector<arma::fmat> rotations;

	std::size_t ri=0, re=hdr.variations;
	for (; ri != re; ++ri)
	{
		arma::fmat rot( hdr.dim, hdr.dim);
		arma::fmat::iterator mi = rot.begin(), me=rot.end();
		for (; mi != me; ++mi,++ai)
		{
			*mi = st.getValue( ai);
		}
		rotations.push_back( rot);
	}

	arma::fmat::iterator mi = modelMatrix.begin(), me=modelMatrix.end();
	for (; mi != me; ++mi,++ai)
	{
		*mi = st.getValue( ai);
	}
	if (ai != ae)
	{
		throw std::runtime_error( _TXT( "lsh model dump is corrupt"));
	}
	return LshModel( hdr.dim, hdr.nofbits, hdr.variations, modelMatrix, rotations);
}

LshModel LshModel::fromSerialization( const std::string& dump)
{
	return fromSerialization( dump.c_str(), dump.size());
}



