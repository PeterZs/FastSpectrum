#include "App_DiffusionDistance.h"

void constructDiffusionTensor(const Eigen::MatrixXd &approxEigVects, const Eigen::VectorXd &approxEigVals, const Eigen::MatrixXd &V,
	const int &numEigen, const vector<double> &t, vector<vector<Eigen::VectorXd>> &DiffusionTensor)
{
	Eigen::VectorXd LDVec = approxEigVals(0) * Eigen::VectorXd::Ones(approxEigVals.size());
	Eigen::VectorXd LDEigVal2 = (approxEigVects - LDVec) / approxEigVals(1);

	DiffusionTensor.resize(t.size());

	for (int tt = 0; tt < t.size(); tt++) {
		DiffusionTensor[tt].resize(V.rows());
		for (int x = 0; x < V.rows(); x++) {
			Eigen::VectorXd RLocHKS(numEigen);
			for (int i = 0; i < numEigen; i++) {
				RLocHKS(i) = exp(-1.0*approxEigVals(i)* (t[tt] / 2.0)) * approxEigVects(x, i);
			}
			DiffusionTensor[tt][x] = RLocHKS;
		}
	}
}

void constructDiffusionTensor(const Eigen::SparseMatrix<double> &Basis, const Eigen::MatrixXd &reducedEigVects, const Eigen::VectorXd &reducedEigVals, const Eigen::MatrixXd &V,
	const int &numEigen, const vector<double> &t, vector<vector<Eigen::VectorXd>> &DiffusionTensor)
{
	Eigen::MatrixXd approxEigVects = Basis * reducedEigVects;
	constructDiffusionTensor(approxEigVects, reducedEigVals, V, numEigen, t, DiffusionTensor);
}

void visualizeDiffusionDist(const Eigen::MatrixXd &V, const vector<vector<Eigen::VectorXd>> &DiffusionTensor, const int &tID, const int &vID, Eigen::VectorXd &diffVector)
{
	diffVector.resize(V.rows());

	for (int x = 0; x < V.rows(); x++) {
		Eigen::VectorXd diff = DiffusionTensor[tID][vID] - DiffusionTensor[tID][x];
		diffVector(x) = diff.norm();
	}
}