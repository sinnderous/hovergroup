/************************************************************/
/*    NAME:                                               */
/*    ORGN: MIT                                             */
/*    FILE: TDOATracker.cpp                                        */
/*    DATE:                                                 */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "TDOATracker.h"

using namespace std;

//---------------------------------------------------------
// Constructor

TDOATracker::TDOATracker()
{
	acomms_heard = vector<bool>(3,0);
	s_dim = 3;vol = 0;
	x_offset = 0;y_offset = 0;
	tdoa_id = 9;

	boost::mt19937 *rng = new boost::mt19937();
	rng->seed(time(NULL));
	boost::normal_distribution<> distribution(0.0, 1.1);
	dist = boost::variate_generator< boost::mt19937, boost::normal_distribution<> > (*rng, distribution);
}

//---------------------------------------------------------
// Destructor

TDOATracker::~TDOATracker()
{
	for(int i=0;i<s_dim;i++){
		gsl_matrix_free(s1[i]);
		gsl_matrix_free(s2[i]);
		gsl_matrix_free(s3[i]);
		gsl_matrix_free(error_cov[i]);
	}
	gsl_vector_free(w);
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool TDOATracker::OnNewMail(MOOSMSG_LIST &NewMail)
{
	MOOSMSG_LIST::iterator p;

	for(p=NewMail.begin(); p!=NewMail.end(); p++) {
		CMOOSMsg &msg = *p;
		string key = msg.GetKey();

		if(key== "TDOA_PROTOBUF"){
			TDOA_protobuf.ParseFromString(msg.GetString());
			slots_heard = vector<int>(3,0);
			switch (TDOA_protobuf.cycle_state()){
			case 0:
				if(TDOA_protobuf.data_size()>0){
					slots_heard[1] = 1;
				}
				break;
			case 1:
				break;
			case 2:
				break;
			case 3:
				acomms_heard = vector<bool>(3,0);
				break;
			}
			NotifyStatus(TDOA_protobuf.cycle_state(),slots_heard);
		}
	}

	return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool TDOATracker::OnConnectToServer()
{
	// register for variables here
	// possibly look at the mission file?
	// m_MissionReader.GetConfigurationParam("Name", <string>);
	// m_Comms.Register("VARNAME", 0);

	m_MissionReader.GetConfigurationParam("TDOAid",tdoa_id);
	m_MissionReader.GetConfigurationParam("XOffset",x_offset);
	m_MissionReader.GetConfigurationParam("YOffset",y_offset);
	m_MissionReader.GetConfigurationParam("SDim",s_dim);	//number of sigma points

	// Getting Sigma Points
	string txtfile = "HermiteMatrices.txt";
	s1 = vector<gsl_matrix*>(s_dim);
	s2 = vector<gsl_matrix*>(s_dim);
	s3 = vector<gsl_matrix*>(s_dim);

	FILE* f = fopen(txtfile.c_str(),"r");
	cout << "Reading Sigma Point Matrices\n";

	for(int i=0;i<s_dim;i++){
		s1[i] = gsl_matrix_alloc(s_dim, s_dim);
		gsl_matrix_fscanf(f,s1[i]);
	}

	for(int i=0;i<s_dim;i++){
		s2[i] = gsl_matrix_alloc(s_dim, s_dim);
		gsl_matrix_fscanf(f,s2[i]);
	}

	for(int i=0;i<s_dim;i++){
		s2[i] = gsl_matrix_alloc(s_dim, s_dim);
		gsl_matrix_fscanf(f,s2[i]);
	}

	w = gsl_vector_alloc(s_dim);
	gsl_vector_fscanf(f,w);
	fscanf (f, "%lf", &vol);
	fclose(f);

	// Getting Error Covariance Matrices
	txtfile = "CovMatrices.txt";
	error_cov = vector<gsl_matrix*>(3);	//3 agents

	FILE* f = fopen(txtfile.c_str(),"r");
	cout << "Reading Cov Matrices\n";

	for(int i=0;i<3;i++){
		error_cov[i] = gsl_matrix_alloc(3, 3);	//3 states
		gsl_matrix_fscanf(f,error_cov[i]);
	}
	fclose(f);

	return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool TDOATracker::Iterate()
{
	return(true);
}

void TDOATracker::GetPriors(gsl_vector * xhat, gsl_matrix * P){
	gsl_vector *current_abs = gsl_vector_alloc(3);
	gsl_vector *dum = gsl_vector_alloc(3);
	for(int i=0;i<s_dim;i++){	//iterating over sigma points
		for(int j=0;j<s_dim;j++){
			for(int k=0;k<s_dim;k++){
				gsl_matrix *sP = MatrixSquareRoot(s_dim,P);
				gsl_vector_set(current_abs,1,gsl_matrix_get(s1[i],j,k));
				gsl_vector_set(current_abs,2,gsl_matrix_get(s2[i],j,k));
				gsl_vector_set(current_abs,3,gsl_matrix_get(s3[i],j,k));
				gsl_vector_add(current_abs,xhat);
				gsl_blas_dgemv(CblasNoTrans,1,sP,current_abs,0,dum);
				localNoise = sqrt(Q)*dist();
			}
		}
	}

}

int TDOATracker::func(double t, const double y[], double f[], void *params){
  double mu = *(double *)params;
  f[0] = y[1];
  f[1] = -y[0] - mu*y[1]*(y[0]*y[0] - 1);
  return GSL_SUCCESS;
}

int TDOATracker::jac(double t, const double y[], double *dfdy, double dfdt[], void *params){
  double mu = *(double *)params;
  gsl_matrix_view dfdy_mat
    = gsl_matrix_view_array (dfdy, 2, 2);
  gsl_matrix * m = &dfdy_mat.matrix;
  gsl_matrix_set (m, 0, 0, 0.0);
  gsl_matrix_set (m, 0, 1, 1.0);
  gsl_matrix_set (m, 1, 0, -2.0*mu*y[0]*y[1] - 1.0);
  gsl_matrix_set (m, 1, 1, -mu*(y[0]*y[0] - 1.0));
  dfdt[0] = 0.0;
  dfdt[1] = 0.0;
  return GSL_SUCCESS;
}

gsl_matrix* TDOATracker::MatrixSquareRoot(int dim, gsl_matrix * matrix_in){

	gsl_permutation * perm = gsl_permutation_alloc(dim);
	int s;
	gsl_vector_complex *eval = gsl_vector_complex_alloc(dim);
	gsl_matrix_complex *evec = gsl_matrix_complex_alloc(dim, dim);
	gsl_eigen_nonsymmv_workspace *w = gsl_eigen_nonsymmv_alloc(dim);

	gsl_eigen_nonsymmv(matrix_in, eval, evec, w);
	gsl_eigen_nonsymmv_free(w);
	gsl_eigen_nonsymmv_sort(eval, evec, GSL_EIGEN_SORT_ABS_DESC);

	gsl_vector_view eval_view = gsl_vector_complex_real(eval);
	gsl_vector *eval_real = &eval_view.vector;
	gsl_matrix *evec_real = gsl_matrix_alloc(dim,dim);

	for (int i = 0; i < dim; i++){
		for (int j = 0; j < dim; j++){
			gsl_matrix_set(evec_real,i,j,GSL_REAL(gsl_matrix_complex_get(evec, i, j)));
		}
	}

	gsl_vector_complex_free(eval);
	gsl_matrix_complex_free(evec);

	gsl_matrix *sqrt_e = gsl_matrix_alloc(dim,dim);
	for(int i=0;i<dim;i++){
		gsl_matrix_set(sqrt_e,i,i,sqrt(gsl_vector_get(eval_real,i)));
	}

	gsl_blas_dgemm(CblasNoTrans,CblasNoTrans,1.0,evec_real,sqrt_e,0.0,matrix_in);

	gsl_matrix * evec_inv = gsl_matrix_alloc(dim,dim);
	gsl_matrix * matrix_out = gsl_matrix_alloc(dim,dim);
	gsl_linalg_LU_decomp (evec_real, perm, &s);
	gsl_linalg_LU_invert (evec_real, perm, evec_inv);
	gsl_blas_dgemm(CblasNoTrans,CblasNoTrans,1.0,matrix_in,evec_inv,0.0,matrix_out);

	return matrix_out;
}

void TDOATracker::NotifyStatus(int cycle_id, vector<int> message_ids){
	stringstream tellme;
	tellme.str("Cycle: ");
	tellme << cycle_id << ' ';
	tellme << "Heard";
	for (vector<int>::iterator it = message_ids.begin() ; it != message_ids.end(); ++it){
		tellme << *it << ' ';
	}
	m_Comms.Notify("TRACKER_STATUS",tellme.str());
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool TDOATracker::OnStartUp()
{
	return(true);
}
