#include "FastSpectrum.h"

/* [MAIN FUNCTIONS IN FAST APPROXIMATIONG ALGORITHM] */
void FastSpectrum::computeEigenPairs(Eigen::MatrixXd &V, Eigen::MatrixXi &F, const int &numOfSamples, Eigen::MatrixXd &reducedEigVects, Eigen::VectorXd &reducedEigVals)
{
	computeEigenPairs(V, F, numOfSamples, Sample_Poisson_Disk, reducedEigVects, reducedEigVals);
}

void FastSpectrum::computeEigenPairs(Eigen::MatrixXd &V, Eigen::MatrixXi &F, const int &numOfSamples, SamplingType sampleType, Eigen::MatrixXd &reducedEigVects, Eigen::VectorXd &reducedEigVals)
{
	// [1.2]	Constructing Laplacian Matrices (Stiffness and Mass-Matrix)
	constructLaplacianMatrix(V, F, S, M, AdM, avgEdgeLength, DistanceTableSpM);

	// [2]		SAMPLING
	constructSample(V, AdM, sampleSize, Sample);

	// [3]		BASIS CONSTRUCTION
	double	distRatio = sqrt(pow(0.7, 2) + pow(0.7, 2));
	maxNeighDist = distRatio * sqrt((double)V.rows() / (double)Sample.size()) * avgEdgeLength;
	constructBasis(V, F, Sample, AdM, DistanceTableSpM, sampleSize, maxNeighDist, Basis);
	formPartitionOfUnity(Basis);

	// [4]		LOW-DIM PROBLEM
	S_ = Basis.transpose() * S  * Basis;
	M_ = Basis.transpose() * M  * Basis;

	// [5]		SOLVING LOW-DIM EIGENPROBLEM
	computeEigenPair(S_, M_, reducedEigVects, reducedEigVals);
	normalizeReducedEigVects(Basis, M, reducedEigVects);
	this->reducedEigVects = reducedEigVects;
	this->reducedEigVals = reducedEigVals;
}

void FastSpectrum::computeEigenPairs(const string &meshFile, const int &numOfSamples, Eigen::MatrixXd &reducedEigVects, Eigen::VectorXd &reducedEigVals)
{
	computeEigenPairs(meshFile, numOfSamples, Sample_Poisson_Disk, reducedEigVects, reducedEigVals);
}

void FastSpectrum::computeEigenPairs(const string &meshFile, const int &numOfSamples, SamplingType sampleType, Eigen::MatrixXd &reducedEigVects, Eigen::VectorXd &reducedEigVals)
{
	readMesh(meshFile, V, F);
	computeEigenPairs(V, F, numOfSamples, Sample_Poisson_Disk, reducedEigVects, reducedEigVals);
}

/* [ENCAPSULATION]*/
void FastSpectrum::constructLaplacianMatrix(){
	constructLaplacianMatrix(V, F, S, M, AdM, avgEdgeLength, DistanceTableSpM);
}

void FastSpectrum::runSampling(){
	constructSample(V, AdM, sampleSize, Sample);
}

void FastSpectrum::constructBasis() {
	double	distRatio = sqrt(pow(1.1, 2) + pow(0.7, 2));			// A heuristic to make the support around 10.00 (i.e. number of non-zeros per row)
	maxNeighDist = distRatio * sqrt((double)V.rows() / (double)Sample.size()) * avgEdgeLength;
	constructBasis(V, F, Sample, AdM, DistanceTableSpM, sampleSize, maxNeighDist, Basis);
	formPartitionOfUnity(Basis);
}

void FastSpectrum::constructRestrictedProblem() {
	S_ = Basis.transpose() * S  * Basis;
	M_ = Basis.transpose() * M  * Basis;

	printf("A set of reduced stiffness and mass matrix (each %dx%d) is constructed.\n", S_.rows(), M_.cols());
}

void FastSpectrum::solveRestrictedProblem() {
	computeEigenPair(S_, M_, reducedEigVects, reducedEigVals);
	normalizeReducedEigVects(Basis, M, reducedEigVects);
}

/* [GETTER AMD SETTER] */
void FastSpectrum::liftEigenVectors() {
	approxEigVects = Basis*reducedEigVects;
}

void FastSpectrum::getV(Eigen::MatrixXd &V)
{
	V = this->V;
}
void FastSpectrum::getF(Eigen::MatrixXi &F)
{
	F = this->F;
}

void FastSpectrum::getSamples(Eigen::VectorXi &Sample)
{
	Sample = this->Sample;
}

void FastSpectrum::getFunctionBasis(Eigen::SparseMatrix<double> &Basis)
{
	Basis = this->Basis;
}

