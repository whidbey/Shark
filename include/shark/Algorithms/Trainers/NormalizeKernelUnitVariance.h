//===========================================================================
/*!
 *  \file NormalizeKernelTrace.h
 *
 *  \brief Determine the scaling factor of a ScaledKernel so that it has unit variance in feature space one on a given dataset.
 *
 *
 *  \author M. Tuma
 *  \date 2012
 *
 *  \par Copyright (c) 1998-2012:
 *      Institut f&uuml;r Neuroinformatik<BR>
 *      Ruhr-Universit&auml;t Bochum<BR>
 *      D-44780 Bochum, Germany<BR>
 *      Phone: +49-234-32-25558<BR>
 *      Fax:   +49-234-32-14209<BR>
 *      eMail: Shark-admin@neuroinformatik.ruhr-uni-bochum.de<BR>
 *      www:   http://www.neuroinformatik.ruhr-uni-bochum.de<BR>
 *      <BR>
 *
 *  <BR><HR>
 *  This file is part of Shark. This library is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, see <http://www.gnu.org/licenses/>.
 *  
 */
//===========================================================================


#ifndef SHARK_ALGORITHMS_TRAINERS_NORMALIZEKERNELUNITVARIANCE_H
#define SHARK_ALGORITHMS_TRAINERS_NORMALIZEKERNELUNITVARIANCE_H


#include <shark/Models/Kernels/ScaledKernel.h>
#include <shark/Algorithms/Trainers/AbstractTrainer.h>

namespace shark {


///
/// \brief Determine the scaling factor of a ScaledKernel so that it has unit variance in feature space one on a given dataset.
///
/// \par
/// For example in the multiple kernel learning setting, it can be important that the sub-kernels
/// are normalized to unit variance in feature space. This class computes both the trace and the
/// mean of a kernel matrix, and uses both to employ the "Multiplicative Kernel Scaling" laid out
/// in "Kloft, Brefeld, Sonnenburg, Zien: l_p-Norm Multiple Kernel Learning. JMLR 12, 2011.".
/// Given a ScaledKernel, which itself holds an arbitrary underlying kernel k, we compute
/// \f[ \frac{1}{N}\sum_{i=1}^N k(x_i,x_i) - \frac{1}{N^2} \sum_{i,j=1}^N k(x_i,x_j) \f]
/// 
/// 
/// 
template < class InputType = RealVector >
class NormalizeKernelUnitVariance : public AbstractUnsupervisedTrainer<ScaledKernel<InputType> >
{
public:

	NormalizeKernelUnitVariance()
	{ this->m_name = "NormalizeKernelUnitVariance"; }
	
	double trace() const {
		return m_trace;
	}
	double mean() const {
		return m_mean;
	}

	void train( ScaledKernel<InputType>& kernel, UnlabeledData<InputType> const& input )
	{
		SHARK_CHECK(input.numberOfElements() >= 2, "[NormalizeKernelUnitVariance::train] input needs to contain at least two points");
		double mean = 0.0;
		double trace = 0.0;
		AbstractKernelFunction< InputType > const* main = kernel.base(); //get direct access to the kernel we want to use.		
		
		// Next compute the trace and mean of the kernel matrix. This means heavy lifting: 
		// calculate each entry of one diagonal half of the kernel matrix exactly once.
		// [MT] This part was taken from the PrecomputedMatrix constructor in QuadraticProgram.h
		// Blockwise version, with order of computations optimized for better use of the processor
		// cache. This can save around 10% computation time for fast kernel functions.
		//TODO: O.K. use batched computation! Save up to factor X.
		std::size_t N = input.numberOfElements();
		std::size_t blocks = N / 64;
		std::size_t rest = N - 64 * blocks;
		//TODO: O.K: we are not programming C anymore!
		std::size_t i, j, ci, cj, ii, jj;

		// loop through full blocks
		for (ci=0; ci<blocks; ci++) {
			{ // diagonal blocks:
				for (i=0, ii=64*ci; i<64; i++, ii++) {
					for (j=0, jj=64*ci; j<i; j++, jj++) {
						mean += main->eval(input(ii), input(jj));
					}
					double d = main->eval(input(ii), input(ii));
					mean += 0.5 * d;
					trace += d;
				}
			} // off-diagonal blocks:
			for (cj=0; cj<ci; cj++) { 
				for (i=0, ii=64*ci; i<64; i++, ii++) {
					for (j=0, jj=64*cj; j<64; j++, jj++) {
						mean += main->eval(input(ii), input(jj));
					}
				}
			}
		}
		if (rest > 0) {
			// loop through the margins
			for (cj=0; cj<blocks; cj++) {
				for (j=0, jj=64*cj; j<64; j++, jj++) {
					for (i=0, ii=64*blocks; i<rest; i++, ii++) {
						mean += main->eval(input(ii), input(jj));
					}
				}
			}
			// lower right block
			for (i=0, ii=64*blocks; i<rest; i++, ii++) {
				for (j=0, jj=64*blocks; j<i; j++, jj++) {
					mean += main->eval(input(ii), input(jj));
				}
				double d = main->eval(input(ii), input(ii));
				mean += 0.5 * d;
				trace += d;
			}
		}
		mean *= 2.0; //correct for the fact that we only counted one diagonal half of the matrix
		m_mean = mean;
		m_trace = trace;
		double tm = trace/N - mean/N/N;
		SHARK_ASSERT( tm > 0 );
		double scaling_factor = 1.0 / tm;
		kernel.setFactor( scaling_factor );
	}

	/// From INameable, returns the models name
	virtual const std::string & name() const {
		return m_name;
	}
	
protected:
	double m_mean; //store for other uses, external queries, etc.
	double m_trace;

	std::string m_name;
	
};


}
#endif