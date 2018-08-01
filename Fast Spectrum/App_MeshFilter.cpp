#include "App_MeshFilter.h"

// Construct a Mesh Filter from Reduced-Eigenvectors
// INPUT: Vertex matrix & Mass Matrix M & Basis Matrix U & Reduced Eigenvectors & paramters of filter
// OUTPUT: New Vertex matrix
void constructMeshFilter(const Eigen::MatrixXd &V, const Eigen::SparseMatrix<double> &M, const Eigen::SparseMatrix<double> &U,
	const Eigen::MatrixXd &LDEigVec, FilterType filterType, const int &k, const int &m, Eigen::MatrixXd &Vout)
{
	Vout.resize(V.rows(), V.cols());

	// Create filters
	Eigen::VectorXd filter, noFilter;
	Eigen::MatrixXd Vref, Vfilter, Vresidu;
	createNoFilter(k, m, noFilter);
	
	switch (filterType)
	{
	case Filter_LowPass:
		createLowPassFilter(k, m, filter);
		break;
	case Filter_Linear:
		createLinearFilter(k, m, filter);
		break;
	case Filter_Polynomial:
		createPolynomialFilter(k, m, filter);
		break;
	case Filter_QuadMiddle:
		createQuadMiddleFilter(k, m, filter);
		break;
	case Filter_QuadMiddleInverse:
		createQuadMiddleFilterInverse(k, m, filter);
		break;
	default:
		break;
	}

	projectSpace(V, U, M, LDEigVec, noFilter, Vref);
	projectSpace(V, U, M, LDEigVec, filter, Vfilter);
	Vresidu = Vfilter - Vref;
	Vout = Vfilter + (double)filter(filter.size()-1) * Vresidu;
}

/* Obtain projection of every columns of V (x,y,z) */
void projectSpace(const Eigen::MatrixXd &Vin, const Eigen::SparseMatrix<double> &U, const Eigen::SparseMatrix<double> &M,
	const Eigen::MatrixXd &LDEigVec, const Eigen::VectorXd &filter, Eigen::MatrixXd &Vout)
{
	Eigen::SparseMatrix<double> MU = M*U;
	Vout = Eigen::MatrixXd::Zero(Vin.rows(), Vin.cols());

	Eigen::MatrixXd BB = Vin.transpose() * MU;
	Eigen::MatrixXd Gamma = BB * LDEigVec;
	for (int i = 0; i < Vin.cols(); i++) {
		Eigen::VectorXd theta = Eigen::VectorXd::Zero(LDEigVec.rows());

		omp_set_num_threads(omp_get_num_procs());
		int tid, ntids, ipts, istart, iproc, j;
#pragma omp parallel private(tid,ntids,ipts,istart,j)
		{
			iproc = omp_get_num_procs();
			tid = omp_get_thread_num();
			ntids = omp_get_num_threads();
			ipts = (int)ceil(1.00*(double)LDEigVec.rows() / (double)ntids);
			istart = tid * ipts;
			if (tid == ntids - 1) ipts = LDEigVec.rows() - istart;
			if (ipts <= 0) ipts = 0;

			for (int j = 0; j < filter.size(); j++) {
				theta.block(istart, 0, ipts, 1) += (filter(j)*Gamma.coeff(i, j))*LDEigVec.block(istart, j, ipts, 1);
			}
		}
		Vout.col(i) = U * theta;
	}
}

/* Construct Filters */
// No filter
void createNoFilter(const int &k, const int &m, Eigen::VectorXd &filterCoef)
{
	filterCoef.resize(m);
	for (int i = 0; i < m; i++) {
		filterCoef(i) = 1.0;
	}
}
// Low Pass Filter (A series of constants followed by decreasing quadratic functions)
void createLowPassFilter(const int &k, const int &m, Eigen::VectorXd &filterCoef)
{
	double scale = 1.0;
	filterCoef.resize(m);
	for (int i = 0; i < k; i++) {
		filterCoef(i) = scale * 1.0;
	}

	for (int i = k; i < m; i++) {
		filterCoef(i) = scale * (1.0 - (1.0 / (double)((m - k)*(m - k))) * (double)((i - k)*(i - k)));
	}

}

