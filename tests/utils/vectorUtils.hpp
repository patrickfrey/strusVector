/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _STRUS_VECTOR_TEST_VECTOR_UTILS
#define _STRUS_VECTOR_TEST_VECTOR_UTILS
/// \brief Test utility functions
#include "strus/lib/vector_std.hpp"
#include "strus/storage/wordVector.hpp"
#include "strus/base/stdint.h"
#include "strus/base/pseudoRandom.hpp"
#include "strus/base/math.hpp"
#include "armadillo"
#include <vector>

namespace strus {
namespace test {

arma::fvec normalizeVector( const arma::fvec& vec);
arma::fvec randomVector( strus::PseudoRandom& random, int dim);
strus::WordVector convertVectorStd( const arma::fvec& vec);
bool isSimilarVector( const strus::WordVector& vec1, const strus::WordVector& vec2, float cosSim);
strus::WordVector createSimilarVector( strus::PseudoRandom& random, const strus::WordVector& vec_, double maxCosSim);
strus::WordVector createRandomVector( strus::PseudoRandom& random, unsigned int dim);
bool compareVector( const strus::WordVector& v1, const strus::WordVector& v2, double epsilon);
std::vector<strus::WordVector> createDistinctRandomVectors( strus::PseudoRandom& random, int dim, int nof, double dist);

}}//namespace
#endif


