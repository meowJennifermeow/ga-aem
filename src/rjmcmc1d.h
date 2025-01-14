/*
This source code file is licensed under the GNU GPL Version 2.0 Licence by the following copyright holder:
Crown Copyright Commonwealth of Australia (Geoscience Australia) 2015.
The GNU GPL 2.0 licence is available at: http://www.gnu.org/licenses/gpl-2.0.html. If you require a paper copy of the GNU GPL 2.0 Licence, please write to Free Software Foundation, Inc. 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

Authors:
Ross C. Brodie, Geoscience Australia,
Richard L. Taylor, Geoscience Australia.
*/

#ifndef _rjMcMC1D_H_
#define _rjMcMC1D_H_

//standard library headers
#include <climits>
#include <cstdint>
#include <cfloat>
#include <memory>
#include <iomanip>

//custom headers
#include "general_utils.h"
#include "random_utils.h"
#include "vector_utils.h"
#include "ptrvec.h"

//third-party headers
#ifdef ENABLE_MPI
	#include "mpi.h"
#endif
#include <netcdf>

#define NUM_NOISE_HISTOGRAM_BINS 17
#define NUM_NUISANCE_HISTOGRAM_BINS 17

using namespace netCDF;
using namespace netCDF::exceptions;


class cParameterization {

	public: enum class Type { LINEAR, LOG10 };

	private:
		Type type = Type::LINEAR;

	public:

		cParameterization() {};

		cParameterization(const Type& t) {
			type = t;
		}

		cParameterization(const std::string& s) {
			type = get_type(s);
		}

		bool islinear() const {
			if (type == Type::LINEAR) return true;
			return false;
		}

		bool islog10() const {
			if (type == Type::LOG10) return true;
			return false;
		}

		std::string get_typestring() const {
			return get_typestring(type);
		}

		static std::string get_typestring(const Type& t) {
			std::string s;
				switch (t) {
				case Type::LINEAR: s = "LINEAR"; break;
				case Type::LOG10: s = "LOG10"; break;
				default: glog.errormsg(_SRC_, "Invalid ParameterizationType\n");
				}
			return s;
		};

		static Type get_type(const std::string& s)
		{
			if (strcasecmp(s, get_typestring(Type::LOG10)) == 0 ){
				return Type::LOG10;
			}
			else if(strcasecmp(s,get_typestring(Type::LINEAR)) == 0){
				return Type::LINEAR;
			}
			glog.errormsg(_SRC_, "Invalid ParameterizationType %s\n",s.c_str());
			return Type::LINEAR;
		};

};

class cProposal {

	public:
		enum class Type { VALUECHANGE, BIRTH, DEATH, MOVE, NUISANCE, NOISE };
		Type type;
		uint32_t np = 0;
		uint32_t na = 0;

		cProposal(const Type& t) {
			type = t;
		}

		void inc_np() { np++; }
		void inc_na() { na++; }

		float ar() const {
			if (np == 0) return 0.0;
			else return 100.0f * (float)na / (float)np;
		}
};

class rjMcMC1DLayer{

public:
	double ptop;
	double value;

	rjMcMC1DLayer() {
		ptop  = DBL_MAX;
		value = DBL_MAX;
	}

	int operator==(const rjMcMC1DLayer& a){
		if(ptop==a.ptop)return 1;
		else return 0;
	};

	int operator<(const rjMcMC1DLayer& a) const {
		if(ptop<a.ptop)return 1;
		else return 0;
	};

	int operator>(const rjMcMC1DLayer& a) const {
		if(ptop>a.ptop)return 1;
		else return 0;
	};
};

class rjMcMCNuisance{

public:
	// enum class Type { TX_HEIGHT, TX_ROLL, TX_PITCH, TX_YAW, TXRX_DX, TXRX_DY, TXRX_DZ, RX_ROLL, RX_PITCH, RX_YAW,		TXRX_ANGLE, TXRX_DISTANCE, UNKNOWN };
	//
	// Type type;
	double value;
	double min;
	double max;
	double sd_valuechange;

	// { return 13; }
	//
	// static std::string ntype2str(Type t){
	// 	if(t == Type::TX_HEIGHT) return "tx_height";
	// 	else if (t == Type::TX_ROLL) return "tx_roll";
	// 	else if (t == Type::TX_PITCH) return "tx_pitch";
	// 	else if (t == Type::TX_YAW) return "tx_yaw";
	// 	else if (t == Type::TXRX_DX) return "txrx_dx";
	// 	else if (t == Type::TXRX_DY) return "txrx_dy";
	// 	else if (t == Type::TXRX_DZ) return "txrx_dz";
	// 	else if (t == Type::RX_ROLL) return "rx_roll";
	// 	else if (t == Type::RX_PITCH) return "rx_pitch";
	// 	else if (t == Type::RX_YAW) return "rx_yaw";
	// 	else if (t == Type::TXRX_DISTANCE) return "txrx_distance";
	// 	else if (t == Type::TXRX_ANGLE) return "txrx_angle";
	// 	else if (t == Type::UNKNOWN) return "UnknownNuisanceType";
	// 	else{
	// 		return "UnknownNuisanceType";
	// 	}
	// };

	// static Type str2ntype(std::string s){
	// 	for(size_t i=0; i < number_of_types(); i++){
	// 		Type nt = (Type)i;
	// 		if(strcasecmp(s,ntype2str(nt))==0){
	// 			return nt;
	// 		}
	// 	}
	// 	return Type::UNKNOWN;
	// }

	virtual void settype(std::string s) = 0;
	// {
	// 	type = str2ntype(s);
	// }

	virtual std::string typestring() const = 0;
	// {
	// 	return ntype2str(type);
	// }

	//this method is necessary so we can deal with a vector
	//of abstract nuisances - std::vector cannot deal with memory
	//management and references for the nuisances if it doesn't know
	//about the implementation of rjMcMCNuisance.
	//this should copy the derived instance and return a pointer to it.

	virtual std::unique_ptr<rjMcMCNuisance> deepcopy() = 0;

	virtual ~rjMcMCNuisance() {};

};

class rjMcMCNoise {
public:
	double value;
	double min;
	double max;
	double sd_valuechange;

	std::pair<size_t, size_t> data_bounds;

};

class rjMcMC1DModel{

	double pmax;
	double vmin;
	double vmax;
	double misfit;

	std::vector<double> predicted;//the predicted data (forward model)
	//parameters for noise inversion
	//residual^2
	std::vector<double> residuals_squared;

public:
	//variance vector (assuming a diagonal
	//covariance matrix for now).
	//changes are applied to this vector when one
	//of the noise magnitudes above changes.
	std::vector<double> nvar;

	std::vector<rjMcMC1DLayer>  layers;
	ptr_vec<rjMcMCNuisance> nuisances;
	//multiplicative noise magnitudes
	std::vector<rjMcMCNoise> mnoises;


	void initialise(const double& maxp, const double& minv, const double& maxv)
	{
		layers.clear();
		nuisances.clear();
		mnoises.clear();

		misfit = DBL_MAX;
		pmax = maxp;

		vmin = minv;
		vmax = maxv;
	}

	size_t nlayers() const {return layers.size();}

	size_t nnuisances() const {return nuisances.size();}

	size_t nnoises() const { return mnoises.size(); }

	size_t nparams() const
	{
		size_t np = 2 * nlayers() + nnuisances();
		return np;
	}

	const double& get_misfit() const { return misfit; }