void FastSpectrum::getReducedEigVects(Eigen::MatrixXd &reducedEigVects)
{
	reducedEigVects = this->reducedEigVects;
}

void FastSpectrum::getReducedLaplacian()
{

}

void FastSpectrum::setV(const Eigen::MatrixXd &V){
	this->V = V;
}
void FastSpectrum::setF(const Eigen::MatrixXi &F){
	this->F = F;
}


/* [Read mesh model from file] */
void FastSpectrum::readMesh(const string &meshFile, Eigen::MatrixXd &V, Eigen::MatrixXi &F)
{
	V.resize(0, 0);
	F.resize(0, 0);

	if (meshFile.substr(meshFile.find_last_of(".") + 1) == "off") {
		igl::readOFF(meshFile, V, F);
	}
	else if (meshFile.substr(meshFile.find_last_of(".") + 1) == "obj") {
		igl::readOBJ(meshFile, V, F);
	}
	else {
		cout << "Error! File type can be either .OFF or .OBJ only." << endl;
		cout << "Program will exit in 2 seconds." << endl;
		Sleep(2000);
		exit(10);
	}

	printf("A new mesh with %d vertices (%d faces).\n", V.rows(), F.rows());
}

/* [Construct the stiffness & mass matrix AND other important tables] */
void FastSpectrum::constructLaplacianMatrix(Eigen::MatrixXd &V, Eigen::MatrixXi &F, Eigen::SparseMatrix<double> &S, Eigen::SparseMatrix<double> &M,
	vector<set<int>> &AdM, double &avgEdgeLength, Eigen::SparseMatrix<double> &DistanceTableSpM)
{
	/* Construct Stiffness matrix S */
	igl::cotmatrix(V, F, S);
	S = -S;

	/* Construct Mass matrix M*/
	igl::massmatrix(V, F, igl::MASSMATRIX_TYPE_BARYCENTRIC, M);

	/* Constructing Average length of the edges */
	avgEdgeLength = igl::avg_edge_length(V, F);

	/* Constructing Adjacency Matrix (neighborhood) */
	initiateAdM(V, F, AdM);

	/* Constructing table of edge-length using sparse Matrix (faster than vector-of-map and vector-of-unordered map) */
	initiateDistTableSpM(V, AdM, DistanceTableSpM);

	printf("A Stiffness matrix S(%dx%d) and Mass matrix M(%dx%d) are constructed.\n", S.rows(), S.cols(), M.rows(), M.cols());
}

/* [Construct samples for the subspace] */
void FastSpectrum::constructSample(const Eigen::MatrixXd &V, vector<set<int>> &AdM, int &sampleSize, Eigen::VectorXi &Sample)
{
	sampleSize = 100;
	int nBox = 13;

	/* Different type of sampling */
	//constructPoissonDiskSample(V, nBox, avgEdgeLength, Sample);
	//constructVoxelSample(viewer, V, nBox, Sample);	
	//constructRandomSample(Sample, V, sampleSize);
	constructFarthestPointSample(V, AdM, sampleSize, Sample);
	sampleSize = Sample.size();

	printf("A set of %d samples are constructed.\n", Sample.size());
}

