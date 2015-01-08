/*
For more information, please see: http://software.sci.utah.edu

The MIT License

Copyright (c) 2012 Scientific Computing and Imaging Institute,
University of Utah.


Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

//    File       : SolveInverseProblemWithTikhonov.cc
//    Author     : Moritz Dannhauer, Ayla Khan, Dan White
//    Date       : November 02th, 2012 (last update)

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <Modules/Legacy/Inverse/SolveInverseProblemWithTikhonovImpl.h>

#include <Core/Datatypes/Matrix.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <Core/Datatypes/DenseColumnMatrix.h>
#include <Core/Datatypes/SparseRowMatrix.h>
#include <Core/Datatypes/MatrixTypeConversions.h>

#include <Core/Logging/LoggerInterface.h>
#include <Core/Utils/Exception.h>

//#include <Core/Exceptions/Exception.h>
//#include <Core/Exceptions/InvalidState.h>
//#include <Core/Exceptions/DimensionMismatch.h>
//#include <Core/Exceptions/LapackError.h>

namespace BioPSE
{

using namespace SCIRun;
using namespace SCIRun::Core;
using namespace SCIRun::Core::Datatypes;
using namespace SCIRun::Core::Logging;

TikhonovAlgorithmImpl::TikhonovAlgorithmImpl(const DenseMatrixHandle& forwardMatrix,
                                             const DenseMatrixHandle& measuredData,
                                             AlgorithmChoice regularizationChoice,
                                             AlgorithmSolutionSubcase regularizationSolutionSubcase,
                                             AlgorithmResidualSubcase regularizationResidualSubcase,
                                             const DenseMatrixHandle sourceWeighting,
                                             const DenseMatrixHandle sensorWeighting,
                                             bool computeRegularizedInverse,
                                             LegacyLoggerInterface* pr)
: forwardMatrix_(forwardMatrix),
measuredData_(measuredData),
sourceWeighting_(sourceWeighting),
sensorWeighting_(sensorWeighting),
regularizationChoice_(regularizationChoice),
regularizationSolutionSubcase_(regularizationSolutionSubcase),
regularizationResidualSubcase_(regularizationResidualSubcase),
lambda_(0),
computeRegularizedInverse_(computeRegularizedInverse),
pr_(pr)
{
  //TODO: size checking here.
}

MatrixHandle TikhonovAlgorithmImpl::get_inverse_solution() const
{
  return inverseSolution_;
}

MatrixHandle TikhonovAlgorithmImpl::get_inverse_matrix() const
{
  return inverseMatrix_;
}

DenseColumnMatrixHandle TikhonovAlgorithmImpl::get_regularization_parameter() const
{
  return regularizationParameter_;
}

#if 0
class SolveInverseProblemWithTikhonov : public Module
{
  GuiDouble     lambda_from_textentry_;
  GuiInt        have_ui_;
  GuiString     reg_method_;
  GuiDouble     lambda_min_;
  GuiDouble     lambda_max_;
  GuiInt        lambda_num_;
  GuiDouble     lambda_resolution_;
  GuiInt        tik_cases_;
  GuiInt        tik_solution_subcases_;
  GuiInt        tik_residual_subcases_;
  GuiDouble     lambda_from_scale_;
  GuiDouble     log_val_;
  GuiDouble     lambda_corner_from_scale_;
  boost::shared_ptr<TikhonovAlgorithmImpl> algo_handle_;
  boost::shared_ptr<TikhonovAlgorithmImpl::Input> input_handle_;

public:
  //! Constructor
  SolveInverseProblemWithTikhonov(GuiContext *context);
  virtual void execute();
  virtual void tcl_command(GuiArgs& args, void* userdata);
  void update_lcurve_gui(const double lambda, const TikhonovAlgorithmImpl::LCurveInput& input, const int lambda_index);
};

//! Constructor
SolveInverseProblemWithTikhonov::SolveInverseProblemWithTikhonov(GuiContext *context) :
Module("SolveInverseProblemWithTikhonov", context, Source, "Inverse", "BioPSE"),
lambda_from_textentry_(context->subVar("lambda_from_textentry"), 0.02),
have_ui_(context->subVar("have_ui"), 0),
reg_method_(context->subVar("reg_method"), "lcurve"),
lambda_min_(context->subVar("lambda_min"), 1.0e-2),
lambda_max_(context->subVar("lambda_max"), 1.0e+6),
lambda_num_(context->subVar("lambda_num"), 10),
lambda_resolution_(context->subVar("lambda_resolution"), 1.0e-6),
tik_cases_(context->subVar("tik_cases"), 0),
tik_solution_subcases_(context->subVar("tik_solution_subcases"), 0),
tik_residual_subcases_(context->subVar("tik_residual_subcases"), 0),
lambda_from_scale_(context->subVar("lambda_from_scale"), 0),
log_val_(context->subVar("log_val"), 0.02),
lambda_corner_from_scale_(context->subVar("lambda_corner_from_scale", false), 0)
{
}
#endif

//! Find Corner, find the maximal curvature which corresponds to the L-curve corner
//! changed by Moritz Dannhauer
double
TikhonovAlgorithmImpl::FindCorner(const LCurveInput& input, int& lambda_index)
{
  const std::vector<double>& rho = input.rho_;
  const std::vector<double>& eta = input.eta_;
  const std::vector<double>& lambdaArray = input.lambdaArray_;
  int nLambda = input.nLambda_;

  std::vector<double> deta(nLambda);
  std::vector<double> ddeta(nLambda);
  std::vector<double> drho(nLambda);
  std::vector<double> ddrho(nLambda);
  std::vector<double> lrho(nLambda);
  std::vector<double> leta(nLambda);
  DenseColumnMatrix kapa(nLambda);

  double maxKapa = -1.0e10;
  for (int i = 0; i < nLambda; i++)
  {
    lrho[i] = std::log10(rho[i]);
    leta[i] = std::log10(eta[i]);
    if(i>0)
    {
      deta[i] = (leta[i]-leta[i-1]) / (lambdaArray[i]-lambdaArray[i-1]); // compute first derivative
      drho[i] = (lrho[i]-lrho[i-1]) / (lambdaArray[i]-lambdaArray[i-1]);
    }
    if(i>1)
    {
      ddeta[i] = (deta[i]-deta[i-1]) / (lambdaArray[i]-lambdaArray[i-1]); // compute second derivative from first
      ddrho[i] = (drho[i]-drho[i-1]) / (lambdaArray[i]-lambdaArray[i-1]);
    }
  }
  drho[0] = drho[1];
  deta[0] = deta[1];
  ddrho[0] = ddrho[2];
  ddrho[1] = ddrho[2];
  ddeta[0] = ddeta[2];
  ddeta[1] = ddeta[2];

  lambda_index = 0;
  for (int i = 0; i < nLambda; i++)
  {
    kapa[i] = std::abs((drho[i] * ddeta[i] - ddrho[i] * deta[i]) /  //compute curvature
                       std::sqrt(std::pow((deta[i]*deta[i]+drho[i]*drho[i]), 3.0)));
    if (kapa[i] > maxKapa) // find max curvature
    {
      maxKapa = kapa[i];
      lambda_index = i;
    }
  }

  return lambdaArray[lambda_index];
}

double
TikhonovAlgorithmImpl::LambdaLookup(const LCurveInput& input, double lambda, int& lambda_index, const double epsilon)
{
  const std::vector<double>& lambdaArray = input.lambdaArray_;
  int nLambda = input.nLambda_;

  for (int i = 0; i < nLambda-1; ++i)
  {
    if (i > 0 && (lambda < lambdaArray[i-1] || lambda > lambdaArray[i+1])) continue;

    double lambda_step_midpoint = std::abs(lambdaArray[i+1] - lambdaArray[i])/2;

    if (std::abs(lambda - lambdaArray[i]) <= epsilon)  // TODO: is this a reasonable comparison???
    {
      lambda_index = i;
      return lambdaArray[lambda_index];
    }

    if (std::abs(lambda - lambdaArray[i]) < lambda_step_midpoint)
    {
      lambda_index = i;
      return lambdaArray[lambda_index];
    }

    if (std::abs(lambda - lambdaArray[i+1]) < lambda_step_midpoint)
    {
      lambda_index = i+1;
      return lambdaArray[lambda_index];
    }
  }
  return -1;
}

namespace LinearAlgebra
{
  void solve_lapack(const DenseMatrix&, const DenseColumnMatrix&, DenseMatrix&)
  {
    //TODO obviously
  }

  class LapackError : public std::exception {};
}

void TikhonovAlgorithmImpl::run(const TikhonovAlgorithmImpl::Input& input)
{
  // TODO: use DimensionMismatch exception where appropriate
  // DIMENSION CHECK!!
  const int M = forwardMatrix_->nrows();
  const int N = forwardMatrix_->ncols();
  if (M != measuredData_->nrows())
  {
    BOOST_THROW_EXCEPTION(DimensionMismatch() << DimensionMismatchInfo("Input matrix dimensions must agree."));
  }
  if (1 != measuredData_->ncols())
  {
    BOOST_THROW_EXCEPTION(DimensionMismatch() << DimensionMismatchInfo("Measured data must be a vector"));
  }
  //decide used Tikhonov regularization formulation (underdetermined or overdetermined)
  //based purely on relationship of number of sensors compared to number of source reconstruction points
  //UNDERDETERMINED CASE
  if ( ((M < N) && (regularizationChoice_ == automatic)) || (regularizationChoice_ == underdetermined))
  {
    //.........................................................................
    // OPERATE ON DATA:
    // Compute X = R * R^T * A^T (A * R * R^T * A^T + LAMBDA * LAMBDA * C * C^T) * Y
    //.........................................................................
    DenseMatrix ARRtrAtr, RRtrAtr;
    double lambda=0, lambda_sq=0;
    DenseMatrix forward_transpose = forwardMatrix_->transpose();
    DenseMatrix regMat;
    #ifdef SCIRUN4_CODE_TO_BE_CONVERTED_LATER
    SparseRowMatrixHandle sourceWeighting_sparse;
    #endif
    DenseColumnMatrixHandle solution_handle(new DenseColumnMatrix(M));
    DenseColumnMatrix &solution = *solution_handle;
    DenseColumnMatrixHandle RRtrAtrsolution(new DenseColumnMatrix(N));
    DenseColumnMatrix &RRtrAtrsolution_handle = *RRtrAtrsolution;

    const DenseMatrixHandle measuredDataRef = measuredData_;
    if (!sourceWeighting_) //check if a Source Space Weighting Matrix exist?
    {
      regMat = DenseMatrix::Identity(N, N);
      ARRtrAtr = *forwardMatrix_ * forward_transpose;
      RRtrAtr = forward_transpose;
    }
    else
    {
      if(((regularizationSolutionSubcase_==solution_constrained) && ((N != sourceWeighting_->nrows()) || (N != sourceWeighting_->ncols()))) ||
         ((regularizationSolutionSubcase_==solution_constrained_squared) && (N != sourceWeighting_->nrows())))
      {
        BOOST_THROW_EXCEPTION(DimensionMismatch() << DimensionMismatchInfo("Solution Weighting Matrix (number of rows and columns) must fit matrix dimensions of Forward Matrix (number of columns) !"));
      }
      regMat = *sourceWeighting_;
      #ifdef SCIRUN4_CODE_TO_BE_CONVERTED_LATER
      sourceWeighting_sparse = sourceWeighting_->sparse();
      #endif

      if (regularizationSolutionSubcase_== solution_constrained)
      {
        auto RRtrAtr = *sourceWeighting_ * forward_transpose;
	      auto AR = forwardMatrix_;
	      ARRtrAtr = *AR * RRtrAtr;
      }
      else if (regularizationSolutionSubcase_== solution_constrained_squared)
      {
        auto AR = *forwardMatrix_ * *sourceWeighting_;
	      auto RtrAtr = AR.transpose();
        auto RRtrAtr = *sourceWeighting_ * RtrAtr;
	      ARRtrAtr = AR * RtrAtr;
      }
    }
    DenseMatrix CCtr;
    if (!sensorWeighting_) //check if a Sensor Space (Noise Covariance) Weighting Matrix exist?
    {
      CCtr = DenseMatrix::Identity(M, M);
    }
    else
    {
      if (((regularizationResidualSubcase_ == residual_constrained) && ((M != sensorWeighting_->nrows()) || (M != sensorWeighting_->ncols()))) ||
          ((regularizationResidualSubcase_ == residual_constrained_squared) && (M != sensorWeighting_->nrows())))
      {
        BOOST_THROW_EXCEPTION(DimensionMismatch() << DimensionMismatchInfo("Data Residual Weighting Matrix (number of rows and columns) must fit matrix dimensions of Forward Matrix (number of rows) !"));
      }
      else
      {
        if (regularizationResidualSubcase_ == residual_constrained)
          CCtr = *sensorWeighting_;
        else
          if ( regularizationResidualSubcase_ == residual_constrained_squared )
            CCtr = *sensorWeighting_ * sensorWeighting_->transpose();
      }
    }
    //DenseMatrix &tmpd1 = ARRtrAtr;
    //DenseMatrix ARRtrAtr_h=tmpd1;
    //DenseMatrix &tmpd2 = CCtr;
    //DenseMatrix CCtr_h=tmpd2;

    DenseMatrixHandle regForMatrix_handle(new DenseMatrix(M, M));
    DenseMatrix & regForMatrix = *regForMatrix_handle;


    //Get Regularization parameter(s) : Lambda
    if ((input.regMethod_ == "single") || (input.regMethod_ == "slider"))
    {
      if (input.regMethod_ == "single")
      {
        // Use single fixed lambda value, entered in UI
        lambda = input.lambdaFromTextEntry_;
      }
      else if (input.regMethod_ == "slider")
      {
        // Use single fixed lambda value, select via slider
        lambda = input.lambdaSlider_;
      }
    }
    else if (input.regMethod_ == "lcurve")
    {
      const int nLambda = input.lambdaCount_;  //declare needed variables

      std::vector<double> lambdaArray(nLambda, 0.0);
      std::vector<double> rho(nLambda, 0.0);
      std::vector<double> eta(nLambda, 0.0);

      lambdaArray[0] = input.lambdaMin_;
      const double lam_step = pow(10.0, log10(input.lambdaMax_ / input.lambdaMin_) / (nLambda-1));

      DenseColumnMatrixHandle Ax(new DenseColumnMatrix(M));
      DenseColumnMatrixHandle Rx(new DenseColumnMatrix(N));

      int lambda_index = 0;
      const DenseMatrix &forwardMatrixRef = *forwardMatrix_;

      int nr_rows = CCtr.nrows();
      int nr_cols = CCtr.ncols();
      const int NR_BOUNDS_CHECK = nr_rows * nr_cols;

      double* AtrA = ARRtrAtr.data();
      double* LLtr = CCtr.data();

      for (int j = 0; j < nLambda; j++)
      {
        if (j)
        {
          lambdaArray[j] = lambdaArray[j-1] * lam_step;
        }

        lambda_sq = lambdaArray[j] * lambdaArray[j]; //generate current lambda

        double* rm = regForMatrix.data();

        for (int i = 0; i < NR_BOUNDS_CHECK; i++)
        {
          rm[i] = AtrA[i] + lambda_sq * LLtr[i];
        }

        try
        {
          LinearAlgebra::solve_lapack(regForMatrix, *measuredDataRef, solution);
        }
        catch (LapackError& e)
        {
          const std::string errorMessage("The Tikhonov linear system could not be solved for a regularization parameter in the Lambda Range of the L-curve. Use a higher Lambda Range ''From'' value for the L-Curve calculation.");
          if (pr_)
          {
            pr_->error(errorMessage);
          }
          else
          {
            std::cerr << errorMessage << std::endl;
          }
          throw;
        }
        catch(DimensionMismatch& e)
        {
          const std::string errorMessage("Invalid matrix sizes are being used in the Tikhonov linear system.");
          if (pr_)
          {
            pr_->error(errorMessage);
          }
          else
          {
            std::cerr << errorMessage << std::endl;
          }
          throw;
        }

        RRtrAtr.multiply(solution, RRtrAtrsolution_handle);

        if (sourceWeighting_)
        {
          if (RRtrAtrsolution->nrows() == sourceWeighting_->ncols())
            Rx = *sourceWeighting_ * RRtrAtrsolution;
          else if  (RRtrAtrsolution->nrows() == sourceWeighting_->nrows())
            Rx = sourceWeighting_->transpose() * RRtrAtrsolution;
          else
          {
            const std::string errorMessage(" Solution weighting matrix unexpectedly does not fit to compute the weighted solution norm. ");
            if (pr_)
            {
              pr_->error(errorMessage);
            }
            else
            {
              std::cerr << errorMessage << std::endl;
            }
          }
        }
        else
          Rx = RRtrAtrsolution;

        // Calculate the norm of Ax-b and Rx for L curve
        forwardMatrixRef.multiply(RRtrAtrsolution_handle, *Ax);

        rho[j]=0; eta[j]=0;
        for (int k = 0; k < Ax->nrows(); k++)
        {
          double T = (*Ax)[k] - (*measuredDataRef)[k];
          rho[j] += T*T; //norm of the data fit term
        }

        for (int k = 0; k < Rx->nrows(); k++)
        {
          double T = Rx->get(k, 0);
          eta[j] += T*T; //norm of the model term
        }
        // eta and rho needed to plot the Lcurve and determine the L corner
        rho[j] = sqrt(rho[j]);
        eta[j] = sqrt(eta[j]);
      }
      boost::shared_ptr<LCurveInput> lcurveInput(new LCurveInput(rho, eta, lambdaArray, nLambda));
      lcurveInput_handle_ = lcurveInput;
      lambda = FindCorner(*lcurveInput_handle_, lambda_index);

      if (input.updateLCurveGui_)
        input.updateLCurveGui_(lambda, *lcurveInput_handle_, lambda_index);
    }
    lambda_sq = lambda * lambda;
    //compute the solution with the selected regularization parameter: lambda
    regularizationParameter_.reset(new DenseColumnMatrix(1));
    (*regularizationParameter_)[0] = lambda;
    int nr_rows = regForMatrix_handle->nrows();
    int nr_cols = regForMatrix_handle->ncols();

    double* AtrA = ARRtrAtr_h.get_data_pointer();
    double* RtrR = CCtr_h.get_data_pointer();
    double* rm   = regForMatrix.get_data_pointer();
    for (int i=0; i<(int)(nr_rows*nr_cols); i++)
    {
      rm[i] = AtrA[i] + lambda_sq * RtrR[i];
    }

    if (computeRegularizedInverse_)
    {
      regForMatrix.invert();

      if (sourceWeighting_)
      {
        inverseMatrix_ = RRtrAtr * regForMatrix_handle;
      }
      else
      {
        inverseMatrix_  = forward_transpose * regForMatrix_handle;
      }
      inverseSolution_ = inverseMatrix_ * measuredData_;
    }
    else
    {
      try
      {
        LinearAlgebra::solve_lapack(regForMatrix, *measuredDataRef,solution);
      }
      catch (LinearAlgebra::LapackError& e)
      {
        const std::string errorMessage("The Tikhonov linear system could not be solved for a regularization parameter in the Lambda Range of the L-curve. Use a higher Lambda Range ''From'' value for the L-Curve calculation.");
        if (pr_)
        {
          pr_->error(errorMessage);
        }
        else
        {
          std::cerr << errorMessage << std::endl;
        }
        throw;
      }
      catch(DimensionMismatch& e)
      {
        const std::string errorMessage("Invalid matrix sizes are being used in the Tikhonov linear system.");
        if (pr_)
        {
          pr_->error(errorMessage);
        }
        else
        {
          std::cerr << errorMessage << std::endl;
        }
        throw;
      }

      RRtrAtr.multiply(solution, RRtrAtrsolution_handle);
      inverseSolution_ = RRtrAtr * solution;
    }

  }
  else
    //OVERDETERMINED CASE,
    //similar procedure as underdetermined case (documentation comments similar, see above)
    if ( ((regularizationChoice_ == automatic) && (M>=N)) || (regularizationChoice_==overdetermined) )
    {
      //.........................................................................
      // OPERATE ON DATA:
      // Computes X = (A^T * C^T * C * A + LAMBDA * LAMBDA * R^T * R) * A^T * C^T * C *Y
      //.........................................................................
      // calculate A^T * A
      DenseMatrix forward_transpose = forwardMatrix_->transpose();
      DenseMatrix mat_AtrA;
      DenseMatrix CCtr;
      DenseMatrix matrixNoiseCov_transpose;

      if (!sensorWeighting_)
      //check if a Sensor Space (Noise Covariance) Weighting Matrix exist?
      {
        // For large problems, allocating this matrix causes the module to hang.
        // The CCtr matrix is only used if sensorWeighting is available.
        //
        // In general, this code really should be rewritten to use sparse matrices (SCIRun 5)
        // or have replaced with more efficient calculations
        // (Eigen may have algorithms we can use).
        //CCtr = DenseMatrix::identity(M);
        mat_AtrA = forward_transpose * *forwardMatrix_;
      }
      else
      {
        if (((regularizationResidualSubcase_ == residual_constrained) && ((M != sensorWeighting_->nrows()) || (M != sensorWeighting_->ncols()))) ||
            ((regularizationResidualSubcase_ == residual_constrained_squared) && (M != sensorWeighting_->ncols())))
        {
          BOOST_THROW_EXCEPTION(DimensionMismatch() << DimensionMismatchInfo("Data Residual Weighting Matrix (number of rows and columns) must fit matrix dimensions of Forward Matrix (number of rows)!"));
        }

        matrixNoiseCov_transpose = sensorWeighting_->transpose();

        // regularizationResidualSubcase_ is either residual_constrained (default) or residual_constrained_squared
        if (regularizationResidualSubcase_ == residual_constrained)
          CCtr = sensorWeighting_->dense();
        else
          if (regularizationResidualSubcase_ == residual_constrained_squared)
            CCtr = matrixNoiseCov_transpose * *sensorWeighting_;

        mat_AtrA = forward_transpose * CCtr * *forwardMatrix_;
      }
      // calculate R^T * R
      DenseMatrix RtrR;
      DenseMatrix regMat;

      if (!sourceWeighting_)
      {
        regMat = DenseMatrix::Identity(N, N);
        RtrR = DenseMatrix::Identity(N, N);
      }
      else
      {
        if(((regularizationSolutionSubcase_==solution_constrained) && ((N != sourceWeighting_->nrows()) || (N != sourceWeighting_->ncols()))) ||
           ((regularizationSolutionSubcase_==solution_constrained_squared) && (N != sourceWeighting_->ncols())))
        {
          BOOST_THROW_EXCEPTION(DimensionMismatch() << DimensionMismatchInfo("Solution Weighting Matrix (number of rows and columns) must fit matrix dimensions of Forward Matrix (number of columns) ! "));
        }
        regMat =  sourceWeighting_;

        if (regularizationSolutionSubcase_ == solution_constrained_squared)
          RtrR = sourceWeighting_->transpose() * regMat;
        else if (regularizationSolutionSubcase_ == solution_constrained)
          RtrR = regMat;
      }

      double lambda = 0, lambda_sq = 0;
      int lambda_index = 0;
      // calculate A^T * Y
      DenseColumnMatrix AtrY(N);
      DenseColumnMatrix measuredDataRef;
      if (sensorWeighting_)
      {
        measuredDataRef=*(CCtr*measuredData_)->column();
      }
      else
      {
        measuredDataRef= *measuredData_;
      }
      forwardMatrix_->mult_transpose(measuredDataRef, mat_AtrY);
      DenseMatrixHandle regForMatrix_handle(new DenseMatrix(N, N));
      DenseMatrix &regForMatrix = *regForMatrix_handle;
      DenseColumnMatrixHandle solution_handle(new DenseColumnMatrix(N));
      DenseColumnMatrixHandle Ax_handle(new DenseColumnMatrix(M));
      DenseColumnMatrixHandle Rx_handle(new DenseColumnMatrix(N));
      DenseColumnMatrix &solution = *solution_handle;
      DenseColumnMatrix &Ax = *Ax_handle;
      DenseColumnMatrix &Rx = *Rx_handle;
      if ((input.regMethod_ == "single") || (input.regMethod_ == "slider"))
      {
        if (input.regMethod_ == "single")
        {
          // Use single fixed lambda value, entered in UI
          lambda = input.lambdaFromTextEntry_;
        }
        else if (input.regMethod_ == "slider")
        {
          // Use single fixed lambda value, select via slider
          lambda = input.lambdaSlider_;
        }
      }
      else if (input.regMethod_ == "lcurve")
      {
        // Use L-curve, lambda from corner of the L-curve
        const int nLambda = input.lambdaCount_;

        std::vector<double> lambdaArray(nLambda);
        std::vector<double> rho(nLambda);
        std::vector<double> eta(nLambda);

        lambdaArray[0] = input.lambdaMin_;
        const double lam_step = pow(10.0, log10(input.lambdaMax_ / input.lambdaMin_) / (nLambda-1));

        double* AtrA = mat_AtrA_h.data();
        double* RtrR = mat_RtrR.data();
        double* rm   = regForMatrix.data();

        int s = N*N;
        for (int j = 0; j < nLambda; j++)
        {
          if (j)
          {
            lambdaArray[j] = lambdaArray[j-1] * lam_step;
          }

          lambda_sq = lambdaArray[j] * lambdaArray[j];

          ///////////////////////////////////////
          ////Calculating the solution directly
          ///////////////////////////////////////
          for (int i = 0; i < s; i++)
          {
            rm[i] = AtrA[i] + lambda_sq * RtrR[i];
          }

          // Before, solution will be equal to (A^T * y)
          // After, solution will be equal to x_reg
          try
          {
            LinearAlgebra::solve_lapack(regForMatrix, mat_AtrY,solution);
          }
          catch (LapackError& e)
          {
            const std::string errorMessage("The Tikhonov linear system could not be solved for a regularization parameter in the Lambda Range of the L-curve. Use a higher Lambda value for the L-Curve calculation.");
            if (pr_)
            {
              pr_->error(errorMessage);
            }
            else
            {
              std::cerr << errorMessage << std::endl;
            }
            throw;
          }
          catch(DimensionMismatch& e)
          {
            const std::string errorMessage("Invalid matrix sizes are being used in the Tikhonov linear system.");
            if (pr_)
            {
              pr_->error(errorMessage);
            }
            else
            {
              std::cerr << errorMessage << std::endl;
            }
            throw;
          }

          ////////////////////////////////
          const DenseMatrix &forwardMatrixRef = *forwardMatrix_;

          forwardMatrixRef.multiply(solution, Ax);
          const DenseColumnMatrixHandle measuredData = measuredData_->column();
          matrixRegMatD.multiply(solution, Rx);
          // Calculate the norm of Ax-b and Rx
          rho[j]=0; eta[j]=0;
          for (int k = 0; k < M; k++)
          {
            double T = Ax[k] - (*measuredData)[k];
            rho[j] += T*T;
          }

          for (int k = 0; k < N; k++)
          {
            double T = Rx[k];
            eta[j] += T*T;
          }

          rho[j] = sqrt(rho[j]);
          eta[j] = sqrt(eta[j]);
        }
        boost::shared_ptr<LCurveInput> lcurveInput(new LCurveInput(rho, eta, lambdaArray, nLambda));
        lcurveInput_handle_ = lcurveInput;
        lambda = FindCorner(*lcurveInput_handle_, lambda_index);

        if (input.updateLCurveGui_)
          input.updateLCurveGui_(lambda, *lcurveInput_handle_, lambda_index);

      } // END  else if (reg_method_.get() == "lcurve")
      lambda_sq = lambda * lambda;

      regularizationParameter_.reset(new DenseColumnMatrix(1));
      (*regularizationParameter_)[0] = lambda;
      int nr_rows = regForMatrix_handle->nrows();
      int nr_cols = regForMatrix_handle->ncols();
      const int NR_BOUNDS_CHECK = nr_rows * nr_cols;
      double* AtrA = mat_AtrA_h.data();
      double* RtrR = mat_RtrR.data();
      double* rm   = regForMatrix.data();

      for (int i = 0; i < NR_BOUNDS_CHECK; i++)
      {
        rm[i] = AtrA[i] + lambda_sq * RtrR[i];
      }
      if (computeRegularizedInverse_)
      {
        regForMatrix.invert();
        inverseMatrix_ = (regForMatrix_handle * forward_transpose) * CCtr;
        inverseSolution_ = inverseMatrix_ * measuredData_;
      }
      else
      {
        try
        {
          LinearAlgebra::solve_lapack(regForMatrix, mat_AtrY,solution);
        }
        catch (LapackError& e)
        {
          const std::string errorMessage("The Tikhonov linear system could not be solved for a regularization parameter in the Lambda Range of the L-curve. Use a higher Lambda value for the L-Curve calculation.");
          if (pr_)
          {
            pr_->error(errorMessage);
          }
          else
          {
            std::cerr << errorMessage << std::endl;
          }
          throw;
        }
        catch(DimensionMismatch& e)
        {
          const std::string errorMessage("Invalid matrix sizes are being used in the Tikhonov linear system.");
          if (pr_)
          {
            pr_->error(errorMessage);
          }
          else
          {
            std::cerr << errorMessage << std::endl;
          }
          throw;
        }
        inverseSolution_ = solution.dense();
      }

    }
}

void TikhonovAlgorithmImpl::update_graph(const TikhonovAlgorithmImpl::Input& input, double lambda, int lambda_index, const double epsilon)
{
  if (lcurveInput_handle_ && input.updateLCurveGui_)
  {
    lambda = LambdaLookup(*lcurveInput_handle_, lambda, lambda_index, epsilon);
    if (lambda >= 0)
    {
      input.updateLCurveGui_(lambda, *lcurveInput_handle_, lambda_index);
    }
  }
}

#if 0
//! Module execution
void SolveInverseProblemWithTikhonov::execute()
{
  // DEFINE MATRIX HANDLES FOR INPUT/OUTPUT PORTS
  MatrixHandle forward_matrix_h, hMatrixRegMat, hMatrixMeasDat, hMatrixNoiseCov;

  bool computeRegularizedInverse = false;

  if (! get_input_handle("- Forward problem", forward_matrix_h)) return;
  if (! get_input_handle("- Data vector", hMatrixMeasDat)) return;

  get_input_handle("- Solution constraint", hMatrixRegMat, false);
  get_input_handle("- Residual constraint", hMatrixNoiseCov, false);

  if (oport_connected("- Regularized inverse"))
  {
    computeRegularizedInverse = true;
  }

  TikhonovAlgorithmImpl::AlgorithmChoice gui_tikhonov_case = static_cast<TikhonovAlgorithmImpl::AlgorithmChoice>(tik_cases_.get());
  TikhonovAlgorithmImpl::AlgorithmSolutionSubcase gui_tikhonov_solution_subcase = static_cast<TikhonovAlgorithmImpl::AlgorithmSolutionSubcase>(tik_solution_subcases_.get());
  TikhonovAlgorithmImpl::AlgorithmResidualSubcase gui_tikhonov_residual_subcase = static_cast<TikhonovAlgorithmImpl::AlgorithmResidualSubcase>(tik_residual_subcases_.get());

  try
  {
    boost::shared_ptr<TikhonovAlgorithmImpl> algo(new TikhonovAlgorithmImpl(forward_matrix_h,
                                                                            hMatrixMeasDat,
                                                                            gui_tikhonov_case,
                                                                            gui_tikhonov_solution_subcase,
                                                                            gui_tikhonov_residual_subcase,
                                                                            hMatrixRegMat,
                                                                            hMatrixNoiseCov,
                                                                            computeRegularizedInverse, this));
    algo_handle_ = algo;

    TikhonovAlgorithmImpl::Input::lcurveGuiUpdate update = boost::bind(&SolveInverseProblemWithTikhonov::update_lcurve_gui, this, _1, _2, _3);
    boost::shared_ptr<TikhonovAlgorithmImpl::Input> input(new TikhonovAlgorithmImpl::Input(reg_method_.get(),
                                                                                           lambda_from_textentry_.get(),
                                                                                           log_val_.get(),
                                                                                           lambda_num_.get(),
                                                                                           lambda_min_.get(),
                                                                                           lambda_max_.get(),
                                                                                           update));
    input_handle_ = input;
    algo_handle_->run(*input_handle_);

    if (computeRegularizedInverse)
    {
      MatrixHandle inverse_matrix = algo_handle_->get_inverse_matrix();
      send_output_handle("- Regularized inverse", inverse_matrix);
    }

    MatrixHandle source_reconstruction_result = algo_handle_->get_inverse_solution();
    send_output_handle("- Inverse solution", source_reconstruction_result);

    ColumnMatrixHandle RegParameter = algo_handle_->get_regularization_parameter();
    send_output_handle("- Regularization parameter", RegParameter);
  }
  catch (Exception& e)
  {
    error(e.message());
    return;
  }
  catch (...)
  {
    error("Caught unexpected error. Module execute failed.");
    return;
  }
}

void SolveInverseProblemWithTikhonov::tcl_command(GuiArgs& args, void* userdata)
{
  if (args[1] == "updategraph" && args.count() == 4)
  {
    double lambda = boost::lexical_cast<double>(args[2]);
    int lambda_index = boost::lexical_cast<double>(args[3]);

    if (input_handle_.get() != 0 && algo_handle_.get() != 0)
    {
      algo_handle_->update_graph(*input_handle_, lambda, lambda_index, lambda_resolution_.get());
    }
  }
  else
  {
    // Relay data to the Module class
    Module::tcl_command(args, userdata);
  }
}

void SolveInverseProblemWithTikhonov::update_lcurve_gui(const double lambda, const TikhonovAlgorithmImpl::LCurveInput& input, const int lambda_index)
{
  lambda_corner_from_scale_.send(lambda);
  //estimate L curve corner
  const double lower_y = Min(input.eta_[0] / 10.0, input.eta_[input.nLambda_ - 1]);
  std::ostringstream str;
  str << get_id() << " plot_graph \" ";
  for (int i = 0; i < input.nLambda_; i++)
    str << log10( input.rho_[i] ) << " " << log10( input.eta_[i] ) << " ";
  str << "\" \" " << log10( input.rho_[0]/10.0 ) << " " << log10( input.eta_[lambda_index] ) << " ";
  str << log10( input.rho_[lambda_index] ) << " " << log10( input.eta_[lambda_index] ) << " ";
  str << log10( input.rho_[lambda_index] ) << " " << log10( lower_y ) << " \" ";
  str << lambda << " " << lambda_index << " ; update idletasks";

  TCLInterface::execute(str.str());
}
#endif

TikhonovAlgorithmImpl::LCurveInput::LCurveInput(const std::vector<double>& rho, const std::vector<double>& eta, const std::vector<double>& lambdaArray, int nLambda)
: rho_(rho), eta_(eta), lambdaArray_(lambdaArray), nLambda_(nLambda)
{}

TikhonovAlgorithmImpl::Input::Input(const std::string& regMethod, double lambdaFromTextEntry, double lambdaSlider, int lambdaCount, double lambdaMin, double lambdaMax,
                                    lcurveGuiUpdate updateLCurveGui)
: regMethod_(regMethod), lambdaFromTextEntry_(lambdaFromTextEntry), lambdaSlider_(lambdaSlider), lambdaCount_(lambdaCount), lambdaMin_(lambdaMin), lambdaMax_(lambdaMax),
updateLCurveGui_(updateLCurveGui)
{}

} // End namespace BioPSE