	double get_chi2() const {
		double chi2 = 0.0;
		for (size_t di = 0; di < residuals_squared.size(); di++) {
			chi2 += residuals_squared[di]/nvar[di];
		}
		return chi2;
	}

	void set_misfit(const double mfit) {
		misfit = mfit;
	}

	void set_predicted(const std::vector<double>& _predicted) {
		predicted = _predicted;
	}

	void set_residuals_squared(const std::vector<double>& _residuals_squared) {
		residuals_squared = _residuals_squared;
	}

	const std::vector<double>& get_predicted() const {
		return predicted;
	}

	const std::vector<double>& get_residuals_squared() const {
		return residuals_squared;
	}

	double logppd() const { return -misfit / 2.0 - std::log((double)nparams()); }

	void sort_layers()
	{
		std::sort(layers.begin(), layers.end());
	}

	size_t which_layer(const double& pos) const
	{
		for (size_t li = 0; li < nlayers() - 1; li++){
			if (pos < layers[li + 1].ptop){
				return li;
			}
		}
		return nlayers() - 1;
	}

	bool move_interface(const size_t& index, const double& pnew)
	{
		misfit = DBL_MAX;
		if(index == 0)return false;
		if(index >= nlayers())return false;

		if(pnew<=0)return false;
		if(pnew>=pmax)return false;

		layers[index].ptop = pnew;
		sort_layers();
		return true;
	}

	bool insert_interface(const double& pos, const double& vbelow)
	{
		misfit = DBL_MAX;
		if (pos < 0.0 || pos > pmax)return false;
		if (vbelow < vmin || vbelow > vmax)return false;

		const double minimumthickness = DBL_EPSILON;
		for (size_t li=0; li<nlayers(); li++){
			if (std::fabs(pos - layers[li].ptop) < minimumthickness){
				//don't allow crazy thin layers
				//std::cout << "*******************" << std::endl;
 				return false;
			}
		}

		rjMcMC1DLayer l;
		if (nlayers() == 0){
			l.ptop  = 0.0;
			l.value = vbelow;
			layers.push_back(l);
		}
		else{
			l.ptop  = pos;
			l.value = vbelow;
			layers.push_back(l);
			sort_layers();
		}
		return true;
	}

	bool delete_interface(const size_t& index)
	{
		misfit = DBL_MAX;
		if (index <= 0)return false;
		if (index >= nlayers())return false;
		layers[index].value = DBL_MAX;
		layers[index].ptop  = DBL_MAX;
		sort_layers();
		layers.pop_back();
		return true;
	}

	double value(const size_t& index){
		return layers[index].value;
	};

	double thickness(const size_t index){
		if (index < nlayers() - 1){
			const double p1 = layers[index].ptop;
			const double p2 = layers[index + 1].ptop;
			return p2 - p1;
		}
		else if (index == nlayers() - 1){
			return DBL_MAX;
		}
		else{
			glog.errormsg(_SRC_,"Invalid thicknes index");
			exit(1);
		}
	};

	std::vector<double> getvalues() const
	{
		std::vector<double> v;
		v.resize(nlayers());
		for (size_t i = 0; i<nlayers(); i++)v[i] = layers[i].value;
		return v;
	}

	std::vector<double> getthicknesses() const
	{
		std::vector<double> t;
		t.resize(nlayers() - 1);
		for (size_t i = 0; i<nlayers() - 1; i++){
			t[i] = layers[i + 1].ptop - layers[i].ptop;
		}
		return t;
	}

	void printmodel() const
	{
		printf("nl=%zu\tnn=%zu\tlppd=%lf\tmisfit=%lf\n", nlayers(), nnuisances(), logppd(), get_misfit());
		for (size_t li = 0; li<nlayers(); li++){
			printf("%4zu\t%10lf\t%10lf\n", li, layers[li].ptop, layers[li].value);
		}
		printf("\n");
	}

	void printmodelex() const
	{
		std::vector<double> c = getvalues();
		std::vector<double> t = getthicknesses();
		printf("nl=%zu\tnn=%zu\tlppd=%lf\tmisfit=%lf\n", nlayers(), nnuisances(), logppd(), get_misfit());
		double top = 0;
		double bot = 0;
		for (size_t li = 0; li<nlayers(); li++){
			if (li<nlayers() - 1){
				bot = top + t[li];
				printf("%4zu top=%7.2lfm bot=%7.2lfm thk=%7.2lfm conductivity=%6.4lf\n", li, top, bot, t[li], c[li]);
			}
			else{
				printf("%4zu top=%7.2lfm bot= Inf thk= Inf conductivity=%6.4lf\n", li, top, c[li]);
			}
			top = bot;
		}
		for (size_t ni = 0; ni<nnuisances(); ni++){			
			printf(" %s = %.3lf\n", (nuisances[(int)ni]->typestring()).c_str(), (nuisances[ni]->value));
		}
		printf("\n");
	}

	void printmodelex1() const
	{
		std::vector<double> c = getvalues();
		std::vector<double> t = getthicknesses();		
		for (size_t li = 0; li < nlayers(); li++) {			
			printf("%6.4lf ", pow10(c[li]));
		}
		printf("\n");
		for (size_t li = 0; li < nlayers()-1; li++) {
			printf("%6.4lf ", t[li]);
		}		
		printf("\n");
		for (size_t ni = 0; ni < nnuisances(); ni++) {
			printf(" %s = %.3lf\n", (nuisances[(int)ni]->typestring()).c_str(), (nuisances[ni]->value));
		}
		printf("\n");
	}
};

class rjMcMC1DNoiseMap {
private:
	size_t nentries = 0;
	std::vector<std::pair<size_t,size_t>> datalims; //bounds for each noise process
	size_t nnoises = 0;
public:
	size_t get_nnoises() const { return nnoises; }

	size_t get_nentries() const { return nentries; }

	std::vector<std::vector<double>> noises;

	void resettozero() {
		for (size_t i = 0; i < noises.size(); i++) {
			noises[i].clear();
		}
		noises.clear();
		datalims.clear();
		nentries = 0;
		nnoises = 0;
	}

	void addmodel(const rjMcMC1DModel& m) {
		if (noises.size() != m.nnoises()) {
			nnoises = m.nnoises();
			noises.resize(m.nnoises());
			datalims.resize(m.nnoises());
			for (size_t i = 0; i < m.nnoises(); i++) {
				datalims[i] = m.mnoises[i].data_bounds;
			}
		}

		for (size_t i = 0; i < m.nnoises(); i++) {
			noises[i].push_back(m.mnoises[i].value);
		}
		nentries++;
	}

	void writedata(FILE* fp) {
		size_t nn = noises.size();
		for (size_t i = 0; i < nn; i++) {
			//do statistics and histogram for each noise process
			cStats<double> s(noises[i]);
			cHistogram<double, size_t> hist(noises[i],s.min,s.max,NUM_NOISE_HISTOGRAM_BINS);

			fprintf(fp, "%zu %zu", datalims[i].first, datalims[i].second);
			fprintf(fp, " %lf", s.min);
			fprintf(fp, " %lf", s.max);
			fprintf(fp, " %lf", s.mean);
			fprintf(fp, " %lf", s.std);
			fprintf(fp, " %zu", get_nentries());

			fprintf(fp, " %zu", hist.nbins);
			for (size_t j = 0; j<hist.nbins; j++){
				fprintf(fp, " %lf", hist.centre[j]);
			}
			for (size_t j = 0; j<hist.nbins; j++){
				fprintf(fp, " %zu", hist.count[j]);
			}
			fprintf(fp, "\n");
		}
		for (size_t i = 0; i<nn; i++){
			for (size_t j = 0; j<nn; j++){
				double cov = covariance(noises[i], noises[j]);
				fprintf(fp, " %15.6e", cov);
			}
			fprintf(fp, "\n");
		}

		for (size_t i = 0; i<nn; i++){
			for (size_t j = 0; j<nn; j++){
				double cor = correlation(noises[i], noises[j]);
				fprintf(fp, " %15.6e", cor);
			}
			fprintf(fp, "\n");
		}

		for (size_t i = 0; i<nn; i++){
			bwrite(fp, noises[i]);
		}


	}

};