// Linear Filter (1 until a certain k, then increase linearly to 2 for m)
void createLinearFilter(const int &k, const int &m, Eigen::VectorXd &filterCoef)
{
	double scale = 1.0;
	filterCoef.resize(m);
	for (int i = 0; i < k; i++) {
		filterCoef(i) = 1.0;
	}

	for (int i = k; i < m; i++) {
		filterCoef(i) = 1.0 + scale*(i - (double)k) / (double)(m - k);
	}
}

// Polynomial Filter (1 until a certain k, then polynom function from k to m)
void createPolynomialFilter(const int &k, const int &m, Eigen::VectorXd &filterCoef)
{
	filterCoef.resize(m);
	for (int i = 0; i < k; i++) {
		filterCoef(i) = 1.0;
	}

	for (int i = k; i < m; i++) {
		filterCoef(i) = 2.0 + 1.0 / (double)((m - k)*(m - k)) * (i - k)*(i - k);
	}

}

// Polynomial Filter (1 until a certain k, then polynom function from k to m)
void createQuadMiddleFilter(const int &k, const int &m, Eigen::VectorXd &filterCoef)
{
	int p1, p2, p3;
	int q1, q2, q3;

	p1 = k;
	p3 = k + 500;
	q1 = m - 500;
	q3 = m;

	filterCoef.resize(m);

	// Ones part
	for (int i = 0; i < k; i++) {
		filterCoef(i) = 1.0;
	}

	// Increase
	double pVal = 1.0;
	double qVal = 0.0;
	double aVal = (-2.0*pVal + qVal*(double)p3) / ((double)(p3*p3*p3));
	double bVal = (pVal - 1.0 / 3.0*qVal*(double)p3) / (1.0 / 3.0 * (double)(p3*p3));

	for (int i = p1; i < p3; i++) {
		filterCoef(i) = 1.0 + aVal*(double)((i - p1)*(i - p1)*(i - p1)) + bVal*(double)((i - p1)*(i - p1));
	}

	// Constant, 3 part
	for (int i = p3; i < q3; i++) {
		filterCoef(i) = 1.0 + pVal;
	}

	// Decrease
	for (int i = q1; i < q3; i++) {
		filterCoef(i) = 1.0 + pVal - aVal*(double)((i - q1)*(i - q1)*(i - q1)) - bVal*(double)((i - q1)*(i - q1));
	}
}

// Polynomial Filter (1 until a certain k, then polynom function from k to m)
void createQuadMiddleFilterInverse(const int &k, const int &m, Eigen::VectorXd &filterCoef)
{
	int p1, p2, p3;
	int q1, q2, q3;

	p1 = 1000;
	p3 = 1000 + 500;
	q1 = 4000 - 500;
	q3 = 4000;

	filterCoef.resize(5000);

	// Ones part
	for (int i = 0; i < p1; i++) {
		filterCoef(i) = 1.0;
	}

	double pVal = 1.0;
	double qVal = 0.0;
	double aVal = (-2.0*pVal + qVal*(double)p3) / ((double)(p3*p3*p3));
	double bVal = (pVal - 1.0 / 3.0*qVal*(double)p3) / (1.0 / 3.0 * (double)(p3*p3));

	// Decrease
	for (int i = p1; i < p3; i++) {
		filterCoef(i) = 1.0 + pVal - aVal*(double)((i - p1)*(i - p1)*(i - p1)) - bVal*(double)((i - p1)*(i - p1));
	}

	// Constant, 3 part
	for (int i = p3; i < q1; i++) {
		filterCoef(i) = 0.0;
	}

	pVal = 2.0;
	qVal = 0.0;
	aVal = (-2.0*pVal + qVal*(double)p3) / ((double)(p3*p3*p3));
	bVal = (pVal - 1.0 / 3.0*qVal*(double)p3) / (1.0 / 3.0 * (double)(p3*p3));

	for (int i = q1; i < q3; i++) {
		filterCoef(i) = 0.0 + aVal*(double)((i - q1)*(i - q1)*(i - q1)) + bVal*(double)((i - q1)*(i - q1));
	}

	// Ones part
	for (int i = q3; i < 5000; i++) {
		filterCoef(i) = 2.0;
	}
}