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
	:m_vecdim(0),m_bits(0),m_variations(0),m_modelMatrix(),m_rotations()
{}

LshModel::LshModel( const LshModel& o)
	:m_vecdim(o.m_vecdim),m_bits(o.m_bits),m_variations(o.m_variations),m_modelMatrix(o.m_modelMatrix),m_rotations(o.m_rotations)
{}

LshModel::LshModel( int vecdim_, int bits_, int variations_)
	:m_vecdim(vecdim_),m_bits(bits_),m_variations(variations_),m_modelMatrix( createModelMatrix( vecdim_, bits_)),m_rotations()
{
	int wi=0, we=variations_;
	for (; wi != we; ++wi)
	{
		arma::fmat rot( (arma::randu<arma::fmat>( m_vecdim, m_vecdim) -0.5) * 2.0);
		if ((int)arma::rank( rot, std::numeric_limits<float>::epsilon()) < m_vecdim)
		{
			--wi;
			continue;
		}
		m_rotations.push_back( rot);
	}
}

LshModel::LshModel( int vecdim_, int bits_, int variations_, const arma::fmat& modelMatrix_, const std::vector<arma::fmat>& rotations_)
	:m_vecdim(vecdim_),m_bits(bits_),m_variations(variations_),m_modelMatrix(modelMatrix_),m_rotations(rotations_)
{
	std::vector<arma::fmat>::const_iterator ri=m_rotations.begin(), re=m_rotations.end();
	for (; ri != re; ++ri)
	{
		if ((int)arma::rank( *ri, std::numeric_limits<float>::epsilon()) < m_vecdim)
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
	if (m_vecdim != o.m_vecdim) return false;
	if (m_bits != o.m_bits) return false;
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

arma::fmat LshModel::createModelMatrix( int vecdim_, int bits_)
{
	if (vecdim_ <= 0 || bits_ <= 0)
	{
		throw std::runtime_error( "illegal dimension or number of bits");
	}
	if (vecdim_ < bits_)
	{
		throw std::runtime_error( "dimension must be at least two times bigger than the number of bits");
	}
	double step = (float) vecdim_ / (float) bits_;
	arma::fmat rt = arma::fmat( bits_, vecdim_);
	int ri = 0, re = bits_;
	for (; ri != re; ++ri)
	{
		int ci = (ri * step);
		int ce = ((ri+1) * step);
		if (ce > vecdim_) ce = vecdim_;
		if ((ri+1) == re) ce = vecdim_;
		rt.row( ri).fill( -1.0 / (vecdim_ - (ce - ci)));
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
	rt << "dim=" << m_vecdim << ", bits=" << m_bits << ", variations=" << m_variations << std::endl;
	std::vector<arma::fmat>::const_iterator roti = m_rotations.begin(), rote = m_rotations.end();
	for (; roti != rote; ++roti)
	{
		rt << *roti << std::endl;
	}
	rt << m_modelMatrix << std::endl;
	return rt.str();
}

SimHash LshModel::simHash( const arma::fvec& vec, const Index& id_) const
{
	std::vector<bool> rt;
	if (m_vecdim != (int)vec.size())
	{
		throw strus::runtime_error( _TXT("vector must have dimension of model: dim=%d != vector=%d"), m_vecdim, (int)vec.size());
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
	return SimHash( rt, id_);
}

union PackedFloat
{
	float float_;
	uint32_t u32_;
};

struct DumpStructHeader
{
	DumpStructHeader()
		:vecdim(0),bits(0),variations(0){}
	DumpStructHeader( int vecdim_, int bits_, int variations_)
		:vecdim(vecdim_),bits(bits_),variations(variations_){}
	DumpStructHeader( const DumpStructHeader& o)
		:vecdim(o.vecdim),bits(o.bits),variations(o.variations){}

	uint32_t vecdim;
	uint32_t bits;
	uint32_t variations;
	enum {NofElements=3};

	void conv_hton()
	{
		vecdim = ByteOrder<uint32_t>::hton(vecdim);
		bits = ByteOrder<uint32_t>::hton(bits);
		variations = ByteOrder<uint32_t>::hton(variations);
	}

	void conv_ntoh()
	{
		vecdim = ByteOrder<uint32_t>::ntoh(vecdim);
		bits = ByteOrder<uint32_t>::ntoh(bits);
		variations = ByteOrder<uint32_t>::ntoh(variations);
	}

	void printSerialization( std::string& buf)
	{
		uint32_t ar[ NofElements];
		ar[0] = ByteOrder<uint32_t>::hton(vecdim);
		ar[1] = ByteOrder<uint32_t>::hton(bits);
		ar[2] = ByteOrder<uint32_t>::hton(variations);
		buf.append( (char*)(void*)ar, NofElements*sizeof(uint32_t));
	}
	void initFromSerialization( const char*& ser)
	{
		vecdim = ByteOrder<uint32_t>::ntoh(*(uint32_t*)(void*)ser); ser += sizeof(uint32_t);
		bits = ByteOrder<uint32_t>::ntoh(*(uint32_t*)(void*)ser); ser += sizeof(uint32_t);
		variations = ByteOrder<uint32_t>::ntoh(*(uint32_t*)(void*)ser); ser += sizeof(uint32_t);
	}
};

struct DumpStruct
	:public DumpStructHeader
{
	DumpStruct( int vecdim_, int bits_, int variations_)
		:DumpStructHeader(vecdim_,bits_,variations_),ar(0),arsize(0)
	{
		std::size_t nofFloats = (vecdim * bits) + (vecdim * vecdim * variations);
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
		std::size_t nofFloats = (vecdim * bits) + (vecdim * vecdim * variations);
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
	DumpStruct st( m_vecdim, m_bits, m_variations);

	std::size_t aidx = 0;
	std::vector<arma::fmat>::const_iterator roti = m_rotations.begin(), rote = m_rotations.end();
	for (; roti != rote; ++roti)
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

	DumpStruct st( hdr.vecdim, hdr.bits, hdr.variations);
	if (st.contentAllocSize() > (blobsize - (src - blob)))
	{
		throw std::runtime_error( _TXT("LSH model dump is corrupt (dump too small)"));
	}
	st.initValuesFromSerialization( src);
	std::size_t ai=0, ae=st.nofValues();

	arma::fmat modelMatrix( hdr.bits, hdr.vecdim);
	std::vector<arma::fmat> rotations;

	std::size_t ri=0, re=hdr.variations;
	for (; ri != re; ++ri)
	{
		arma::fmat rot( hdr.vecdim, hdr.vecdim);
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
	return LshModel( hdr.vecdim, hdr.bits, hdr.variations, modelMatrix, rotations);
}

LshModel LshModel::fromSerialization( const std::string& dump)
{
	return fromSerialization( dump.c_str(), dump.size());
}