class rjMcMC1DNuisanceMap{

private:
	size_t nentries=0;
	std::vector<std::string> typestring;

public:

	size_t get_nentries() const { return nentries; }

	size_t get_nnuisances() const { return typestring.size(); }

	const std::vector<std::string>& get_types() const {
		return typestring;
	}

	std::vector<std::vector<double>> nuisance;

	void resettozero(){
		for (size_t i = 0; i < nuisance.size(); i++){
			nuisance[i].clear();
		}
		nuisance.clear();
		nentries = 0;
	}

	void addmodel(const rjMcMC1DModel& m)
	{
		if (nuisance.size() != m.nnuisances()){
			nuisance.resize(m.nnuisances());
			typestring.resize(m.nnuisances());
			for (size_t i = 0; i < nuisance.size(); i++){
				typestring[i] = m.nuisances[i]->typestring();
			}
		}

		for (size_t i = 0; i<nuisance.size(); i++){
			nuisance[i].push_back(m.nuisances[i]->value);
		};

		nentries++;
	}

	void writedata(FILE* fp)
	{
		size_t nn = nuisance.size();
		for (size_t i = 0; i<nn; i++){
			cStats<double> s(nuisance[i]);
			cHistogram<double, size_t> hist(nuisance[i],s.min,s.max,NUM_NUISANCE_HISTOGRAM_BINS);

			fprintf(fp, "%s", typestring[i].c_str());
			fprintf(fp, " %lf", s.min);
			fprintf(fp, " %lf", s.max);
			fprintf(fp, " %lf", s.mean);
			fprintf(fp, " %lf", s.std);
			fprintf(fp, " %zu", get_nentries());

			fprintf(fp, " %zu", hist.nbins);
			for (size_t j = 0; j<hist.nbins; j++){
				fprintf(fp, " %lf", hist.centre[j]);
			}
			for (size_t j = 0; j<hist.nbins; j++){
				fprintf(fp, " %zu", hist.count[j]);
			}
			fprintf(fp, "\n");
		}

		for (size_t i = 0; i<nn; i++){
			for (size_t j = 0; j<nn; j++){
				double cov = covariance(nuisance[i], nuisance[j]);
				fprintf(fp, " %15.6e", cov);
			}
			fprintf(fp, "\n");
		}

		for (size_t i = 0; i<nn; i++){
			for (size_t j = 0; j<nn; j++){
				double cor = correlation(nuisance[i], nuisance[j]);
				fprintf(fp, " %15.6e", cor);
			}
			fprintf(fp, "\n");
		}

		for (size_t i = 0; i<nn; i++){
			bwrite(fp, nuisance[i]);
		}

	}
};

class rjMcMC1DPPDMap{

private:
	size_t nentries;//number of samples
	size_t nlmin;
	size_t nlmax;
	double pmax;
	double vmin;
	double vmax;
	size_t np;//number of positions
	size_t nv;//number of values
	double dp;
	double dv;

	// double nmmin;//min, max and number of bins for noise magnitude
	// double nmmax;
	// double nnm;

public:

	std::vector<double> pbin;//positions
	std::vector<double> vbin;//values
	std::vector<uint32_t> counts;//frequency
	std::vector<uint32_t> cpcounts;//changepoints
	std::vector<uint32_t> layercounts;//number of layers

	// std::vector<double> nmags;//multiplicative noise magnitudes

	size_t get_nentries() const { return nentries; }

	size_t npbins() { return np; }

	size_t nvbins() { return nv; }

	size_t index(const size_t& pi, const size_t& vi) const {
		return pi * nv + vi;
	}

	uint32_t* row_ptr(const size_t pi) {
		return &(counts[index(pi, 0)]);
	}

	double toppbin(size_t i){ return pbin[i] - dp/2.0;}

	const std::vector<uint32_t>& changepoint(){ return cpcounts; }

	size_t getvbin(double val){
		if (val < vmin)return 0;
		if (val >= vmax)return nv-1;
		return (size_t)((val-vmin)/dv);
	}

	size_t getpbin(double pos){
		if (pos < 0.0)return 0;
		if (pos >= pmax)return np-1;
		return (size_t)(pos/dp);
	}

	void initialise(size_t _nlmin, size_t _nlmax, double _pmax, size_t _np, double _vmin, double _vmax, size_t _nv)
	{
		nlmin = _nlmin;
		nlmax = _nlmax;
		np = _np;
		nv = _nv;
		pmax = _pmax;
		vmin = _vmin;
		vmax = _vmax;

		dp = _pmax / (double)(np);
		dv = (vmax - vmin)/(double)(nv);

		pbin.resize(np);
		for (size_t i = 0; i<np; i++)pbin[i] = dp*((double)i+0.5);

		vbin.resize(nv);
		for (size_t i = 0; i<nv; i++)vbin[i] = vmin + dv*((double)i+0.5);
		resettozero();
	}

	//set up and zero the noise histogram
	// void initialise_noises(double _nmmin, double _nmmax, size_t _nnm) {
	// 	nmmin = _nmmin;
	// 	nmmax = _nmmax;
	// 	nnm = _nnm;
	// 	nmags.assign(nnm,0);
	// }

	void resettozero()
	{
		nentries = 0;
		layercounts.assign(nlmax - nlmin + 1, 0);
		counts.assign(np*nv,0);
		cpcounts.assign(np,0);
	}

	void addmodel(const rjMcMC1DModel& m)
	{
		nentries++;

		layercounts[m.nlayers()-nlmin]++;

		for(size_t pi=0; pi<np; pi++){
			size_t li = m.which_layer(pbin[pi]);
			size_t vi = getvbin(m.layers[li].value);
			counts[pi*nv+vi]++;
		}

		for(size_t li=1; li<m.nlayers(); li++){
			size_t pi = getpbin(m.layers[li].ptop);
			cpcounts[pi]++;
		}

	}

	std::vector<double> modelmap(const rjMcMC1DModel& m)
	{
		std::vector<double> model(np);
		for (size_t pi=0;pi<np;pi++){
			size_t li = m.which_layer(pbin[pi]);
			model[pi] = m.layers[li].value;
		}
		return model;
	}

	cHistogramStats<double> hstats(const size_t& pi){
		cHistogramStats<double> hs(vbin, row_ptr(pi));
		return hs;
	}

	std::vector<cHistogramStats<double>> hstats(){
		std::vector<cHistogramStats<double>> hs(np);
		for (size_t pi = 0; pi < np; pi++){
			hs[pi] = hstats(pi);
		}
		return hs;
	}

	struct cSummaryModels {
		std::vector<float> mean;
		std::vector<float> mode;
		std::vector<float> p10;
		std::vector<float> p50;
		std::vector<float> p90;

