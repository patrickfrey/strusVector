/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some local helper functions in the context of the armadillo library
#include "armautils.hpp"
#include "strus/base/math.hpp"

using namespace strus;


arma::fvec strus::normalizeVector( const strus::WordVector& vec)
{
	arma::fvec res( vec);
	WordVector::const_iterator vi = vec.begin(), ve = vec.end();
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

arma::fvec strus::normalizeVector( const arma::fvec& vec)
{
	arma::fvec res( vec);
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