/* [Construct Basis Matrix] */
void FastSpectrum::constructBasis(const Eigen::MatrixXd &V, Eigen::MatrixXi &T, const Eigen::VectorXi &Sample, const vector<set<int>> AdM, const Eigen::SparseMatrix<double> DistanceTableSpM,
	const int sampleSize, const double maxNeighDist, Eigen::SparseMatrix<double> &Basis)
{
	Basis.resize(V.rows(), Sample.size());
	Basis.reserve(20 * V.rows());

	vector<vector<Eigen::Triplet<double>>>	UTriplet;
	vector<Eigen::Triplet<double>>			AllTriplet;
	int				eCounter = 0;
	double			aCoef = 2.0 / (maxNeighDist*maxNeighDist*maxNeighDist);
	double			bCoef = -3.0 / (maxNeighDist*maxNeighDist);
	int				i = 0;
	Eigen::VectorXi	Si(1);

	vector<Eigen::Triplet<double>>	emptyTriplet;
	const int						NUM_OF_THREADS = omp_get_num_procs();
	omp_set_num_threads(NUM_OF_THREADS);
	UTriplet.resize(NUM_OF_THREADS);

	int tid, ntids, ipts, istart, iproc;
	chrono::high_resolution_clock::time_point	t4, t5;
	chrono::duration<double>					time_span4;

	vector<int> elementCounter;
	elementCounter.resize(NUM_OF_THREADS);
	int totalElementInsertions = 0;

	/* Manually break the chunck */
#pragma omp parallel private(tid,ntids,ipts,istart,i, t4,t5,time_span4)
	{
		t4		= chrono::high_resolution_clock::now();
		iproc	= omp_get_num_procs();
		tid		= omp_get_thread_num();
		ntids	= omp_get_num_threads();
		ipts	= (int)ceil(1.00*(double)sampleSize / (double)ntids);
		istart	= tid * ipts;
		if (tid == ntids - 1) ipts = sampleSize - istart;
		if (ipts <= 0) ipts = 0;

		// Instantiate D once, used for all Dijkstra
		Eigen::VectorXd				D(V.rows());
		for (int i = 0; i < V.rows(); i++) {
			D(i) = numeric_limits<double>::infinity();
		}

		//printf("Thread %d handles %d samples, starting from %d-th sample.\n", tid, ipts, istart);

		UTriplet[tid].reserve(2.0 * ((double)ipts / (double)sampleSize) * 20.0 * V.rows());

		for (i = istart; i < (istart + ipts); i++) {
			if (i >= sampleSize) break;
			/* Dijkstra basis function */
			ComputeDijkstraCompact(V, Sample(i), AdM, D, aCoef, bCoef, maxNeighDist, i, sampleSize, UTriplet[tid], elementCounter[tid]);
		}
	} /* End of OpenMP parallelization */

	int totalElements = 0;
	for (int j = 0; j < NUM_OF_THREADS; j++) {
		totalElements += UTriplet[j].size();
		totalElementInsertions += elementCounter[j];
	}

	/* Combining the Triplet */
	AllTriplet.resize(totalElements);
#pragma omp parallel for
	for (int j = 0; j < NUM_OF_THREADS; j++) {
		int tripSize = 0;
		for (int k = 0; k < j; k++) {
			tripSize += UTriplet[k].size();
		}
		std::copy(UTriplet[j].begin(), UTriplet[j].end(), AllTriplet.begin() + tripSize);
	}
	Basis.setFromTriplets(AllTriplet.begin(), AllTriplet.end());

	printf("A basis matrix (%dx%d) is constructed.\n", Basis.rows(), Basis.cols());
}

/* [Form partition of unity for the basis matrix Basis] */
void FastSpectrum::formPartitionOfUnity(Eigen::SparseMatrix<double> &Basis)
{
	/* Temporary variables */
	vector<Eigen::Triplet<double>>		diagTrip;
	Eigen::SparseMatrix<double>			DiagNormMatrix;
	Eigen::VectorXd						rowSum = Eigen::VectorXd::Zero(Basis.rows());
	DiagNormMatrix.resize(Basis.rows(), Basis.rows());

	/* Getting the sum of every non-zero elements in a row */
	for (int k = 0; k < Basis.outerSize(); ++k) {
		for (Eigen::SparseMatrix<double>::InnerIterator it(Basis, k); it; ++it) {
			rowSum(it.row()) += it.value();
		}
	}

	/* Get the diagonal matrix, reciprocal of the total sum per row */
	for (int i = 0; i < rowSum.size(); i++) {
		diagTrip.push_back(Eigen::Triplet<double>(i, i, 1.0 / rowSum(i)));
	}
	DiagNormMatrix.setFromTriplets(diagTrip.begin(), diagTrip.end());

	/* Form partition of unity */
	Basis = DiagNormMatrix * Basis;
}

/* [Compute the eigenpairs of Low-Dim Problem ] */
void FastSpectrum::computeEigenPair(Eigen::SparseMatrix<double> &S_, Eigen::SparseMatrix<double> &M_, Eigen::MatrixXd &LDEigVec, Eigen::VectorXd &LDEigVal)
{
	computeEigenGPU(S_, M_, LDEigVec, LDEigVal);
}

void FastSpectrum::normalizeReducedEigVects(const Eigen::SparseMatrix<double> &Basis, const Eigen::SparseMatrix<double> &M, Eigen::MatrixXd &LDEigVec)
{
	Eigen::SparseMatrix<double> UTMU;
	UTMU = Basis.transpose() * M *Basis;
	double mNorm;
	int i = 0;

	for (i = 0; i < LDEigVec.cols(); i++) {
		mNorm = LDEigVec.col(i).transpose() * UTMU * LDEigVec.col(i);
		mNorm = (double) 1.0 / sqrt(mNorm);
		LDEigVec.col(i) = mNorm * LDEigVec.col(i);
	}
	printf("A set of %d eigenparis is computed.\n", LDEigVec.cols());
}