	public:
		cSummaryModels(const size_t& n) {
			mean.resize(n);
			mode.resize(n);
			p10.resize(n);
			p50.resize(n);
			p90.resize(n);
		}
	};

	cSummaryModels get_summary_models() {
		cSummaryModels s(np);
		for (size_t pi = 0; pi < np; pi++) {
			cHistogramStats<double> hs = hstats(pi);
			s.mean[pi] = (float)hs.mean;
			s.mode[pi] = (float)hs.mode;
			s.p10[pi]  = (float)hs.p10;
			s.p50[pi]  = (float)hs.p50;
			s.p90[pi]  = (float)hs.p90;
		}
		return s;
	}

};

class cChainHistory{

public:
	std::vector<float> temperature;
	std::vector<uint32_t> sample;
	std::vector<uint32_t> nlayers;
	std::vector<float> misfit;
	std::vector<float> logppd;
	std::vector<float> ar_valuechange;
	std::vector<float> ar_move;
	std::vector<float> ar_birth;
	std::vector<float> ar_death;
	std::vector<float> ar_nuisancechange;
	std::vector<float> ar_noisechange;
	std::vector<rjMcMC1DModel> models;
};

class rjMcMC1DSampler;

class cChain {

private:

public:

	cProposal pvaluechange = cProposal(cProposal::Type::VALUECHANGE);
	cProposal pmove = cProposal(cProposal::Type::MOVE);
	cProposal pbirth = cProposal(cProposal::Type::BIRTH);
	cProposal pdeath = cProposal(cProposal::Type::DEATH);
	cProposal pnuisancechange = cProposal(cProposal::Type::NUISANCE);
	cProposal pnoisechange = cProposal(cProposal::Type::NOISE);

	cChainHistory history;
	std::vector<uint32_t> swap_histogram;
	double temperature;
	rjMcMC1DModel model;
};

class rjMcMC1DSampler{

public:
	size_t mpiRank;
	size_t mpiSize;
	bool   verbose = false;

	//const double DEFAULTLOGSTDDECADES   = 0.05;
	//const double DEFAULTMOVESTDFRACTION = 0.05;
	const double DEFAULTLOGSTDDECADES   = 1.0;
	const double DEFAULTMOVESTDFRACTION = 0.25;

	std::string starttime;
	std::string endtime;
	double samplingtime;

	ptr_vec<rjMcMCNuisance> nuisance_init;

	//parameters for noise prior
	std::vector<double> noisemag_sd;
	std::vector<std::pair<size_t, size_t>> noisemag_dbounds;
	std::vector<std::pair<double, double>> noisemag_priorbounds;

	size_t  nl_min;
	size_t  nl_max;
	double  pmax;
	double  vmin;
	double  vmax;
	cParameterization param_position;
	cParameterization param_value;

	size_t nsamples;
	size_t nburnin;
	size_t thinrate;
	double temperature_high;//lowest is always 1.0

	bool BirthDeathFromPrior;

	rjMcMC1DPPDMap pmap;
	rjMcMC1DNuisanceMap nmap;
	rjMcMC1DNoiseMap mnmap;
	std::vector<cChain> chains;
	std::vector<rjMcMC1DModel> ensemble;
	rjMcMC1DModel  HighestLikelihood;
	rjMcMC1DModel  LowestMisfit;

	size_t ndata;
	std::vector<double> obs;
	//initial noise variance vector.
	//if sampling multiplicative noises this should contain
	//the additive noise floor for each data vector element.
	std::vector<double> err;

	static double gaussian_pdf(const double mean, const double std, const double x)
	{
		double p = std::exp(-0.5 * std::pow((x - mean) / std, 2.0)) / (std::sqrt(TWOPI)*std);
		return p;
	}

	double get_misfit(const rjMcMC1DModel& m) const
	{
		return m.get_misfit();
	};

	double get_normalised_misfit(const rjMcMC1DModel& m) const
	{
		return m.get_misfit()/(double)ndata;
	};

	double get_normalised_misfit1(const rjMcMC1DModel& m) const
	{
		return m.get_misfit() / (double)ndata;
	};

	size_t nchains() const { return chains.size(); };

	void addmodel(const rjMcMC1DModel& m)
	{
		pmap.addmodel(m);
		nmap.addmodel(m);
		mnmap.addmodel(m);
	}

	virtual std::vector<double> forwardmodel(const rjMcMC1DModel& m) = 0;

	size_t nnuisances()
	{
		return nuisance_init.size();
	}
	
	size_t nnoises()
	{
		return noisemag_sd.size();
	}

	bool isinbounds(const double& bmin, const double& bmax, const double& b)
	{
		if (b<bmin || b>bmax)return false;
		else return true;
	}

	bool should_include_in_maps(const size_t& si)
	{
		if (si < nburnin) return false;
		size_t k = si - nburnin;
		if ((k % thinrate) == 0) {
			return true;
		}
		return false;
	}

	// double l2misfit(const std::vector<double>& g)
	// {
	// 	double sum = 0.0;
	// 	for (size_t di = 0; di < ndata; di++) {
	// 		double nr = (obs[di] - g[di]) / err[di];
	// 		sum += nr * nr;
	// 	}
	// 	return sum;
	// }

	double standard_l2misfit(const rjMcMC1DModel& m) const
	{
		std::vector<double> r2 = m.get_residuals_squared();
	 	double sum = 0.0;
	 	for (size_t di = 0; di < ndata; di++) {
	 		double nr = r2[di] * (obs[di]*obs[di]) / (err[di]*err[di]);
	 		sum += nr * nr;
	 	}
	 	return sum/ndata;
	}

	void compute_predicted_and_residuals_squared(rjMcMC1DModel& m) {
		//bookmark
		std::vector<double> pred = forwardmodel(m);				
		std::vector<double> res2(ndata);
		for (size_t di = 0; di < ndata; di++){
			double rd = (obs[di]-pred[di])/obs[di];			
			res2[di] = rd*rd;
		}
		m.set_predicted(pred);
		m.set_residuals_squared(res2);		
	}

	void set_misfit(rjMcMC1DModel& m)
	{
		//bookmark
		//Set to dummy constant for testing the the prior is returned
		//std::vector<double> tmp(ndata,1.0);
		//m.set_misfit(1);		
		//m.set_predicted(tmp);
		//m.set_residuals_squared(tmp);
		//return;
				
		compute_predicted_and_residuals_squared(m);

		const std::vector<double>& res2 = m.get_residuals_squared();
		double negloglike = 0.0;
		for (size_t di = 0; di < ndata; di++) {			
			negloglike += res2[di]/(m.nvar[di]);
			//prefactor term necessary for noise moves to work correctly
			//see my (RT) notes for noise move with fixed additive floor
			//to see how this works.
			negloglike += std::log(m.nvar[di]);
		}
		m.set_misfit(negloglike);		
	}

	void set_misfit_noisechange(rjMcMC1DModel& m, double nv, size_t ni) {
		//reset the misfit for a noise magnitude change, without
		//recomputing the forward.
		double prev_nv = m.mnoises[ni].value;
		m.mnoises[ni].value = nv;
		std::pair<size_t, size_t> bounds = m.mnoises[ni].data_bounds;

		double var_old;
		double negloglike = m.get_misfit();
		std::vector<double> res = m.get_residuals_squared();
		for (size_t di = bounds.first; di < bounds.second; di++) {
			//reset this variance element
			var_old = m.nvar[di];
			m.nvar[di] = m.nvar[di] - prev_nv * prev_nv + nv * nv;
			//compute new misfit contribution

			negloglike -= res[di] / var_old + std::log(var_old);
			negloglike += res[di] / m.nvar[di] + std::log(m.nvar[di]);
		}
		m.set_misfit(negloglike);
	}

