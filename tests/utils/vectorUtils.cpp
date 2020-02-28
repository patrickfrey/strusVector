/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test utility functions
#include "vectorUtils.hpp"

using namespace strus;
using namespace strus::test;

arma::fvec strus::test::normalizeVector( const arma::fvec& vec)
{
	arma::fvec res = vec;
	arma::fvec::const_iterator vi = vec.begin(), ve = vec.end();
	double sqlen = 0.0;
	for (; vi != ve; ++vi)
	{
		sqlen += *vi * *vi;
	}
	float normdiv = strus::Math::sqrt( sqlen);
	arma::fvec::iterator ri = res.begin(), re = res.end();
	for (; ri != re; ++ri)
	{
		*ri = *ri / normdiv;
	}
	return res;
}

arma::fvec strus::test::randomVector( strus::PseudoRandom& random, int dim)
{
	strus::WordVector res;
	int di = 0, de = dim;
	for (; di != de; ++di)
	{
		float val = (1.0 * (float)random.get( 0, 10000)) / 10000.0;
		res.push_back( val);
	}
	return arma::fvec( res);
}

strus::WordVector strus::test::convertVectorStd( const arma::fvec& vec)
{
	strus::WordVector rt;
	arma::fvec::const_iterator vi = vec.begin(), ve = vec.end();
	for (; vi != ve; ++vi)
	{
		rt.push_back( *vi);
	}
	return rt;
}

bool strus::test::isSimilarVector( const strus::WordVector& vec1, const strus::WordVector& vec2, float cosSim)
{
	return cosSim <= arma::norm_dot( arma::fvec( vec1), arma::fvec( vec2));
}

strus::WordVector strus::test::createSimilarVector( strus::PseudoRandom& random, const strus::WordVector& vec_, double maxCosSim)
{
	arma::fvec vec( vec_);
	arma::fvec orig( vec);
	for (;;)
	{
		struct Change
		{
			int idx;
			float oldvalue;
		};
		enum {NofChanges=16};
		Change changes[ NofChanges];
		int ci;

		for (ci=0; ci < NofChanges; ++ci)
		{
			int idx = random.get( 0, vec.size());
			float elem = vec[ idx];
			if (random.get( 0, 2) == 0)
			{
				elem -= elem / 10;
				if (elem < 0.0) continue;
			}
			else
			{
				elem += elem / 10;
			}
			changes[ ci].idx = idx;
			changes[ ci].oldvalue = vec[ idx];
			vec[ idx] = elem;
		}
		float cosSim = arma::norm_dot( vec, orig);
		if (maxCosSim > cosSim)
		{
			for (ci=0; ci < NofChanges; ++ci)
			{
				vec[ changes[ ci].idx] = changes[ ci].oldvalue;
			}
			break;
		}
		vec = normalizeVector( vec);
	}
	return convertVectorStd( vec);
}

strus::WordVector strus::test::createRandomVector( strus::PseudoRandom& random, unsigned int dim)
{
	return convertVectorStd( normalizeVector( randomVector( random, dim))); // vector values between 0.0 and 1.0, with normalized vector length 1.0
}

bool strus::test::compareVector( const strus::WordVector& v1, const strus::WordVector& v2, double epsilon)
{
	strus::WordVector::const_iterator vi1 = v1.begin(), ve1 = v1.end();
	strus::WordVector::const_iterator vi2 = v2.begin(), ve2 = v2.end();
	for (unsigned int vidx=0; vi1 != ve1 && vi2 != ve2; ++vi1,++vi2,++vidx)
	{
		float diff = (*vi1 > *vi2)?(*vi1 - *vi2):(*vi2 - *vi1);
		if (diff > epsilon)
		{
			return false;
		}
	}
	if (vi1 == ve1 && vi2 == ve2)
	{
		return true;
	}
	else
	{
		return false;
	}
}

std::vector<strus::WordVector> strus::test::createDistinctRandomVectors( strus::PseudoRandom& random, int dim, int nof, double dist)
{
	std::vector<strus::WordVector> rt;
	while ((int)rt.size() < nof)
	{
		int ni = rt.size(), ne = nof * 2;
		for (; ni != ne; ++ni)
		{
			rt.push_back( strus::test::createRandomVector( random, dim));
		}
		std::vector<int> deleteAr;
		std::vector<strus::WordVector>::iterator ri = rt.begin(), re = rt.end();
		for (; ri != re; ++ri)
		{
			std::vector<strus::WordVector>::iterator rn = ri;
			for (++rn; rn != re; ++rn)
			{
				if (strus::test::isSimilarVector( *ri, *rn, dist))
				{
					deleteAr.push_back( rn-rt.begin());
				}
			}
		}
		std::sort( deleteAr.begin(), deleteAr.end(), std::greater<int>());
		std::vector<int>::iterator di = deleteAr.begin(), de = deleteAr.end();
		for (; di != de; ++di)
		{
			rt.erase( rt.begin() + *di);
		}
	}
	rt.resize( nof);
	return rt;
}