	bool should_save_convergence_record(const size_t& si)
	{
		if (si == 0 || si == nsamples - 1)return true;
		size_t k = (size_t) std::pow(10, std::floor(std::log10(si)));
		if (k > thinrate) k = thinrate;
		if (si % k == 0)return true;
		return false;
	}

	bool propose_valuechange(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;
		chn.pvaluechange.inc_np();

		size_t index = irand((size_t)0, (size_t)(mcur.nlayers() - 1));

		double logstd = DEFAULTLOGSTDDECADES;
		double vold = mcur.layers[index].value;
		double vnew;
		double pqratio;
		if (param_value.islinear()) {
			double m = (std::pow(10.0, logstd) - std::pow(10.0, -logstd)) / 2.0;
			vnew = vold + m * vold * nrand<double>();
			double qpdfforward = gaussian_pdf(vold, m * vold, vnew);
			double qpdfreverse = gaussian_pdf(vnew, m * vnew, vold);
			pqratio = qpdfreverse / qpdfforward;
		}
		else {
			vnew = vold + logstd * nrand<double>();
			pqratio = 1.0;
		}

		bool isvalid = isinbounds(vmin, vmax, vnew);
		if (isvalid == false) return false;
		mpro.layers[index].value = vnew;
		set_misfit(mpro);

		double logpqr = std::log(pqratio);
		double loglr = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logar = logpqr + loglr;
		if (std::log(urand<double>()) < logar) {
			chn.pvaluechange.inc_na();
			return true;
		}
		return false;
	}

	bool propose_move(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;
		chn.pmove.inc_np();

		size_t n = mcur.nlayers();
		if (n <= 1)return false;

		size_t index = irand((size_t)1, n - 1);
		double pold = mcur.layers[index].ptop;

		//double std = sd_move;
		double std = DEFAULTMOVESTDFRACTION * pold;
		double pnew = pold + std * nrand<double>();
		double qpdfforward = gaussian_pdf(pold, pold * DEFAULTMOVESTDFRACTION, pnew);
		double qpdfreverse = gaussian_pdf(pnew, pnew * DEFAULTMOVESTDFRACTION, pold);

		bool isvalid = mpro.move_interface(index, pnew);
		if (isvalid == false)return false;

		set_misfit(mpro);
		double pqratio = qpdfreverse / qpdfforward;
		double logpqr = std::log(pqratio);

		double loglr = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logar = logpqr + loglr;
		double logu = std::log(urand<double>());
		if (logu < logar) {
			chn.pmove.inc_na();
			return true;
		}
		return false;
	}

	bool propose_birth(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;
		chn.pbirth.inc_np();

		size_t n = mcur.nlayers();
		if (n >= nl_max)return false;

		double  pos = urand(0.0, pmax);
		size_t  index = mcur.which_layer(pos);
		double vold = mcur.layers[index].value;
		double vnew, pqratio;

		if (BirthDeathFromPrior) {
			vnew = urand(vmin, vmax);
			pqratio = 1.0;
		}
		else {
			double vcpdf;
			double logstd = DEFAULTLOGSTDDECADES;
			if (param_value.islinear()) {
				double m = (std::pow(10.0, logstd) - std::pow(10.0, -logstd)) / 2.0;
				vnew = vold + m * vold * nrand<double>();
				vcpdf = gaussian_pdf(vold, m * vold, vnew);
			}
			else {
				vnew = vold + logstd * nrand<double>();
				vcpdf = gaussian_pdf(vold, logstd, vnew);
			}
			pqratio = 1.0 / ((vmax - vmin) * vcpdf);
		}
		
		//Jefferies?
		//pqratio *= (double)(n) / double(n + 1);

		bool isvalid = mpro.insert_interface(pos, vnew);
		if (isvalid == false)return false;
		set_misfit(mpro);

		double logpqr = std::log(pqratio);
		double loglr  = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logar  = logpqr + loglr;
		if (std::log(urand<double>()) < logar) {
			chn.pbirth.inc_na();
			return true;
		}
		return false;
	}

	bool propose_death(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;
		chn.pdeath.inc_np();

		size_t n = mcur.nlayers();
		if (n <= nl_min)return false;

		size_t index = irand((size_t)1, n - 1);
		bool isvalid = mpro.delete_interface(index);
		if (isvalid == false)return false;
		set_misfit(mpro);

		double pqratio;
		if (BirthDeathFromPrior) {
			pqratio = 1.0;
		}
		else {
			double logstd = DEFAULTLOGSTDDECADES;
			double vnew = mcur.layers[index - 1].value;
			double vold = mcur.layers[index].value;
			double vcpdf;
			if (param_value.islinear()) {
				double m = (pow(10.0, logstd) - pow(10.0, -logstd)) / 2.0;
				vcpdf = gaussian_pdf(vnew, m * vnew, vold);
			}
			else {
				vcpdf = gaussian_pdf(vnew, logstd, vold);
			}
			pqratio = (vmax - vmin) * vcpdf;
		}
		
		//Jefferies?
		//pqratio *= (double)(n) / double(n - 1);

		double logpqr = std::log(pqratio);
		double loglr  = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logar  = logpqr + loglr;
		if (std::log(urand<double>()) < logar) {
			chn.pdeath.inc_na();
			return true;
		}
		return false;
	}

	bool propose_nuisancechange(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;
		chn.pnuisancechange.inc_np();

		size_t ni = irand((size_t)0, mcur.nnuisances() - 1);

		
		double delta = nrand<double>() * mcur.nuisances[ni]->sd_valuechange;		
		double nv = mcur.nuisances[ni]->value + delta;
		
		bool isvalid = isinbounds(mcur.nuisances[ni]->min, mcur.nuisances[ni]->max, nv);
		if (isvalid == false)return false;

		//std::cout << "Delta = " << delta << std::endl;
		//std::cout << "nv    = " << nv << std::endl;
		mpro.nuisances[ni]->value = nv;

		set_misfit(mpro);
		double loglr = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logu = log(urand<double>());
		if (logu < loglr) {
			chn.pnuisancechange.inc_na();
			return true;
		}
		return false;
	}

	bool propose_noisechange(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;

		chn.pnoisechange.inc_np();

		//propose a change to the multiplicative noise magnitudes.
		//one multiplicative noise per system.
		size_t ni = irand((size_t)0, mcur.nnoises() - 1);
		double delta = nrand<double>() * mcur.mnoises[ni].sd_valuechange;
		double nv = mcur.mnoises[ni].value + delta;
		bool isvalid = isinbounds(mcur.mnoises[ni].min, mcur.mnoises[ni].max, nv);
		if (isvalid == false) return false;

		//this function will change the per-window variance vector,
		//and recompute the misfit in an intelligent way
		set_misfit_noisechange(mpro, nv, ni);
		//loglr needs to take the normalising factor into account
		//because this changes when we change the variance of the distribution.
		//see my notes on noise changes for details.
		double loglr = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logu = log(urand<double>());
		if (logu < loglr) {
			chn.pnoisechange.inc_na();
			return true;
		}
		return false;

	}

	bool propose_independent(cChain& chn, rjMcMC1DModel& mpro)
	{
		const rjMcMC1DModel& mcur = chn.model;
		const double& temperature = chn.temperature;
		mpro = choosefromprior();
		set_misfit(mpro);

		size_t npro = mpro.nlayers() - 1;
		size_t ncur = mcur.nlayers() - 1;
		double a = pow(1.0 / (vmax - vmin), npro - ncur);
		double b = pow(1.0 / pmax, npro - ncur);
		double c = (double)factorial((unsigned int)npro) / (double)factorial((unsigned int)ncur);
		double priorratio = a * b * c;
		priorratio = 1.0;

		double logpriorratio = std::log(priorratio);
		double loglr = -(mpro.get_misfit() - mcur.get_misfit()) / 2.0 / temperature;
		double logar = logpriorratio + loglr;
		double logu = std::log(urand<double>());
		if (logu < logar) {
			return true;
		}
		return false;
	}

	rjMcMC1DModel choosefromprior()	{
		rjMcMC1DModel m;
		size_t nl = irand(nl_min, nl_max);
		m.initialise(pmax, vmin, vmax);
		for (size_t li = 0; li < nl; li++) {
			bool status = false;
			while (status == false) {
				double pos = urand(0.0, pmax);//position of interface
				double value = urand(vmin, vmax);//value
				status = m.insert_interface(pos, value);
			}
		}

		m.nuisances = nuisance_init;
		for (size_t di = 0; di < ndata; di++) {
			//noises are computed as a ratio against the observations
			m.nvar.push_back((err[di]*err[di])/(obs[di]*obs[di]));
		}
		//create the noise vector, sample the prior
		//and set the variance values to include
		//the sampled multiplicative noise.
		std::vector<rjMcMCNoise> noisevec(noisemag_sd.size());
		for (size_t ni = 0; ni < noisemag_sd.size(); ni++) {
			rjMcMCNoise mnoise;
			mnoise.min = noisemag_priorbounds[ni].first;
			mnoise.max = noisemag_priorbounds[ni].second;
			mnoise.data_bounds = noisemag_dbounds[ni];
			//sample
			mnoise.value = urand(mnoise.min, mnoise.max);
			mnoise.sd_valuechange = noisemag_sd[ni];
			m.mnoises.push_back(mnoise);

			for (size_t di = mnoise.data_bounds.first; di < mnoise.data_bounds.second; di++) {
				m.nvar[di] += mnoise.value * mnoise.value;
			}

		}

		//m.printmodel();

		return m;
	}

	std::vector<double> get_temperature_ladder() {
		std::vector<double> t = log10space(1.0, temperature_high, nchains());
		return t;
	}

	void reset()
	{
		starttime = "";
		endtime = "";
		samplingtime = 0;
		pmap.resettozero();
		nmap.resettozero();
		mnmap.resettozero();
		for (size_t ci = 0; ci < nchains(); ci++) {
			chains[ci] = cChain();
			chains[ci].swap_histogram.resize(nchains());
		}

	}

	void sample()
	{
		// for (size_t di = 0; di < ndata; di++) {
		// 	std::cout << err[di]<<" ";
		// }
		// std::cout << std::endl;
		starttime = timestamp();
		double t1 = gettime();
		BirthDeathFromPrior = false;

		std::vector<double> ladder = get_temperature_ladder();
		for (size_t ci = 0; ci < nchains(); ci++) {
			chains[ci].temperature = ladder[ci];
		}

		for (size_t si = 0; si < nsamples; si++) {
			for (size_t ci = 0; ci < nchains(); ci++) {
				cChain& chn = chains[ci];//Current chain
				rjMcMC1DModel& mcur = chn.model;//Current model on chain

				if (si == 0) {
					//Initialise chain
					mcur = choosefromprior();
					set_misfit(mcur);
				}
				else {
					rjMcMC1DModel mpro = mcur;
					size_t nopt = 4;
					//adaptive based on whether noise or nuisance inversion
					//is enabled.
					if (mcur.nnoises() > 0) nopt += 1;
					if (mcur.nnuisances() > 0) nopt += 1;

					size_t option = irand((size_t)0, nopt - 1);

					bool accept = false;
					switch (option) {
					case 0: accept = propose_valuechange(chn,mpro); break;
					case 1: accept = propose_move(chn,mpro); break;
					case 2: accept = propose_birth(chn,mpro); break;
					case 3: accept = propose_death(chn,mpro); break;
					case 4: {
						if (mcur.nnoises() > 0) {
							accept = propose_noisechange(chn,mpro);
						}
						else {
							accept = propose_nuisancechange(chn,mpro);
						}
						break;
						}
					case 5: accept = propose_nuisancechange(chn,mpro); break;
					case 6: accept = propose_independent(chn,mpro); break;
					default: glog.errormsg(_SRC_, "Proposal option %zu out of range\n", option);
					}

					// std::cout << &mcur << " " << &mpro << std::endl;
					if (accept) {
						mcur = mpro;
					}
				}

				if (chn.temperature == 1.0) {
					if(ci == 0 && si == 0){
						HighestLikelihood = mcur;
						LowestMisfit = mcur;
					}
					else {
						if (mcur.logppd() > HighestLikelihood.logppd()) {
							HighestLikelihood = mcur;
						}
						//if (mcur.get_misfit() < LowestMisfit.get_misfit()) {
						double cnmf  = standard_l2misfit(mcur);
						double lnmf = standard_l2misfit(LowestMisfit);
						if (cnmf < lnmf) {
							LowestMisfit = mcur;
						}
					}

					if (should_include_in_maps(si)) {
						pmap.addmodel(mcur);
						nmap.addmodel(mcur);
						mnmap.addmodel(mcur);
						ensemble.push_back(mcur);
					}
					if (verbose) {
						print_report(si, ci, chn.temperature, mcur);
					}
				}

				if (should_save_convergence_record(si)) {
					chn.history.models.push_back(mcur);
					chn.history.temperature.push_back((float)chn.temperature);
					chn.history.sample.push_back((uint32_t)si);
					chn.history.nlayers.push_back((uint32_t)mcur.nlayers());
					chn.history.misfit.push_back((float)mcur.get_chi2());
					chn.history.logppd.push_back((float)mcur.logppd());
					chn.history.ar_valuechange.push_back(chn.pvaluechange.ar());
					chn.history.ar_move.push_back(chn.pmove.ar());
					chn.history.ar_birth.push_back(chn.pbirth.ar());
					chn.history.ar_death.push_back(chn.pdeath.ar());
					chn.history.ar_nuisancechange.push_back(chn.pnuisancechange.ar());					
					if (mcur.nnoises() > 0) {
						chn.history.ar_noisechange.push_back(chn.pnoisechange.ar());
					}
				}				
			}

			//Parallel Tempering
			for (size_t i = nchains()-1; i >= 1; i--) {
				const size_t j = irand<size_t>(0,i);
				chains[i].swap_histogram[j]++;
				if (i != j) {
					propose_chain_swap(chains[i].temperature, chains[i].model, chains[j].temperature, chains[j].model);
				}
			}
		}
		double t2 = gettime();
		endtime = timestamp();
		samplingtime = t2 - t1;
	}

	static bool propose_chain_swap(double& Ti, rjMcMC1DModel& Mi, double& Tj, rjMcMC1DModel& Mj) {
		//double ar = std::exp((1.0 / Ti - 1.0 / Tj) * (Phii - Phij));
		double logar = (1.0 / Ti - 1.0 / Tj) * (Mi.get_misfit() - Mj.get_misfit());
		double logu  = std::log(urand<double>());
		if (logu < logar) {
			std::swap(Ti, Tj);
			//std::swap(Mi, Mj);
			return true;
		}
		return false;
	}

	void print_temperatures_misfits(const std::vector<double>& tladder, const std::vector<rjMcMC1DModel>& current_models){
		for (size_t i = 0; i < nchains(); i++) {
			std::cout << "\t" << tladder[i] << "\t" << current_models[i].get_misfit() << std::endl;
		}
	}

	void print_report(const size_t& si, const size_t& ci, const double& temperature, const rjMcMC1DModel& mcur) const
	{
		if (mpiRank == 0 && is_sample_reportable(si)) {			
			//printstats(si, ci, mcur.nlayers(), get_normalised_misfit1(mcur), temperature);
			double nmf = standard_l2misfit(mcur);
			printstats(si, ci, mcur.nlayers(), nmf, temperature);
			//mcur.printmodel();
			mcur.printmodelex1();
		}
	}

	bool is_sample_reportable(const size_t& si) const
	{
		if (si == 0 || si == nsamples - 1) return true;
		size_t k = (size_t)std::pow(10, std::floor(log10((double)si)));
		if (k > thinrate) k = thinrate;
		if (si % k == 0)return true;
		return false;
	}

	void printstats(const size_t& si, const size_t& ci, const size_t& np, const double& nmf, const double& temperature) const
	{
		std::cout <<
			" si="   << si <<
			" ci="  << ci <<	std::fixed <<
			" temp="<< std::setprecision(1) << temperature <<
			" np="  << std::setw(2) << np <<
			" nmf=" << std::setw(8) << std::setprecision(2) << nmf <<
			" vc="  << std::setprecision(2) << chains[ci].pvaluechange.ar() <<
			" mv="  << std::setprecision(2) << chains[ci].pmove.ar() <<
			" b="   << std::setprecision(2) << chains[ci].pbirth.ar() <<
			" d="   << std::setprecision(2) << chains[ci].pdeath.ar() <<
			" n="   << std::setprecision(2) << chains[ci].pnuisancechange.ar() << std::endl;
	}

	void writemapstofile_netcdf(NcFile& nc, const bool savechains=false) {

		NcGroupAtt a;
		a = nc.putAtt("ndata", NcType::nc_UINT, (unsigned int) ndata);
		a = nc.putAtt("value_parameterization", param_value.get_typestring());
		a = nc.putAtt("vmin", NcType::nc_DOUBLE, vmin);
		a = nc.putAtt("vmax", NcType::nc_DOUBLE, vmax);
		a = nc.putAtt("position_parameterization", param_position.get_typestring());
		a = nc.putAtt("pmin", NcType::nc_DOUBLE, 0.0);
		a = nc.putAtt("pmax", NcType::nc_DOUBLE, pmax);
		a = nc.putAtt("nlayers_min", NcType::nc_UINT, (unsigned int) nl_min);
		a = nc.putAtt("nlayers_max", NcType::nc_UINT, (unsigned int) nl_max);

		a = nc.putAtt("nsamples", NcType::nc_UINT, (unsigned int) nsamples);
		a = nc.putAtt("nchains", NcType::nc_UINT, (unsigned int) nchains());
		a = nc.putAtt("nburnin", NcType::nc_UINT, (unsigned int) nburnin);
		a = nc.putAtt("thinrate", NcType::nc_UINT, (unsigned int) thinrate);

		a = nc.putAtt("starttime", starttime);
		a = nc.putAtt("endtime", endtime);
		a = nc.putAtt("samplingtime", NcType::nc_DOUBLE, samplingtime);


		NcVar var;
		NcDim data_dim = nc.addDim("data", ndata);
		var = nc.addVar("observations", NcType::nc_DOUBLE, data_dim);
		var.putVar(obs.data());

		var = nc.addVar("errors", NcType::nc_DOUBLE, data_dim);
		var.putVar(err.data());

		NcDim np_dim = nc.addDim("depth", pmap.npbins());
		var = nc.addVar("depth", NcType::nc_DOUBLE, np_dim);
		var.putVar(pmap.pbin.data());

		NcDim nv_dim = nc.addDim("value", pmap.nvbins());
		var = nc.addVar("value", NcType::nc_DOUBLE, nv_dim);
		var.putVar(pmap.vbin.data());

		NcDim nl_dim = nc.addDim("layer", pmap.layercounts.size());
		var = nc.addVar("layer", NcType::nc_UINT, nl_dim);
		std::vector<unsigned int> lbin = increment(pmap.layercounts.size(), 1U, 1U);
		var.putVar(lbin.data());

		std::vector<NcDim> count_dims = { np_dim, nv_dim };
		var = nc.addVar("log10conductivity_histogram", NcType::nc_UINT, count_dims);
		var.putVar(pmap.counts.data());

		var = nc.addVar("interface_depth_histogram", NcType::nc_UINT, np_dim);
		var.putVar(pmap.cpcounts.data());

		var = nc.addVar("nlayers_histogram", NcType::nc_UINT, nl_dim);
		var.putVar(pmap.layercounts.data());

		//a = nc.putAtt("ar_valuechange", NcType::nc_FLOAT, pvaluechange.ar());
		//a = nc.putAtt("ar_move", NcType::nc_FLOAT, pmove.ar());
		//a = nc.putAtt("ar_birth", NcType::nc_FLOAT, pbirth.ar());
		//a = nc.putAtt("ar_death", NcType::nc_FLOAT, pdeath.ar());
		//a = nc.putAtt("ar_nuisancechange", NcType::nc_FLOAT, pnuisancechange.ar() );

		//Convergence Records
		NcDim chain_dim = nc.addDim("chain", nchains());
		var = nc.addVar("chain", NcType::nc_UINT, chain_dim);
		var.putVar(chains[0].history.sample.data());

		size_t ncvs = chains[0].history.sample.size();//Number of samples in the convergence record
		NcDim cvs_dim = nc.addDim("convergence_sample", ncvs);
		var = nc.addVar("convergence_sample", NcType::nc_UINT, cvs_dim);
		var.putVar(chains[0].history.sample.data());

		std::vector<unsigned int> chn = increment(nchains(), 1U, 1U);
		std::vector<NcDim> dims    = { chain_dim, cvs_dim };
		std::vector<NcDim> dchnchn = { chain_dim, chain_dim };
		
		for (size_t ci = 0; ci < nchains(); ci++) {
			cChain& chn = chains[ci];
			write_chain_variable(ci, chn.history.temperature, "temperature", NcType::nc_FLOAT, nc, dims);
			write_chain_variable(ci, chn.history.nlayers, "nlayers", NcType::nc_UINT, nc, dims);
			write_chain_variable(ci, chn.history.misfit, "misfit", NcType::nc_FLOAT, nc, dims);
			write_chain_variable(ci, chn.history.logppd, "logppd", NcType::nc_FLOAT, nc, dims);
			write_chain_variable(ci, chn.history.ar_valuechange, "ar_valuechange", NcType::nc_FLOAT, nc, dims);
			write_chain_variable(ci, chn.history.ar_move, "ar_move", NcType::nc_FLOAT, nc, dims);
			write_chain_variable(ci, chn.history.ar_birth, "ar_birth", NcType::nc_FLOAT, nc, dims);
			write_chain_variable(ci, chn.history.ar_death, "ar_death", NcType::nc_FLOAT, nc, dims);
			if (chn.model.nnuisances() > 0) {
				write_chain_variable(ci, chn.history.ar_nuisancechange, "ar_nuisancechange", NcType::nc_FLOAT, nc, dims);
			}
			if (chn.model.nnoises() > 0) {
				write_chain_variable(ci, chn.history.ar_noisechange, "ar_noisechange", NcType::nc_FLOAT, nc, dims);
			}
			write_chain_variable(ci, chn.swap_histogram, "swap_histogram", NcType::nc_UINT, nc, dchnchn);

			//bookmark
			if (savechains){
				std::vector<NcDim> dims_predicted = { chain_dim, cvs_dim, data_dim };
				std::vector<NcDim> dims_partition = { chain_dim, cvs_dim, nl_dim };
				write_chain_partitions(nc, ci, chn.history.models, dims_partition);
				write_chain_predicted(nc, ci, chn.history.models, dims_predicted);
			}
		}

		rjMcMC1DPPDMap::cSummaryModels s = pmap.get_summary_models();
		var = nc.addVar("mean_model", NcType::nc_FLOAT, np_dim);
		var.putVar(s.mean.data());

		var = nc.addVar("mode_model", NcType::nc_FLOAT, np_dim);
		var.putVar(s.mode.data());

		var = nc.addVar("p10_model", NcType::nc_FLOAT, np_dim);
		var.putVar(s.p10.data());

		var = nc.addVar("p50_model", NcType::nc_FLOAT, np_dim);
		var.putVar(s.p50.data());

		var = nc.addVar("p90_model", NcType::nc_FLOAT, np_dim);
		var.putVar(s.p90.data());

		// nuisance map write
		if (nmap.get_nnuisances() > 0) {
			//noise hist is stored as 2D,
			//(histogram bins * number of noises).
			//histogram bin number is constant between noises to make this easier.
			NcDim nuisance_dim = nc.addDim("nuisance",nmap.get_nnuisances());
			NcDim nuisance_bin_dim = nc.addDim("nuisance_bin",NUM_NUISANCE_HISTOGRAM_BINS);

			std::vector<NcDim> nuisance_dims;
			nuisance_dims.push_back(nuisance_dim);
			nuisance_dims.push_back(nuisance_bin_dim);

			NcVar binsvar = nc.addVar("nuisance_bins",NcType::nc_DOUBLE,nuisance_dims);
			NcVar histvar = nc.addVar("nuisance_histogram",NcType::nc_UINT,nuisance_dims);

			NcVar typevar = nc.addVar("nuisance_types",NcType::nc_STRING,nuisance_dim);
			typevar.putVar(nmap.get_types().data());

			std::vector<size_t> startp, countp;
			startp.push_back(0);
			startp.push_back(0);
			countp.push_back(1);
			countp.push_back(NUM_NUISANCE_HISTOGRAM_BINS);
			for (size_t ni = 0; ni < nmap.get_nnuisances(); ni++) {
				//save a separate histogram for each noise process
				startp[0] = ni;
				cStats<double> s(nmap.nuisance[ni]);
				cHistogram<double, uint32_t> hist(nmap.nuisance[ni],s.min,s.max,NUM_NUISANCE_HISTOGRAM_BINS);
				binsvar.putVar(startp,countp,hist.centre.data());
				histvar.putVar(startp,countp,hist.count.data());
			}
		}

		if (mnmap.get_nnoises() > 0) {
			//noise hist is stored as 2D,
			//(histogram bins * number of noises).
			//histogram bin number is constant between noises to make this easier.
			NcDim noises_dim = nc.addDim("noise",mnmap.get_nnoises());
			NcDim noise_bin_dim = nc.addDim("noise_bin",NUM_NOISE_HISTOGRAM_BINS);

			std::vector<NcDim> noise_dims;
			noise_dims.push_back(noises_dim);
			noise_dims.push_back(noise_bin_dim);

			NcVar binsvar = nc.addVar("noise_bins",NcType::nc_DOUBLE,noise_dims);
			NcVar histvar = nc.addVar("noise_histogram",NcType::nc_UINT,noise_dims);

			std::vector<size_t> startp, countp;
			startp.push_back(0);
			startp.push_back(0);
			countp.push_back(1);
			countp.push_back(NUM_NOISE_HISTOGRAM_BINS);
			for (size_t ni = 0; ni < mnmap.get_nnoises(); ni++) {
				//save a separate histogram for each noise process
				startp[0] = ni;
				cStats<double> s(mnmap.noises[ni]);
				cHistogram<double, uint32_t> hist(mnmap.noises[ni],s.min,s.max,NUM_NOISE_HISTOGRAM_BINS);
				binsvar.putVar(startp,countp,hist.centre.data());
				histvar.putVar(startp,countp,hist.count.data());
			}
		}
	}

	template<typename T>
	void write_chain_variable(const size_t& ci, std::vector<T>& data, const std::string& name, const NcType& nctype, NcFile& nc, const std::vector<NcDim>& dims)
	{
		std::vector<size_t> startp = { ci, 0 };
		std::vector<size_t> countp = { 1, data.size() };
		NcVar var;
		if (ci == 0) {
			var = nc.addVar(name, nctype, dims);
		}
		else{
			var = nc.getVar(name);
		}
		var.putVar(startp, countp, data.data());
	}

	void write_chain_partitions(NcFile& nc, const size_t& ci, const std::vector<rjMcMC1DModel>& models, std::vector<NcDim> dims)
	{
		//bookmark		
		const std::string ptop_name = "layer_depth_top";
		const std::string val_name = "layer_value";

		NcType nctype = NC_FLOAT;
		NcVar ptop_var,val_var;
		if (ci == 0) {
			ptop_var = nc.addVar(ptop_name, nctype, dims);
			val_var = nc.addVar(val_name, nctype, dims);
		}
		else {
			ptop_var = nc.getVar(ptop_name);
			val_var = nc.getVar(val_name);
		}

		std::vector<float> ptop(nl_max);
		std::vector<float> value(nl_max);
		

		std::vector<size_t> startp = { ci, 0, 0 };
		std::vector<size_t> countp = { 1, 1, nl_max};
		for (size_t mi = 0; mi < models.size(); mi++) {
			const rjMcMC1DModel& m = models[mi];
			for (size_t li = 0; li < m.layers.size(); li++) {
				ptop[li]  = (float)m.layers[li].ptop;
				value[li] = (float)m.layers[li].value;
			}
			for (size_t li = m.layers.size(); li < nl_max; li++) {
				ptop[li]  = NC_FILL_FLOAT;
				value[li] = NC_FILL_FLOAT;
			}

			startp[1] = mi;
			ptop_var.putVar(startp, countp, ptop.data());
			val_var.putVar(startp, countp, value.data());
		}
	}

	void write_chain_predicted(NcFile& nc, const size_t& ci, const std::vector<rjMcMC1DModel>& models, std::vector<NcDim> dims)
	{
		//bookmark		
		const std::string name = "predicted";
		NcType nctype = NC_FLOAT;		
		
		
		NcVar var;
		if (ci == 0) {
			var = nc.addVar(name, nctype, dims);
		}
		else {
			var = nc.getVar(name);
		}
		
		std::vector<size_t> startp = { ci, 0, 0 };
		std::vector<size_t> countp = { 1, 1, models[0].get_predicted().size() };
		for (size_t mi = 0; mi < models.size(); mi++) {
			startp[1] = mi;			
			var.putVar(startp, countp, models[mi].get_predicted().data());
		}
	}

};


#endif
