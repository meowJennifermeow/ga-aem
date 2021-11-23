/*
This source code file is licensed under the GNU GPL Version 2.0 Licence by the following copyright holder:
Crown Copyright Commonwealth of Australia (Geoscience Australia) 2015.
The GNU GPL 2.0 licence is available at: http://www.gnu.org/licenses/gpl-2.0.html. If you require a paper copy of the GNU GPL 2.0 Licence, please write to Free Software Foundation, Inc. 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

Author: Ross C. Brodie, Geoscience Australia.
*/

#ifndef _csbsinverter_H
#define _csbsinverter_H

#include <stdio.h>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <variant>

#include "general_types.h"
#include "string_utils.h"
#include "vector_utils.h"

#include "airborne_types.h"
#include "cinverter.h"
#include "tdemsystem.h"
#include "tdemsysteminfo.h"
#include "samplebunch.h"

class cGeomStruct {

public:
	cTDEmGeometry input;
	cTDEmGeometry ref;
	cTDEmGeometry std;
	cTDEmGeometry min;
	cTDEmGeometry max;
	cTDEmGeometry tfr;
	cTDEmGeometry invmodel;
};

class cEarthStruct {

public:	
	cEarth1D ref;
	cEarth1D std;
	cEarth1D min;
	cEarth1D max;
	cEarth1D invmodel;

	void sanity_check() {

		size_t nc = ref.conductivity.size();
		size_t nt = ref.thickness.size();

		std::ostringstream oss;
		if (nc != nt + 1) {
			oss << "The conductivity and/or thickness do not have the correct number of layers\n";
		}

		if (ref.conductivity.size() > 0) {
			if (::min(ref.conductivity) <= 0) oss << "The conductivity ref is <= 0 in at least one layer\n";
		}	

		if (std.conductivity.size() > 0) {
			if (::min(std.conductivity) <= 0) oss << "The conductivity std is <= 0\n";
		}

		if (min.conductivity.size() > 0) {		
			if (min.conductivity.size() != nc) oss << "The conductivity min does not have the correct number of layer\n";
			if (max.conductivity.size() != nc) oss << "The conductivity max does not have the correct number of layer\n";
			if (::min(min.conductivity) <= 0) oss << "The conductivity min is <= 0 in at least one layer in at least one layer\n";
			if (::min(max.conductivity) <= 0) oss << "The conductivity max is <= 0 in at least one layer in at least one layer\n";
			if (::min(max.conductivity - min.conductivity) <= 0) oss << "The conductivity max <= min in at least one layer\n";
			if (::min(ref.conductivity - min.conductivity) <= 0) oss << "The conductivity ref <= min in at least one layer\n";
			if (::min(max.conductivity - ref.conductivity) <= 0) oss << "The conductivity ref >= max in at least one layer\n";
		}

		if (ref.thickness.size() > 0) {
			if (::min(ref.thickness) <= 0) oss << "The thickness ref is <= 0 in at least one layer\n";
		}

		if (std.thickness.size() > 0) {
			if (::min(std.thickness) <= 0) oss << "The thickness std is <= 0 in at least one layer\n";
		}

		if (min.thickness.size() > 0) {
			if (min.thickness.size() != nc) oss << "The thickness min does not have the correct number of layer\n";
			if (max.thickness.size() != nc) oss << "The thickness max does not have the correct number of layer\n";
			if (::min(min.thickness) <= 0) oss << "The thickness min is <= 0 in at least one layer\n";
			if (::min(max.thickness) <= 0) oss << "The thickness max is <= 0 in at least one layer\n";
			if (::min(max.thickness - min.thickness) <= 0) oss << "The thickness max <= min in at least one layer\n";
			if (::min(ref.thickness - min.thickness) <= 0) oss << "The thickness ref <= min in at least one layer\n";
			if (::min(max.thickness - ref.thickness) <= 0) oss << "The thickness ref >= max in at least one layer\n";
		}

		if (oss.str().size() > 0) {
			glog.errormsg(oss.str());
		}

	}
	
};

class cOutputOptions {

private:
	std::string DumpBasePath;

public:	
	std::string LogFile;
	bool PositiveLayerTopDepths = false;
	bool NegativeLayerTopDepths = false;
	bool PositiveLayerBottomDepths = false;
	bool NegativeLayerBottomDepths = false;
	bool InterfaceElevations = false;
	bool ParameterSensitivity = false;
	bool ParameterUncertainty = false;
	bool ObservedData = false;
	bool NoiseEstimates = false;
	bool PredictedData = false;
	bool Dump = false;
	std::string DumpPath(const size_t datafilerecord, const size_t iteration) const
	{
		return DumpBasePath + pathseparatorstring() + 
			strprint("si%07d", (int)datafilerecord) + pathseparatorstring() +
			strprint("it%03d", (int)iteration) + pathseparatorstring();
	};

	cOutputOptions(){};
	cOutputOptions(const cBlock& b) {		
		LogFile = b.getstringvalue("LogFile");				
		fixseparator(LogFile);

		PositiveLayerTopDepths = b.getboolvalue("PositiveLayerTopDepths");
		NegativeLayerTopDepths = b.getboolvalue("NegativeLayerTopDepths");
		PositiveLayerBottomDepths = b.getboolvalue("PositiveLayerBottomDepths");
		NegativeLayerBottomDepths = b.getboolvalue("NegativeLayerBottomDepths");
		InterfaceElevations = b.getboolvalue("InterfaceElevations");
		ParameterSensitivity = b.getboolvalue("ParameterSensitivity");
		ParameterUncertainty = b.getboolvalue("ParameterUncertainty");
		ObservedData = b.getboolvalue("ObservedData");
		NoiseEstimates = b.getboolvalue("NoiseEstimates");
		PredictedData = b.getboolvalue("PredictedData");				

		Dump = b.getboolvalue("Dump");
		if (Dump) {
			DumpBasePath = b.getstringvalue("DumpPath");
			fixseparator(DumpBasePath);
			if (DumpBasePath[DumpBasePath.length() - 1] != pathseparator()) {
				DumpBasePath.append(pathseparatorstring());
			}
			makedirectorydeep(DumpBasePath.c_str());
		}
	}	
};

class cSBSInverter : public cInverter {

	using cIFDMap = cKeyVec<std::string, cInvertibleFieldDefinition, caseinsensetiveequal<std::string>>;
	const size_t XCOMP = 0;
	const size_t YCOMP = 1;
	const size_t ZCOMP = 2;
	const size_t XZAMP = 3;
	std::vector<std::vector<std::vector<std::vector<int>>>> _dindex_;

	int    BeginGeometrySolveIteration = 0;
	bool   FreeGeometry = false;
	
	Matrix Wc;
	Matrix Wt;
	Matrix Wg;
	Matrix Wr;
	Matrix Ws;
	Matrix Wq;

	double AlphaC = 0.0;
	double AlphaT = 0.0;
	double AlphaG = 0.0;
	double AlphaS = 0.0;
	double AlphaQ = 0.0;

	size_t nSoundings = 0;
	size_t nBunchSubsample = 0;	
	size_t nDataPerSounding = 0;
	size_t nAllData = 0;			
	size_t nLayers = 0;
	size_t nParamPerSounding = 0;
	size_t nGeomParamPerSounding = 0;	
	size_t cOffset = 0;//Offset within sample of conductivity parameters
	size_t tOffset = 0;//Offset within sample of thickness parameters
	size_t gOffset = 0;//Offset within sample of geometry parameters

	size_t nSystems = 0;
	size_t pointsoutput = 0;
	std::vector<cGeomStruct> G;
	std::vector<cEarthStruct> E;	
	cOutputOptions OO;
	std::vector<cTDEmSystemInfo> SV;

	//Column definitions		
	cInvertibleFieldDefinition fdC;
	cInvertibleFieldDefinition fdT;
	cIFDMap fdG;

	//Sample instances
	struct SampleId {
		int uniqueid = -1;
		int survey = -1;
		int date = -1;
		int flight = -1;
		int line = -1;
		double fiducial = -1.0;
		double x = -1.0;
		double y = -1.0;
		double elevation = 0.0;
	};
	std::vector<SampleId> Id;
	cKeyVec<std::string, cFdVrnt, caseinsensetiveequal<std::string>> AncFld;
	
private:	
	
	Vector cull(const Vector& vall) const {
		assert(ActiveData.size() == nData);
		assert(vall.size() == nAllData);
		Vector vcull(nData);
		for (size_t i = 0; i < nData; i++) {
			vcull[i] = vall[ActiveData[i]];
		}
		return vcull;
	}

	Vector cull(const std::vector<double>& vall) const {
		assert(ActiveData.size() == nData);
		assert(vall.size() == nAllData);
		Vector vcull(nAllData);
		for (size_t i = 0; i < nData; i++) {
			vcull[i] = vall[ActiveData[i]];
		}
		return vcull;
	}

	Matrix cull(const Matrix& mall) const {
		assert(ActiveData.size() == nData);
		assert(mall.rows() == nAllData);
		assert(mall.cols() == nParam);
		Matrix mcull(nData, nParam);
		for (size_t i = 0; i < nData; i++) {
			mcull.row(i) = mall.row(ActiveData[i]);
		}
		return mcull;
	}

public:			
	
	cSBSInverter(const std::string& controlfile, const int& size, const int& rank, const bool& usingopenmp, const std::string commandline) 
					: cInverter(controlfile, size, rank, usingopenmp, commandline)
	{
		std::cout << "Constructing cSBSInverter\n";
		_GSTPUSH_
		try {			
			initialise(controlfile);
		}
		catch (const std::string& msg) {
			std::cerr << msg;
			glog.logmsg(msg);
		}
		catch (const std::runtime_error& e) {
			std::cerr << e.what();
			glog.logmsg(std::string(e.what()));
		}
		catch (const std::exception& e) {
			std::cerr << e.what();
			glog.logmsg(std::string(e.what()));
		}
	};

	~cSBSInverter() {
		//std::cout << "Destroying cSBSInverter\n";
	}
			
	void loadcontrolfile(const std::string& filename)
	{		
		glog.logmsg(0, "Loading control file %s\n", filename.c_str());
		Control = cBlock(filename);
		cBlock ob = Control.findblock("Output");
		cBlock ib = Control.findblock("Input");

		OO = cOutputOptions(ob);
		Verbose = ob.getboolvalue("verbose");

		std::string suffix = stringvalue(Rank, ".%04d");
		OO.LogFile = insert_after_filename(OO.LogFile, suffix);
		openlogfile(); //load this first to get outputlogfile opened

		//Load control file
		parseoptions();
		initialise_systems();


		if (cInputManager::isnetcdf(ib)) {
#if !defined HAVE_NETCDF
			glog.errormsg(_SRC_, "Sorry NETCDF I/O is not available in this executable\n");
#endif			
			IM = std::make_unique<cNetCDFInputManager>(ib);
			std::string s = IM->datafilename();
		}
		else {
			IM = std::make_unique<cASCIIInputManager>(ib);
		}

		if (cOutputManager::isnetcdf(ob)) {
#if !defined HAVE_NETCDF
			glog.errormsg(_SRC_, "Sorry NETCDF I/O is not available in this executable\n");
#endif			
			OM = std::make_unique<cNetCDFOutputManager>(ob, Size, Rank);
		}
		else {
			OM = std::make_unique<cASCIIOutputManager>(ob, Size, Rank);
		}
		OM->opendatafile(IM->datafilename(), IM->subsamplerate());
	}

	bool solve_thickness() const
	{
		return fdT.solve;
	};
	
	bool solve_conductivity() const {
		return fdC.solve;
	};
	
	bool solve_geometry_element(const std::string& e) {						
		return fdG.cref(e).solve;
	};
	
	bool solve_geometry() const {		
		if (nGeomParamPerSounding > 0) return true;
		return true;
	};
	
	std::string bunch_id() {
		const size_t si = Bunch.master_index();
		const size_t& record = Bunch.master_record();
		std::ostringstream s;
		s << "Rec " << ixd(6) << 1 + record;
		s << " Fl " << ixd(3) << Id[si].flight;
		s << " Ln " << ixd(7) << Id[si].line;
		s << " Fd " << fxd(10, 2) << Id[si].fiducial;
		return s.str();
	}

	std::string bunch_result(const double& etime) {
		std::ostringstream s;
		s << " Its=" << ixd(3) << CIS.iteration;
		s << " Phid=" << fxd(6, 2) << CIS.phid;
		s << " Time=" << fxd(4, 1) << etime;
		s << " " << TerminationReason;
		s << " " << OutputMessage;
		s << " nF= " << nForwards / CIS.iteration;
		s << " nJ= " << nJacobians;
		return s.str();
	}

	std::string dumppath() const
	{
		const size_t& record  = Bunch.master_record();
		std::string s = OO.DumpPath(record, CIS.iteration);
		return s;
	};

	void dump_record_number() {
		const size_t& record = Bunch.master_record();
		std::ofstream of(dumppath() + "record.dat");
		of << "Record\t" << record << std::endl;
	}

	const int cindex(const size_t& si, const size_t& li) {
		if (solve_conductivity() == false) {
			glog.errormsg("Out of boundes in cindex()\n");
		}
		return (int) (si*nParamPerSounding + cOffset + li);
	}

	const int tindex(const size_t& si, const size_t& li) {
		if (solve_thickness() == false) {
			glog.errormsg("Out of boundes in tindex()\n");
		}		
		return (int) (si*nParamPerSounding + tOffset + li);
	}
	
	const int gindex(const size_t& si, const std::string& gname) {
		if (solve_geometry() == false) {
			glog.errormsg("Out of boundes in gindex\n");
		}
		const int& goff = fdG.cref(gname).offset;
		if (goff < 0) return -1;
		return (int)(si*nParamPerSounding + goff);
	}

	const int gindex(const size_t& si, const size_t& gi) {
		if (solve_geometry() == false) {
			glog.errormsg("Out of boundes in gindex\n");
		}
		int goff = fdG[gi].second.offset;
		if (goff < 0) return -1;
		return (int)(si*nParamPerSounding + goff);
	}
	
	void openlogfile()
	{		
		glog.logmsg(0, "Opening log file %s\n", OO.LogFile.c_str());
		glog.open(OO.LogFile);
		glog.logmsg(0, "%s\n", CommandLine.c_str());
		glog.logmsg(0, "%s\n", versionstring(GAAEM_VERSION, __TIME__, __DATE__).c_str());				
		glog.logmsg(0, "Working directory %s\n", getcurrentdirectory().c_str());
		if (UsingOpenMP && Size > 1) {
			glog.logmsg(0, "Using OpenMP threading Processes=%d\tRank=%d\n", Size, Rank);
		}
		else if (Size>1) {
			glog.logmsg(0, "Using MPI Processes=%d\tRank=%d\n", Size, Rank);
		}
		else {
			glog.logmsg(0, "Standalone Processes=%d\tRank=%d\n", Size, Rank);
		}

		glog.logmsg(0, "Control file %s\n", Control.Filename.c_str());
		glog.log(Control.get_as_string());
		glog.flush();
	}
	
	void parseoptions()
	{
		cBlock b = Control.findblock("Options");		
		if (b.getvalue("SoundingsPerBunch", nSoundings)==false) {
			nSoundings = 1;
		}

		if (b.getvalue("BunchSubsample", nBunchSubsample) == false) {
			nBunchSubsample = 1;
		}

		AlphaC = b.getdoublevalue("AlphaConductivity");
		AlphaT = b.getdoublevalue("AlphaThickness");
		AlphaG = b.getdoublevalue("AlphaGeometry");		
		AlphaS = b.getdoublevalue("AlphaSmoothness");
		AlphaQ = b.getdoublevalue("AlphaHomogeneous");
		
		BeginGeometrySolveIteration = b.getintvalue("BeginGeometrySolveIteration");						
		if (!isdefined(BeginGeometrySolveIteration)) {
			BeginGeometrySolveIteration = 0;
		}

		NormType = eNormType::L2;//default
		std::string nt = b.getstringvalue("NormType");
		if (!isdefined(nt)) {
			NormType = eNormType::L2;
		}
		else if (strcasecmp(nt, "L1") == 0) {
			NormType = eNormType::L1;
		}
		else if (strcasecmp(nt, "L2") == 0) {
			NormType = eNormType::L2;
		}
		else {
			glog.errormsg("Unknown NormType %s\n", nt.c_str());
		}


		SmoothnessMethod = eSmoothnessMethod::DERIVATIVE_2ND;//default
		std::string sm = b.getstringvalue("SmoothnessMethod");
		if (!isdefined(sm)) {
			SmoothnessMethod = eSmoothnessMethod::DERIVATIVE_2ND;
		}
		else if (strcasecmp(sm, "Minimise1stDerivatives") == 0) {
			SmoothnessMethod = eSmoothnessMethod::DERIVATIVE_1ST;
		}
		else if (strcasecmp(sm, "Minimize1stDerivatives") == 0) {
			SmoothnessMethod = eSmoothnessMethod::DERIVATIVE_1ST;
		}
		else if (strcasecmp(sm, "Minimise2ndDerivatives") == 0) {
			SmoothnessMethod = eSmoothnessMethod::DERIVATIVE_2ND;
		}
		else if (strcasecmp(sm, "Minimize2ndDerivatives") == 0) {
			SmoothnessMethod = eSmoothnessMethod::DERIVATIVE_2ND;
		}
		else {
			glog.errormsg(_SRC_, "Unknown SmoothnessMethod %s\n", sm.c_str());
		}
		MaxIterations = b.getsizetvalue("MaximumIterations");
		MinimumPhiD = b.getdoublevalue("MinimumPhiD");
		MinimumImprovement = b.getdoublevalue("MinimumPercentageImprovement");
	}
	
	void set_field_definitions()
	{
		cBlock b = Control.findblock("Input.AncillaryFields");
		set_field_definitions_ancillary(b);
		if (AncFld.keyindex("line") < 0) {
			glog.errormsg("Must specify a linenumber field\n");
		}

		b = Control.findblock("Input.Geometry");
		fdG = set_field_definitions_geometry(b);

		b = Control.findblock("Input.Earth");
		fdC = cInvertibleFieldDefinition(b, "Conductivity");
		fdT = cInvertibleFieldDefinition(b, "Thickness");
	}

	void set_field_definitions_ancillary(const cBlock& parent) {
		const cBlock& b = parent;
		for (size_t i = 0; i < b.Entries.size(); i++) {
			std::string key   = b.key(i);
			std::string value = b.value(i);
			cFieldDefinition fd(parent, key);						
			cFdVrnt fdvrnt(fd,cVrnt());						
			IM->set_variant_type(fd.varname, fdvrnt.vnt);
			AncFld.add(key, fdvrnt);
		}		
	}
	
	cIFDMap set_field_definitions_geometry(const cBlock& parent)
	{				
		cIFDMap g;
		for (size_t i = 0; i < cTDEmGeometry::size(); i++) {			
			std::string key = cTDEmGeometry::element_name(i);
			cInvertibleFieldDefinition f(parent, key);
			bool a = g.add(key, f);
			if (a == false) {
				std::string msg = strprint("Parameter %s has already been already added\n", key.c_str());
				glog.errormsg(msg);
			}
		}
		return g;
	}
	
	void setup_parameters()
	{
		Id.resize(nSoundings);
		E.resize(nSoundings);
		G.resize(nSoundings);

		bool status = Control.getvalue("Input.Earth.Conductivity.NumberOfLayers",nLayers);
		if (status == false) {
			std::stringstream msg;
			msg << "The NumberOfLayers must be specified in Input.Columns.Conductivity\n";
			glog.errormsg(msg.str());
		}

		nParamPerSounding = 0;
		nGeomParamPerSounding = 0;
		cOffset = 0;
		tOffset = 0;
		gOffset = 0;		

		if (solve_conductivity()) {			
			fdC.offset = 0;
			tOffset += nLayers;
			gOffset += nLayers;
			nParamPerSounding += nLayers;
		}

		if (solve_thickness()) {
			fdT.offset = (int)tOffset;
			gOffset += nLayers-1;
			nParamPerSounding += nLayers-1;
		}

		//Geometry params			
		for (size_t gi = 0; gi < cTDEmGeometry::size(); gi++) {
			std::string gname = cTDEmGeometry::element_name(gi);
			cInvertibleFieldDefinition& g = fdG.cref(gname);
			if (g.solve) {
				g.offset = (int)nParamPerSounding;
				nGeomParamPerSounding++;
				nParamPerSounding++;
			}
			else {
				g.offset = -1;
			}
		}		
		nParam = nParamPerSounding*nSoundings;				
		RefParam.resize(nParam);
		RefParamStd.resize(nParam);
	}
	
	void initialise_Wc() {
		Wc = Matrix::Zero(nParam, nParam);
		if (solve_conductivity() == false)return;

		for (size_t si = 0; si < nSoundings; si++) {
			const cEarthStruct& e = E[si];
			std::vector<double> t(nLayers);
			if (nLayers == 1) {
				t[0] = 1;
			}
			else if (nLayers == 2) {
				t[0] = e.ref.thickness[0];
				t[1] = e.ref.thickness[0];
			}
			else {
				for (size_t i = 0; i < (nLayers - 1); i++) {
					t[i] = e.ref.thickness[i];
				}
				t[nLayers - 1] = (t[nLayers - 2] / t[nLayers - 3]) * t[nLayers - 2];
			}

			double tsum = 0.0;
			for (size_t li = 0; li < nLayers; li++)tsum += t[li];
			double tavg = tsum / (double)nLayers;

			double s = AlphaC / (double)(nLayers * nSoundings);
			for (size_t li = 0; li < nLayers; li++) {
				int p = cindex(si, li);
				Wc(p, p) = s * (t[li] / tavg) / (RefParamStd[p] * RefParamStd[p]);
			}
		}
	}

	void initialise_Wt() {
		Wt = Matrix::Zero(nParam, nParam);
		if (solve_thickness() == false)return;

		const double s = AlphaT / (double)((nLayers - 1) * nSoundings);
		for (size_t si = 0; si < nSoundings; si++) {
			for (size_t li = 0; li < nLayers - 1; li++) {
				const int pi = tindex(si, li);
				Wt(pi, pi) = s / (RefParamStd[pi] * RefParamStd[pi]);
			}
		}
	}

	void initialise_Wg() {
		Wg = Matrix::Zero(nParam, nParam);
		if (nGeomParamPerSounding <= 0)return;

		double s = AlphaG / (double)(nGeomParamPerSounding * nSoundings);
		for (size_t si = 0; si < nSoundings; si++) {
			for (size_t gi = 0; gi < cTDEmGeometry::size(); gi++) {
				const int pi = gindex(si, gi);
				if (pi >= 0) {
					Wg(pi, pi) = s / (RefParamStd[pi] * RefParamStd[pi]);
				}
			}
		}
	}

	void initialise_L_Ws_1st_derivative()
	{
		Ws = Matrix::Zero(nParam, nParam);
		if (AlphaS == 0 || nLayers < 3) return;
		if (solve_conductivity() == false) return;

		Matrix L = Matrix::Zero(nSoundings * (nLayers - 1), nParam);
		size_t nrows = 0;
		for (size_t si = 0; si < nSoundings; si++) {
			const cEarthStruct& e = E[si];
			std::vector<double> t = e.ref.dummy_thickness();
			double tavg = mean(t);
			for (size_t li = 1; li < nLayers; li++) {
				const int pi0 = cindex(si, li - 1);
				const int pi1 = cindex(si, li);
				double t1 = t[li - 1];
				double t2 = t[li];
				double d12 = (t1 + t2) / 2.0;
				double s = sqrt(t2 / tavg);//sqrt because it gets squared in L'L		
				L(nrows, pi0) = -s / d12;
				L(nrows, pi1) = s / d12;
				nrows++;
			}
		}
		Ws = L.transpose() * L;
		Ws *= (AlphaS / (double)(nrows));
	}

	void initialise_L_Ws_2nd_derivative()
	{
		Ws = Matrix::Zero(nParam, nParam);
		if (AlphaS == 0 || nLayers < 3) return;
		if (solve_conductivity() == false) return;

		Matrix L = Matrix::Zero(nSoundings * (nLayers - 2), nParam);
		size_t nrows = 0;
		for (size_t si = 0; si < nSoundings; si++) {
			const cEarthStruct& e = E[si];
			std::vector<double> t = e.ref.dummy_thickness();
			double tavg = mean(t);
			for (size_t li = 1; li < nLayers - 1; li++) {
				const int pi0 = cindex(si, li - 1);
				const int pi1 = cindex(si, li);
				const int pi2 = cindex(si, li + 1);
				double t1 = t[li - 1];
				double t2 = t[li];
				double t3 = t[li + 1];
				double d12 = (t1 + t2) / 2.0;
				double d23 = (t2 + t3) / 2.0;
				double s = sqrt(t2 / tavg);//sqrt because it gets squared in L'L		
				L(nrows, pi0) = s / d12;
				L(nrows, pi1) = -s / d12 - s / d23;
				L(nrows, pi2) = s / d23;
				nrows++;
			}
		}
		Ws = L.transpose() * L;
		Ws *= (AlphaS / (double)(nrows));
	}

	void initialise_Ws() {
		if (SmoothnessMethod == eSmoothnessMethod::DERIVATIVE_1ST) {
			initialise_L_Ws_1st_derivative();
		}
		else if (SmoothnessMethod == eSmoothnessMethod::DERIVATIVE_2ND) {
			initialise_L_Ws_2nd_derivative();
		}
	}

	void initialise_Wq()
	{
		Wq = Matrix::Zero(nParam, nParam);
		if (AlphaQ == 0) return;
		if (solve_conductivity() == false) return;
		Matrix L = Matrix::Zero(nLayers * nSoundings, nParam);

		size_t nrows = 0;
		for (size_t si = 0; si < nSoundings; si++) {
			const cEarthStruct& e = E[si];
			std::vector<double> t = e.ref.dummy_thickness();
			double tavg = mean(t);

			//Loop over constraints equations
			for (size_t li = 0; li < nLayers; li++) {
				const int lpindex = cindex(si, li);

				//Loop over layers for this equation
				for (size_t ki = 0; ki < nLayers; ki++) {
					const int pindex = cindex(si, ki);
					double s = std::sqrt(t[li] / tavg);//sqrt because it gets squared in L'L				
					if (lpindex == pindex) {
						L(nrows, pindex) = 1.0;
					}
					else {
						L(nrows, pindex) = -1.0 / ((double)nLayers - 1);
					}
				}
				nrows++;
			}
		}
		Wq = L.transpose() * L;
		Wq *= (AlphaQ / (double)(nrows));
		//std::cerr << Wq;
	}

	void initialise_Wr() {
		initialise_Wc();
		initialise_Wt();
		initialise_Wg();

		Wr = Matrix::Zero(nParam, nParam);
		if (AlphaC > 0.0) Wr += Wc;
		if (AlphaT > 0.0) Wr += Wt;
		if (AlphaG > 0.0) Wr += Wg;
	}

	void initialise_Wm() {
		initialise_Wq();
		initialise_Ws();
		initialise_Wr();
		Wm = Wr + Ws + Wq;
	}

	void dump_W_matrices() {
		if (OO.Dump) {
			writetofile(Wc, dumppath() + "Wc.dat");
			writetofile(Wt, dumppath() + "Wt.dat");
			writetofile(Wg, dumppath() + "Wg.dat");
			writetofile(Wr, dumppath() + "Wr.dat");
			writetofile(Ws, dumppath() + "Ws.dat");
			writetofile(Wm, dumppath() + "Wm.dat");
			writetofile(Wd, dumppath() + "Wd.dat");
		}
	}

	const int& dindex(const size_t& sampleindex, const size_t& systemindex, const size_t& componentindex, const size_t& windowindex) {
		const size_t& si = sampleindex;
		const size_t& sysi = systemindex;		
		const size_t& ci = componentindex;
		const size_t& wi = windowindex;	
		return _dindex_[si][sysi][ci][wi];
	};

	void initialise_systems()
	{
		set_fftw_lock();
		std::vector<cBlock> B = Control.findblocks("EMSystem");
		nSystems = B.size();
		SV.resize(nSystems);
		for (size_t sysi = 0; sysi < nSystems; sysi++) {
			SV[sysi].initialise(B[sysi], nSoundings);
		}
		unset_fftw_lock();
	}

	void setup_data()
	{
		nAllData = 0;
		_dindex_.resize(nSoundings);
		for (size_t si = 0; si < nSoundings; si++) {
			_dindex_[si].resize(nSystems);
			for (size_t sysi = 0; sysi < nSystems; sysi++) {
				_dindex_[si][sysi].resize(4);//4 because of xzinversion				
				for (size_t ci = 0; ci < 4; ci++) {
					_dindex_[si][sysi][ci].resize(SV[sysi].nwindows);
					for (size_t wi = 0; wi < SV[sysi].nwindows; wi++) {
						_dindex_[si][sysi][ci][wi] = -1;
					}
				}
			}
		}

		int di = 0;
		for (size_t si = 0; si < nSoundings; si++) {
			for (size_t sysi = 0; sysi < nSystems; sysi++) {
				cTDEmSystemInfo& S = SV[sysi];
				if (S.invertXPlusZ) {
					nAllData += S.nwindows;
					for (size_t wi = 0; wi < S.nwindows; wi++) {
						_dindex_[si][sysi][XZAMP][wi] = di;
						di++;
					}

					if (S.CompInfo[YCOMP].Use) {
						nAllData += S.nwindows;
						for (size_t wi = 0; wi < S.nwindows; wi++) {
							_dindex_[si][sysi][YCOMP][wi] = di;
							di++;
						}
					}
				}
				else {
					for (size_t ci = 0; ci < 3; ci++) {
						cTDEmComponentInfo& c = S.CompInfo[ci];
						if (c.Use) {
							nAllData += S.nwindows;
							for (size_t wi = 0; wi < S.nwindows; wi++) {
								_dindex_[si][sysi][ci][wi] = di;
								di++;
							}
						}
					}
				}
			}
		}
	}

	bool initialise_bunch_data(){
		std::vector<double> obs(nAllData);
		std::vector<double> err(nAllData);
		std::vector<double> pred(nAllData);

		for (size_t si = 0; si < nSoundings; si++) {
			for (size_t sysi = 0; sysi < nSystems; sysi++) {
				cTDEmSystemInfo& S = SV[sysi];
				cTDEmSystem& T = S.T;
				if (S.reconstructPrimary) {
					T.setgeometry(G[si].tfr);
					T.LEM.calculation_type = cLEM::CalculationType::FORWARDMODEL;
					T.LEM.derivative_layer = INT_MAX;
					T.setprimaryfields();
					S.CompInfo[XCOMP].data[si].P = T.PrimaryX;
					S.CompInfo[YCOMP].data[si].P = T.PrimaryY;
					S.CompInfo[ZCOMP].data[si].P = T.PrimaryZ;
				}

				if (S.invertXPlusZ) {
					for (size_t wi = 0; wi < S.nwindows; wi++) {
						//XZ Amplitude						
						int di = dindex(si, sysi, XZAMP, wi);
						double X = S.CompInfo[XCOMP].data[si].S[wi];
						double Z = S.CompInfo[ZCOMP].data[si].S[wi];						
						if (S.invertPrimaryPlusSecondary) {
							X += S.CompInfo[XCOMP].data[si].P;
							Z += S.CompInfo[ZCOMP].data[si].P;														
						}
						obs[di] = std::hypot(X, Z);

						const double& Xerr = S.CompInfo[XCOMP].data[si].E[wi];
						const double& Zerr = S.CompInfo[ZCOMP].data[si].E[wi];
						err[di] = std::hypot(X * Xerr, Z * Zerr) / obs[di];

						//Y Comp
						if (S.CompInfo[YCOMP].Use) {
							int di = dindex(si, sysi, YCOMP, wi);							
							obs[di] = S.CompInfo[YCOMP].data[si].S[wi];
							if (S.invertPrimaryPlusSecondary) {
								obs[di] += S.CompInfo[YCOMP].data[si].P;
							}
							err[di] = S.CompInfo[YCOMP].data[si].E[wi];
						}
					}
				}
				else {
					for (size_t ci = 0; ci < 3; ci++) {
						if (S.CompInfo[ci].Use == false) continue;
						for (size_t wi = 0; wi < S.nwindows; wi++) {							
							int di = dindex(si, sysi, ci, wi);
							obs[di] = S.CompInfo[ci].data[si].S[wi];
							if (S.invertPrimaryPlusSecondary) {
								obs[di] += S.CompInfo[ci].data[si].P;
							}
							err[di] = S.CompInfo[ci].data[si].E[wi];
						}
					}
				}
			}
		}
		
		//Work out indices to be culled				
		ActiveData.clear();
		for (size_t i = 0; i < nAllData; i++){
			if(!isnull(obs[i])  && !isnull(err[i])) ActiveData.push_back(i);			
		}
		nData = ActiveData.size();

		if (nData != nAllData) {
			size_t ncull = nAllData - nData;
			OutputMessage += strprint(", %d null data/noise were culled",(int)ncull);
		}
		Err = cull(err);
		Obs = cull(obs);				
		//std::cerr << std::endl << Obs << std::endl;

		//Check for zero Error values		
		int nzeroerr = 0;
		for (size_t i = 0; i < nData; i++) {
			if (Err[i] == 0.0) nzeroerr++;
		}
		if (nzeroerr > 0) {			
			OutputMessage += strprint(", Skipped %d noise values were 0.0", nzeroerr);			
			return false;
		}				
		return true;
	}
		
	void initialise_bunch_parameters() {

		for (size_t si = 0; si < nSoundings; si++) {
			const cEarthStruct& e = E[si];
			const cGeomStruct& g = G[si];
			if (solve_conductivity()) {
				for (size_t li = 0; li < nLayers; li++) {
					RefParam[cindex(si,li)] = log10(e.ref.conductivity[li]);
					RefParamStd[cindex(si,li)] = e.std.conductivity[li];
				}
			}

			if (solve_thickness()) {
				for (size_t li = 0; li < nLayers - 1; li++) {
					RefParam[tindex(si,li)] = log10(e.ref.thickness[li]);
					RefParamStd[tindex(si,li)] = e.std.thickness[li];
				}
			}

			for (int gi = 0; gi < cTDEmGeometry::size(); gi++) {
				std::string gname = cTDEmGeometry::element_name(gi);
				const int pi = gindex(si, gname);								
				if (pi >= 0) {						
					RefParam[pi] = g.ref[gname];
					RefParamStd[pi] = g.std[gname];
				}
			}
		}
	}
		
	Vector parameter_change(const double& lambda, const Vector& m_old, const Vector& pred)
	{
		Vector m_new = solve_linear_system(lambda, m_old, pred);
		Vector dm = m_new - m_old;

		if (fdC.bound()) {			
			for (size_t si = 0; si < nSoundings; si++) {
				const cEarthStruct& e = E[si];
				for (size_t li = 0; li < nLayers; li++) {
					const int pindex = cindex(si,li);
					const double lmin = std::log10(e.min.conductivity[li]);
					const double lmax = std::log10(e.max.conductivity[li]);
					if (m_new[pindex] < lmin) {
						if (Verbose) {
							//std::cerr << rec_it_str() << std::endl;
							//std::cerr << "Lower conductivity bound reached" << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << dm[pindex] << "\tm=" << m_old[pindex] << "\tm+dm=" << m_new[pindex] << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(m_new[pindex]) << std::endl;
						}
						dm[pindex] = lmin - m_old[pindex];
						//if (Verbose) std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(dm[pindex] + m_old[pindex]) << std::endl;
					}
					else if (m_new[pindex] > lmax) {
						if (Verbose) {
							//std::cerr << rec_it_str() << std::endl;
							//std::cerr << "Upper conductivity bound reached" << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << dm[pindex] << "\tm=" << m_old[pindex] << "\tm+dm=" << m_new[pindex] << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(m_new[pindex]) << std::endl;
						}
						dm[pindex] = lmax - m_old[pindex];
						//if (Verbose) std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(dm[pindex] + m_old[pindex]) << std::endl;
					}
				}
			}
		}
		
		if (fdT.bound()) {
			for (size_t si = 0; si < nSoundings; si++) {
				const cEarthStruct& e = E[si];
				for (size_t li = 0; li < nLayers - 1; li++) {
					const int pindex = tindex(si,li);
					const double lmin = std::log10(e.min.thickness[li]);
					const double lmax = std::log10(e.max.thickness[li]);
					if (m_new[pindex] < lmin) {
						if (Verbose) {
							//std::cerr << rec_it_str() << std::endl;
							//std::cerr << "Lower thickness bound reached" << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << dm[pindex] << "\tm=" << m_old[pindex] << "\tm+dm=" << m_new[pindex] << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(m_new[pindex]) << std::endl;
						}
						dm[pindex] = lmin - m_old[pindex];
						//if (Verbose) std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(dm[pindex] + m_old[pindex]) << std::endl;
					}
					else if (m_new[pindex] > lmax) {
						if (Verbose) {
							//std::cerr << rec_it_str() << std::endl;
							//std::cerr << "Upper thickness bound reached" << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << dm[pindex] << "\tm=" << m_old[pindex] << "\tm+dm=" << m_new[pindex] << std::endl;
							//std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(m_new[pindex]) << std::endl;
						}
						dm[pindex] = lmax - m_old[pindex];
						//if (Verbose) std::cerr << "\t li=" << li << "\tdm=" << pow10(dm[pindex]) << "\tm=" << pow10(m_old[pindex]) << "\tm+dm=" << pow10(dm[pindex] + m_old[pindex]) << std::endl;
					}
				}
			}
		}

		for (size_t si = 0; si < nSoundings; si++) {
			cGeomStruct& g = G[si];
			for (size_t i = 0; i < cTDEmGeometry::size(); i++) {
				const std::string ename = cTDEmGeometry::element_name(i);
				const cInvertibleFieldDefinition& e = fdG.cref(ename);
				if (e.bound()) {
					const int pi = gindex(si,ename);
					const double emin = g.min[ename];
					const double emax = g.max[ename];
					if (m_new[pi] < emin) {
						if (Verbose) {
							//std::cerr << rec_it_str() << std::endl;
							//std::cerr << "Lower " << ename << " bound reached" << std::endl;
							//std::cerr << "\tdm=" << dm[pi] << "\tm=" << m_old[pi] << "\tm+dm=" << m_new[pi] << std::endl;
						}
						dm[pi] = emin - m_old[pi];
					}
					else if (m_new[pi] > emax) {
						if (Verbose) {
							//std::cerr << rec_it_str() << std::endl;
							//std::cerr << "Upper " << ename << " bound reached" << std::endl;
							//std::cerr << "\tdm=" << dm[pi] << "\tm=" << m_old[pi] << "\tm+dm=" << m_new[pi] << std::endl;
						}
						dm[pi] = emax - m_old[pi];
					}
				}
			}
		}
		
		return dm;
	}

	std::vector<cEarth1D> get_earth(const Vector& parameters)
	{
		std::vector<cEarth1D> ev(nSoundings);;
		for (size_t si = 0; si < nSoundings; si++) {
			ev[si] = E[si].ref;
			if (solve_conductivity()) {				
				for (size_t li = 0; li < nLayers; li++) {
					ev[si].conductivity[li] = pow10(parameters[cindex(si, li)]);
				}
			}			

			if (solve_thickness()) {				
				for (size_t li = 0; li < nLayers - 1; li++) {
					ev[si].thickness[li] = pow10(parameters[tindex(si, li)]);
				}
			}			
		}
		return ev;
	}

	std::vector<cTDEmGeometry> get_geometry(const Vector& parameters)
	{
		std::vector<cTDEmGeometry> gv(nSoundings);
		for (size_t si = 0; si < nSoundings; si++) {
			gv[si] = G[si].input;
			for (int gi = 0; gi < cTDEmGeometry::size(); gi++) {
				const std::string& gname = cTDEmGeometry::element_name(gi);				
				const int pi = gindex(si, gname);
				if (pi>=0) {
					gv[si][gname] = parameters[pi];
				}
			}
		}
		return gv;
	}

	void set_predicted()
	{
		for (size_t sysi = 0; sysi < nSystems; sysi++) {
			cTDEmSystemInfo& S = SV[sysi];
			cTDEmSystem& T = S.T;

			cTDEmData& d = S.predicted;
			d.xcomponent().Primary = T.PrimaryX;
			d.ycomponent().Primary = T.PrimaryY;
			d.zcomponent().Primary = T.PrimaryZ;
			d.xcomponent().Secondary = T.X;
			d.ycomponent().Secondary = T.Y;
			d.zcomponent().Secondary = T.Z;
		}
	}

	void forwardmodel(const Vector& parameters, Vector& predicted) {
		Matrix dummy;	
		nForwards++;
		forwardmodel_impl(parameters, predicted, dummy, false);		
	}

	void forwardmodel_and_jacobian(const Vector& parameters, Vector& predicted, Matrix& jacobian) {
		nForwards++;
		nJacobians++;
		forwardmodel_impl(parameters, predicted, jacobian, true);
	}

	void forwardmodel_impl(const Vector& parameters, Vector& predicted, Matrix& jacobian, bool computederivatives)
	{
		Vector pred_all(nAllData);
		Matrix J_all;
		if (computederivatives) {
			J_all.resize(nAllData, nParam);
			J_all.setZero();
		}

		std::vector<cEarth1D> ev = get_earth(parameters);
		std::vector<cTDEmGeometry> gv = get_geometry(parameters);		
		for (size_t sysi = 0; sysi < nSystems; sysi++) {			
			cTDEmSystemInfo& S = SV[sysi];
			cTDEmSystem& T = S.T;
			const size_t nw = T.NumberOfWindows;
			for (size_t si = 0; si < nSoundings; si++) {
				const cEarth1D& e = ev[si];
				const cTDEmGeometry& g = gv[si];
				T.setconductivitythickness(e.conductivity, e.thickness);
				T.setgeometry(g);

				//Forwardmodel
				T.LEM.calculation_type = cLEM::CalculationType::FORWARDMODEL;
				T.LEM.derivative_layer = INT_MAX;
				T.setupcomputations();
				T.setprimaryfields();
				T.setsecondaryfields();

				std::vector<double> xfm = T.X;
				std::vector<double> yfm = T.Y;
				std::vector<double> zfm = T.Z;
				std::vector<double> xzfm;
				if (S.invertPrimaryPlusSecondary) {
					xfm += T.PrimaryX;
					yfm += T.PrimaryY;
					zfm += T.PrimaryZ;
				}

				if (S.invertXPlusZ) {
					xzfm.resize(T.NumberOfWindows);
					for (size_t wi = 0; wi < T.NumberOfWindows; wi++) {
						xzfm[wi] = std::hypot(xfm[wi], zfm[wi]);
					}
				}

				if (S.invertXPlusZ) {
					for (size_t wi = 0; wi < nw; wi++) {
						const int& di = dindex(si, sysi, XZAMP, wi);
						pred_all[di] = xzfm[wi];
						if (S.CompInfo[1].Use){
							pred_all[dindex(si, sysi, YCOMP, wi)] = yfm[wi];
						}
					}
				}
				else {
					for (size_t wi = 0; wi < nw; wi++) {
						if (S.CompInfo[XCOMP].Use) pred_all[dindex(si, sysi, XCOMP, wi)] = xfm[wi];
						if (S.CompInfo[YCOMP].Use) pred_all[dindex(si, sysi, YCOMP, wi)] = yfm[wi];
						if (S.CompInfo[ZCOMP].Use) pred_all[dindex(si, sysi, ZCOMP, wi)] = zfm[wi];
					}
				}

				if (computederivatives) {					
					std::vector<double> xdrv(nw);
					std::vector<double> ydrv(nw);
					std::vector<double> zdrv(nw);
					if (solve_conductivity()) {
						for (size_t li = 0; li < nLayers; li++) {
							const int pindex = cindex(si,li);
							T.LEM.calculation_type = cLEM::CalculationType::CONDUCTIVITYDERIVATIVE;
							T.LEM.derivative_layer = li;
							T.setprimaryfields();
							T.setsecondaryfields();

							fillDerivativeVectors(S, xdrv, ydrv, zdrv);
							//multiply by natural log(10) as parameters are in logbase10 units
							double sf = log(10.0) * e.conductivity[li];
							xdrv *= sf; ydrv *= sf; zdrv *= sf;
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}
					}

					if (solve_thickness()) {
						for (size_t li = 0; li < nLayers - 1; li++) {
							const int pindex = tindex(si,li);
							T.LEM.calculation_type = cLEM::CalculationType::THICKNESSDERIVATIVE;
							T.LEM.derivative_layer = li;
							T.setprimaryfields();
							T.setsecondaryfields();
							fillDerivativeVectors(S, xdrv, ydrv, zdrv);
							//multiply by natural log(10) as parameters are in logbase10 units
							double sf = log(10.0) * e.thickness[li];
							xdrv *= sf; ydrv *= sf; zdrv *= sf;
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}
					}

					if (FreeGeometry) {

						if (solve_geometry_element("tx_height")) {							
							const size_t pindex = gindex(si, "tx_height");
							T.LEM.calculation_type = cLEM::CalculationType::HDERIVATIVE;
							T.LEM.derivative_layer = INT_MAX;
							T.setprimaryfields();
							T.setsecondaryfields();
							fillDerivativeVectors(S, xdrv, ydrv, zdrv);
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}

						if (solve_geometry_element("txrx_dx")) {							
							const size_t pindex = gindex(si,"txrx_dx");
							T.LEM.calculation_type = cLEM::CalculationType::XDERIVATIVE;
							T.LEM.derivative_layer = INT_MAX;
							T.setprimaryfields();
							T.setsecondaryfields();
							fillDerivativeVectors(S, xdrv, ydrv, zdrv);
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}

						if (solve_geometry_element("txrx_dy")) {							
							const size_t pindex = gindex(si, "txrx_dy");
							T.LEM.calculation_type = cLEM::CalculationType::YDERIVATIVE;
							T.LEM.derivative_layer = INT_MAX;
							T.setprimaryfields();
							T.setsecondaryfields();
							fillDerivativeVectors(S, xdrv, ydrv, zdrv);
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}

						if (solve_geometry_element("txrx_dz")) {
							const size_t pindex = gindex(si, "txrx_dz");
							T.LEM.calculation_type = cLEM::CalculationType::ZDERIVATIVE;
							T.LEM.derivative_layer = INT_MAX;
							T.setprimaryfields();
							T.setsecondaryfields();
							fillDerivativeVectors(S, xdrv, ydrv, zdrv);
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}

						if (solve_geometry_element("rx_pitch")) {
							const size_t pindex = gindex(si, "rx_pitch");
							T.drx_pitch(xfm, zfm, g.rx_pitch, xdrv, zdrv);
							ydrv *= 0.0;
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}
						
						if (solve_geometry_element("rx_roll")) {
							const size_t pindex = gindex(si, "rx_roll");
							T.drx_roll(yfm, zfm, g.rx_roll, ydrv, zdrv);
							xdrv *= 0.0;
							fillMatrixColumn(J_all, si, sysi, pindex, xfm, yfm, zfm, xzfm, xdrv, ydrv, zdrv);
						}
					}
				}
			}
		}
		predicted = cull(pred_all);
		if(computederivatives) jacobian = cull(J_all);

		if (Verbose && computederivatives) {
			//std::cerr << "\n-----------------\n";
			//std::cerr << "J_all: It " << CIS.iteration + 1 << std::endl;			
			//std::cerr << J_all;			
			//std::cerr << "\n-----------------\n";
		}
	}

	void fillDerivativeVectors(cTDEmSystemInfo& S, std::vector<double>& xdrv, std::vector<double>& ydrv, std::vector<double>& zdrv)
	{
		cTDEmSystem& T = S.T;
		xdrv = T.X;
		ydrv = T.Y;
		zdrv = T.Z;
		if (S.invertPrimaryPlusSecondary) {
			xdrv += T.PrimaryX;
			ydrv += T.PrimaryY;
			zdrv += T.PrimaryZ;
		}
	}

	void fillMatrixColumn(Matrix& M, const size_t& si, const size_t& sysi, const size_t& pindex, const std::vector<double>& xfm, const std::vector<double>& yfm, const std::vector<double>& zfm, const std::vector<double>& xzfm, const std::vector<double>& xdrv, const std::vector<double>& ydrv, const std::vector<double>& zdrv)
	{
		const cTDEmSystemInfo& S = SV[sysi];
		const size_t& nw = S.T.NumberOfWindows;
		if (S.invertXPlusZ) {
			for (size_t wi = 0; wi < nw; wi++) {								
				M(dindex(si, sysi, XZAMP, wi), pindex) = (xfm[wi] * xdrv[wi] + zfm[wi] * zdrv[wi]) / xzfm[wi];
				if (S.CompInfo[1].Use) {
					M(dindex(si, sysi, YCOMP, wi), pindex) = ydrv[wi];
				}
			}
		}
		else {
			for (size_t wi = 0; wi < nw; wi++) {				
				if (S.CompInfo[XCOMP].Use)M(dindex(si, sysi, XCOMP, wi),pindex) = xdrv[wi];
				if (S.CompInfo[YCOMP].Use)M(dindex(si, sysi, YCOMP, wi),pindex) = ydrv[wi];
				if (S.CompInfo[ZCOMP].Use)M(dindex(si, sysi, ZCOMP, wi),pindex) = zdrv[wi];
			}
		}
	}

	void save_iteration_file(const cIterationState& S) {
		std::ofstream ofs(dumppath() + "iteration.dat");
		ofs << S.info_string();
	};
	
	void writeresult(const int& pointindex, const cIterationState& S)
	{		
		const int& pi = (int)Bunch.master_record();
		const int& si = (int)Bunch.master_index();
		OM->begin_point_output();
		
		//Ancillary	
		OM->writefield(pi, Id[si].uniqueid, "uniqueid", "Inversion sequence number", UNITLESS, 1, NC_UINT, DN_NONE, 'I', 12, 0);
		for (size_t i = 0; i<AncFld.size(); i++) {
			cFdVrnt& fdv = AncFld[i].second;
			cAsciiColumnField c;
			std::string fname = fdv.fd.varname;
			IM->get_acsiicolumnfield(fname, c);			
			OM->writevrnt(pi, fdv.vnt, c);
		}

		//Geometry Input
		bool invertedfieldsonly = false;
		for (size_t i = 0; i < G[si].input.size(); i++) {
			if (invertedfieldsonly && solvegeometryindex(i) == false)continue;
			OM->writefield(pi, G[si].input[i], "input_" + G[si].input.element_name(i), "Input " + G[si].input.description(i), G[si].input.units(i), 1, NC_FLOAT, DN_NONE, 'F', 9, 2);
		}

		//Geometry Modelled		
		const cTDEmGeometry& g = G[si].invmodel;
		invertedfieldsonly = true;
		for (size_t gi = 0; gi < g.size(); gi++) {
			if (invertedfieldsonly && solvegeometryindex(gi) == false)continue;
			OM->writefield(pi, g[gi], "inverted_" + g.element_name(gi), "Inverted " + g.description(gi), g.units(gi), 1, NC_FLOAT, DN_NONE, 'F', 9, 2);
		}
				
		//ndata
		OM->writefield(pi,
			nData, "ndata", "Number of data in inversion", UNITLESS,
			1, NC_UINT, DN_NONE, 'I', 4, 0);

		//Earth	
		const cEarth1D& e = E[si].invmodel;
		OM->writefield(pi,
			nLayers,"nlayers","Number of layers ", UNITLESS,
			1, NC_UINT, DN_NONE, 'I', 4, 0);
		
		OM->writefield(pi,
			e.conductivity, "conductivity", "Layer conductivity", "S/m",
			e.conductivity.size(), NC_FLOAT, DN_LAYER, 'E', 15, 6);
		
		double bottomlayerthickness = 100.0;
		if (solve_thickness() == false && nLayers > 1) {
			bottomlayerthickness = e.thickness[nLayers - 2];
		}
		std::vector<double> thickness = e.thickness;
		thickness.push_back(bottomlayerthickness);

		OM->writefield(pi,
			thickness, "thickness", "Layer thickness", "m",
			thickness.size(), NC_FLOAT, DN_LAYER, 'F', 9, 2);
					
				
		if (OO.PositiveLayerTopDepths) {			
			std::vector<double> dtop = e.layer_top_depth();
			OM->writefield(pi,
				dtop, "depth_top", "Depth to top of layer", "m",
				dtop.size(), NC_FLOAT, DN_LAYER, 'F', 9, 2);
		}

		if (OO.NegativeLayerTopDepths) {
			std::vector<double> ndtop = -1.0*e.layer_top_depth();
			OM->writefield(pi,
				ndtop, "depth_top_negative", "Negative of depth to top of layer", "m",
				ndtop.size(), NC_FLOAT, DN_LAYER, 'F', 9, 2);
		}
		
		if (OO.PositiveLayerBottomDepths) {
			std::vector<double> dbot = e.layer_bottom_depth();
			OM->writefield(pi,
				dbot, "depth_bottom", "Depth to bottom of layer", "m",
				dbot.size(), NC_FLOAT, DN_LAYER, 'F', 9, 2);
		}

		if (OO.NegativeLayerBottomDepths) {
			std::vector<double> ndbot = -1.0 * e.layer_bottom_depth();
			OM->writefield(pi,
				ndbot, "depth_bottom_negative", "Negative of depth to bottom of layer", "m",
				ndbot.size(), NC_FLOAT, DN_LAYER, 'F', 9, 2);
		}

		if (OO.InterfaceElevations) {			
			std::vector<double> etop = e.layer_top_depth();
			etop += Id[si].elevation;
			OM->writefield(pi,
				etop, "elevation_interface", "Elevation of interface", "m",
				etop.size(), NC_FLOAT, DN_LAYER, 'F', 9, 2);
		}
				
		if (OO.ParameterSensitivity) {
			std::vector<double> ps = copy(ParameterSensitivity);
			if (solve_conductivity()) {
				std::vector<double> v(ps.begin() + cindex(si,0), ps.begin() + cindex(si,0) + nLayers);
				OM->writefield(pi,
					v, "conductivity_sensitivity", "Conductivity parameter sensitivity", UNITLESS,
					v.size(), NC_FLOAT, DN_LAYER, 'E', 15, 6);
			}
			
			if (solve_thickness()) {
				std::vector<double> v(ps.begin() + tindex(si,0), ps.begin() + tindex(si,0) + nLayers-1);
				v.push_back(0.0);//halfspace layer not a parameter
				OM->writefield(pi,
					v, "thickness_sensitivity", "Thickness parameter sensitivity", UNITLESS,
					v.size(), NC_FLOAT, DN_LAYER, 'E', 15, 6);
			}

			const cTDEmGeometry& g = G[si].input;
			for (size_t gi = 0; gi < g.size(); gi++) {
				if (solvegeometryindex(gi) == true) {
					const std::string& gname = g.element_name(gi);
					std::string name = "inverted_" + gname + "_sensitivity";
					std::string desc = g.description(gi) + " parameter sensitivity";
					OM->writefield(pi,
						ps[gindex(si,gname)], name, desc, UNITLESS,
						1, NC_FLOAT, DN_NONE, 'E', 15, 6);					
				}
			}
		}

		if (OO.ParameterUncertainty) {
			std::vector<double> pu = copy(ParameterUncertainty);
			if (solve_conductivity()) {
				std::vector<double> v(pu.begin() + cindex(si,0), pu.begin() + cindex(si,0) + nLayers);
				OM->writefield(pi,
					v, "conductivity_uncertainty", "Conductivity parameter uncertainty", "log10(S/m)",
					v.size(), NC_FLOAT, DN_LAYER, 'E', 15, 6);
			}

			if (solve_thickness()) {
				std::vector<double> v(pu.begin() + tindex(si,0), pu.begin() + tindex(si,0) + nLayers - 1);
				v.push_back(0.0);//halfspace layer not a parameter
				OM->writefield(pi,
					v, "thickness_uncertainty", "Thickness parameter uncertainty", "log10(m)",
					v.size(), NC_FLOAT, DN_LAYER, 'E', 15, 6);
			}
			
			const cTDEmGeometry& g = G[si].input;
			for (size_t gi = 0; gi < g.size(); gi++) {
				if (solvegeometryindex(gi) == false) continue;
				const std::string& gname = g.element_name(gi);
				std::string name = "inverted_" + gname + "_uncertainty";
				std::string desc = g.description(gi) + " parameter uncertainty";
				OM->writefield(pi,
					pu[gindex(si,gname)], name, desc, g.units(gi),
					1, NC_FLOAT, DN_NONE, 'E', 15, 6);				
			}
		}

				
		//ObservedData
		if (OO.ObservedData) {			
			for (size_t sysi = 0; sysi < nSystems; sysi++) {
				cTDEmSystemInfo& S = SV[sysi];
				for (size_t ci = 0; ci < 3; ci++) {
					if (S.CompInfo[ci].Use) writeresult_emdata(pi,
						si, S.CompInfo[ci].Name,
						"observed", "Observed",
						'E', 15, 6, S.CompInfo[ci].data[si].P, S.CompInfo[ci].data[si].S, S.invertPrimaryPlusSecondary);
				}
			}
		}

		
		//Noise Estimates
		if (OO.NoiseEstimates) {
			for (size_t sysi = 0; sysi < nSystems; sysi++) {
				cTDEmSystemInfo& S = SV[sysi];
				for (size_t ci = 0; ci < 3; ci++) {
					if (S.CompInfo[ci].Use) writeresult_emdata(pi,
						sysi, S.CompInfo[ci].Name,
						"noise", "Estimated noise",						
						'E', 15, 6, 0.0, S.CompInfo[ci].data[si].E, false);
				}
			}
		}
		
		//PredictedData
		if (OO.PredictedData) {
			for (size_t sysi = 0; sysi < nSystems; sysi++) {
				cTDEmSystemInfo& S = SV[sysi];
				for (size_t ci = 0; ci < 3; ci++) {
					if (S.CompInfo[ci].Use) writeresult_emdata(pi,
						sysi, S.CompInfo[ci].Name, "predicted", "Predicted", 'E', 15, 6,
						S.predicted.component(ci).Primary,
						S.predicted.component(ci).Secondary,
						S.invertPrimaryPlusSecondary);
				}
			}
		}
		

		//Inversion parameters and norms
		OM->writefield(pi, AlphaC, "AlphaC", "AlphaConductivity inversion parameter", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, AlphaT, "AlphaT", "AlphaThickness inversion parameter", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, AlphaG, "AlphaG", "AlphaGeometry inversion parameter", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, AlphaS, "AlphaS", "AlphaSmoothness inversion parameter", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);		
		OM->writefield(pi, AlphaQ, "AlphaQ", "AlphaHomogeneous inversion parameter", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phid, "PhiD", "Normalised data misfit", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phim, "PhiM", "Combined model norm", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phic, "PhiC", "Conductivity reference model norm", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phit, "PhiT", "Thickness reference model norm", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phig, "PhiG", "Geometry reference model norm", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phis, "PhiS", "Smoothness model norm", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.phiq, "PhiQ", "Homogeneity model norm", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.lambda, "Lambda", "Lambda regularization parameter", UNITLESS, 1, NC_FLOAT, DN_NONE, 'E', 15, 6);
		OM->writefield(pi, S.iteration, "Iterations", "Number of iterations", UNITLESS, 1, NC_UINT, DN_NONE, 'I', 4, 0);
				
		//End of record book keeping
		OM->end_point_output();		
		if (pointsoutput == 0) {			
			OM->end_first_record();//only do this once		
		}
		pointsoutput++;
	};

	void writeresult_emdata(const int& pointindex, const size_t& sysnum, const std::string& comp, const std::string& nameprefix, const std::string& descprefix, const char& form, const int& width, const int& decimals, const double& p, std::vector<double>& s, const bool& includeprimary)
	{
		std::string DN_WINDOW = "em_window";
		std::string sysname = nameprefix + strprint("_EMSystem_%d_", (int)sysnum + 1);
		std::string sysdesc = descprefix + strprint(" EMSystem %d ", (int)sysnum + 1);
		if (includeprimary) {
			std::string name = sysname + comp + "P";
			std::string desc = sysdesc + comp + "-component primary field";			
			OM->writefield(pointindex,
				p, name, desc, UNITLESS,
				1, NC_FLOAT, DN_NONE, form, width, decimals);			
		}

		{
			std::string name = sysname + comp + "S";
			std::string desc = sysdesc + comp + "-component secondary field";
			OM->writefield(pointindex,
				s, name, desc, UNITLESS,
				s.size(), NC_FLOAT, DN_WINDOW, form, width, decimals);
		}
	}

	bool solvegeometryindex(const size_t index) {		
		//eGeometryElementType getype = cTDEmGeometry::elementtype(index);		
		return fdG.cref(cTDEmGeometry::element_name(index)).solve;
	}
	
	bool read_bunch(const size_t& record) {
		_GSTITEM_

		int fi = AncFld.keyindex("line");
		cFieldDefinition& fdline = AncFld[fi].second.fd;
		bool bunchstatus = IM->get_bunch(Bunch, fdline, (int)record, (int)nSoundings, (int)nBunchSubsample);

		if (bunchstatus == false) {
			return bunchstatus;
		}

		for (size_t si = 0; si < Bunch.size(); si++) {
			const size_t& record = Bunch.record(si);
			bool loadstatus = IM->load_record(record);
			if (loadstatus == false) {
				OutputMessage += ", Skipping - could not load record";
				return false;
			}
			bool valid = IM->is_record_valid();
			if (valid == false) {
				OutputMessage += ", Skipping - record is not valid";
				return false;
			}
			bool readstatus = read_record(si);
			if (valid == false) {
				OutputMessage += ", Skipping - could not read record";
				return false;
			}
		}
		return true;			
	}

	bool read_record(const size_t& bunchsoundingindex)
	{
		const size_t& si = bunchsoundingindex;
		bool readstatus = true;
		cEarthStruct& e = E[si];
		cGeomStruct& g = G[si];


		if (IM->parse_record() == false) return false;

		bool status;
		Id[si].uniqueid = (int)IM->record();

		status = read_ancillary_fields(si);
		status = read_geometry(si, fdG);
		status = IM->read(fdC.input, e.ref.conductivity, nLayers); if (status == false) readstatus = false;
		if (solve_conductivity()) {
			status = IM->read(fdC.ref, e.ref.conductivity, nLayers); if (status == false) readstatus = false;
			status = IM->read(fdC.std, e.std.conductivity, nLayers); if (status == false) readstatus = false;
			status = IM->read(fdC.min, e.min.conductivity, nLayers); if (status == false) readstatus = false;
			status = IM->read(fdC.max, e.max.conductivity, nLayers); if (status == false) readstatus = false;
		}

		status = IM->read(fdT.input, e.ref.thickness, nLayers - 1); if (status == false) readstatus = false;
		if (solve_thickness()) {
			status = IM->read(fdT.ref, e.ref.thickness, nLayers - 1); if (status == false) readstatus = false;
			status = IM->read(fdT.std, e.std.thickness, nLayers - 1); if (status == false) readstatus = false;
			status = IM->read(fdT.min, e.min.thickness, nLayers - 1); if (status == false) readstatus = false;
			status = IM->read(fdT.max, e.max.thickness, nLayers - 1); if (status == false) readstatus = false;
		}
		e.sanity_check();

		for (size_t sysi = 0; sysi < nSystems; sysi++) {
			read_system_data(sysi, si);
		}
		return readstatus;
	}

	bool read_ancillary_fields(const size_t& bunchindex) {
		const size_t& si = bunchindex;
		SampleId& id = Id[si];

		for (size_t fi = 0; fi < AncFld.size(); fi++) {
			IM->readfdvnt(AncFld[fi].second);
		}

		set_ancillary_id("Survey", id.survey);
		set_ancillary_id("Date", id.date);
		set_ancillary_id("Flight", id.flight);
		set_ancillary_id("Line", id.line);
		set_ancillary_id("Fiducial", id.fiducial);
		set_ancillary_id("X", id.x);
		set_ancillary_id("Y", id.y);
		set_ancillary_id("GroundElevation", id.elevation);
		return true;
	}

	template<typename T>
	bool set_ancillary_id(const std::string key, T& value) {
		int ki = AncFld.keyindex(key);
		if (ki >= 0) {
			value = std::get<T>(AncFld[ki].second.vnt);
			return true;
		}
		return false;
	}

	bool read_geometry(const size_t& bunchindex, cIFDMap& map)
	{
		bool status = true;
		const size_t si = bunchindex;
		cGeomStruct& g = G[si];
		for (size_t gi = 0; gi < cTDEmGeometry::size(); gi++) {
			std::string ename = cTDEmGeometry::element_name(gi);
			const cInvertibleFieldDefinition ge = map.cref(ename);
			bool inpstatus = IM->read(ge.input, g.input[gi]);
			bool refstatus = IM->read(ge.ref, g.ref[gi]);

			if (refstatus == false && inpstatus == true) {
				g.ref[gi] = g.input[gi];
				refstatus = true;
			}
			else if (inpstatus == false && refstatus == true) {
				g.input[gi] = g.ref[gi];
				inpstatus = true;
			}

			if (inpstatus == false) {
				std::ostringstream msg;
				msg << "Error: no 'Input or Ref' defined for " << ename << std::endl;
				glog.errormsg(msg.str());
			}

			if (refstatus == false) {
				std::ostringstream msg;
				msg << "Error: no 'Ref or Input' defined for " << ename << std::endl;
				glog.errormsg(msg.str());
			}

			bool tfrstatus = IM->read(ge.tfr, g.tfr[gi]);
			if (tfrstatus == false) {
				g.tfr[gi] = g.input[gi];
			}

			if (ge.solve) {
				bool stdstatus = IM->read(ge.std, g.std[gi]);
				if (stdstatus == false) {
					std::ostringstream msg;
					msg << "Error: no 'Std' defined for " << ename << std::endl;
					glog.errormsg(msg.str());
				}

				bool minstatus = IM->read(ge.min, g.min[gi]);
				bool maxstatus = IM->read(ge.max, g.max[gi]);
			}
		}
		return status;
	}

	bool read_geometryxxx(const std::vector<cFieldDefinition>& gfd, cTDEmGeometry& g)
	{		
		bool status = true;
		for (size_t i = 0; i < g.size(); i++) {
			bool istatus = IM->read(gfd[i], g[i]);
			if (istatus == false) {
				status = false;
			}
		}
		return status;
	}
	
	void read_system_data(size_t& sysindex, const size_t& soundingindex)
	{
		cTDEmSystemInfo& S = SV[sysindex];
		S.CompInfo[XCOMP].readdata(IM, soundingindex);
		S.CompInfo[YCOMP].readdata(IM, soundingindex);
		S.CompInfo[ZCOMP].readdata(IM, soundingindex);
	}

	void dump_first_iteration() {
		
		const std::string dp = dumppath();
		makedirectorydeep(dumppath());

		const size_t si = Bunch.master_index();
		cGeomStruct& g = G[si];
		cEarthStruct& e = E[si];
		SampleId& id = Id[si];
		
		write(Obs, dp + "observed.dat");
		write(Err, dp + "observed_std.dat");

		g.ref.write(dp + "geometry_start.dat");
		e.ref.write(dp + "earth_start.dat");

		g.ref.write(dp + "geometry_ref.dat");
		e.ref.write(dp + "earth_ref.dat");

		g.std.write(dp + "geometry_std.dat");
		e.std.write(dp + "earth_std.dat");

		std::ofstream ofs(dp + "Id.dat");
		char sep = '\n';

		ofs << id.uniqueid << sep;
		ofs << id.survey << sep;
		ofs << id.date << sep;
		ofs << id.flight << sep;
		ofs << id.line << sep;
		ofs << id.fiducial << sep;
		ofs << id.x << sep;
		ofs << id.y << sep;
		ofs << id.elevation << sep;
	}

	void dump_iteration(const cIterationState& state) {
		const std::string dp = dumppath();
		makedirectorydeep(dp);
		writetofile(Obs, dp + "d.dat");
		writetofile(Err, dp + "e.dat");
		writetofile(state.param, dp + "m.dat");
		writetofile(state.pred, dp + "g.dat");
		std::vector<cEarth1D> e = get_earth(state.param);
		std::vector <cTDEmGeometry> g = get_geometry(state.param);
		e[Bunch.master_index()].write(dumppath() + "earth_inv.dat");
		g[Bunch.master_index()].write(dumppath() + "geometry_inv.dat");
		save_iteration_file(state);
	}

	bool initialise_bunch() {	
		nForwards = 0;
		nJacobians = 0;
		OutputMessage = "";		
		CIS = cIterationState();
		bool status = initialise_bunch_data();
		if (status == false) return false;
		initialise_bunch_parameters();
		initialise_Wd();
		initialise_Wm();		
		dump_W_matrices();
		return true;
	}

	void iterate() {
		_GSTITEM_				
		CIS.iteration = 0;
		CIS.lambda = 1e8;
		CIS.param = RefParam;		
		forwardmodel(CIS.param, CIS.pred);
		CIS.phid = phiData(CIS.pred);
		CIS.targetphid = CIS.phid;
		CIS.phim = phiModel(CIS.param, CIS.phic, CIS.phit, CIS.phig, CIS.phis, CIS.phim);
		
		TerminationReason = "Has not terminated";

		if (OO.Dump) {
			dump_first_iteration();
			dump_iteration(CIS);
		}
					
		double percentchange = 100.0;
		bool   keepiterating = true;
		while (keepiterating == true) {
			if (CIS.iteration >= MaxIterations) {
				keepiterating = false;
				TerminationReason = "Too many iterations";
			}
			else if (CIS.phid <= MinimumPhiD) {
				keepiterating = false;
				TerminationReason = "Reached minimum";
			}
			else if (CIS.iteration > 4  && percentchange < MinimumImprovement) {
				keepiterating = false;
				TerminationReason = "Small % improvement";
			}
			else {			
				if (Verbose) {
					std::cerr << CIS.info_string();
				}
				if (CIS.iteration+1 >= BeginGeometrySolveIteration) FreeGeometry = true;
				else FreeGeometry = false;
				//if ((CIS.iteration+1)%2) FreeGeometry = false;
				//else FreeGeometry = true;
				
				Vector g;
				forwardmodel_and_jacobian(CIS.param, g, J);
				
				double targetphid = std::max(CIS.phid*0.7, MinimumPhiD);
				cTrial t  = lambda_search_target(CIS.lambda, targetphid);
				Vector dm = parameter_change(t.lambda, CIS.param, CIS.pred);
				Vector m = CIS.param + (t.stepfactor * dm);
				
				forwardmodel(m,g);
				double phid = phiData(g);

				percentchange = 100.0 * (CIS.phid - phid) / (CIS.phid);
				if (phid <= CIS.phid) {				
					CIS.iteration++;
					CIS.param = m;
					CIS.pred = g;
					CIS.targetphid = targetphid;										
					CIS.phid   = phid;
					CIS.lambda = t.lambda;
					CIS.phim = phiModel(CIS.param, CIS.phic, CIS.phit, CIS.phig, CIS.phis, CIS.phiq);
					if (OO.Dump) dump_iteration(CIS);
				}						
			}			
		} 
		
		std::vector<cEarth1D> ev = get_earth(CIS.param);
		std::vector<cTDEmGeometry> gv = get_geometry(CIS.param);
		for (size_t si = 0; si < nSoundings; si++) {			
			E[si].invmodel = ev[si];
			G[si].invmodel = gv[si];
		}

		forwardmodel_and_jacobian(CIS.param, CIS.pred, J);
		set_predicted();		
		ParameterSensitivity = compute_parameter_sensitivity();
		ParameterUncertainty = compute_parameter_uncertainty();
	}
	
	int execute() {
		_GSTITEM_				
		bool readstatus = true;
		int paralleljob = 0;			
		do{										
			int record = paralleljob*(int)IM->subsamplerate();			
			if ((paralleljob % Size) == Rank) {					
				std::ostringstream s;				
				if (readstatus = read_bunch(record)) {
					s << bunch_id();
					if (initialise_bunch()) {
						double t1 = gettime();
						iterate();
						double t2 = gettime();
						double etime = t2 - t1;
						writeresult(record, CIS);						
						s << bunch_result(etime);																		
					}
					else {
						OutputMessage += ", Skipping - could not initialise the bunch";						
					}
					s << std::endl;
					if (OutputMessage.size() > 0) {
						std::cerr << s.str();
					}
					glog.logmsg(s.str());
				}								
			}
			//break;
			paralleljob++;
		} while (readstatus == true);
		glog.close();
		return 0;
	}	

	double phiModel(const Vector& p)
	{
		double phic, phit, phig, phis, phiq;
		return phiModel(p, phic, phit, phig, phis, phiq);
	}

	double phiModel(const Vector& p, double& phic, double& phit, double& phig, double& phis, double& phiq)
	{
		phic = phiC(p);
		phit = phiT(p);
		phig = phiG(p);
		phis = phiS(p);
		phiq = phiQ(p);

		double v = phic + phit + phig + phis + phiq;
		return v;
	}

	double phiC(const Vector& p)
	{
		if (AlphaC == 0.0) return 0.0;
		Vector v = p - RefParam;
		return mtDm(v, Wc);
	}

	double phiT(const Vector& p)
	{
		if (AlphaT == 0.0)return 0.0;
		if (solve_thickness() == false)return 0.0;
		Vector v = p - RefParam;
		return mtDm(v, Wt);
	}

	double phiG(const Vector& p)
	{
		if (AlphaG == 0.0)return 0.0;
		Vector v = p - RefParam;
		return mtDm(v, Wg);
	}

	double phiS(const Vector& p)
	{
		if (AlphaS == 0)return 0.0;
		else return mtAm(p, Ws);
	}

	double phiQ(const Vector& p)
	{
		if (AlphaQ == 0)return 0.0;
		else return mtAm(p, Wq);
	}

	Vector solve_linear_system(const double& lambda, const Vector& param, const Vector& pred)
	{
		// Phi = (d-g(m)+Jm) Wd (d-g(m)+Jm) + lambda ( (m-m0)' Wr (m-m0) + m' Ws m) )
		//Ax = b
		//A = [J'WdJ + lambda (Wr + Ws)]
		//x = m(n+1)
		//b = J'Wd(d - g(m) + Jm) + lambda*Wr*m0
		//dm = m(n+1) - m = x - m

		
		const Vector& m = param;
		const Vector& g = pred;
		const Vector& d = Obs;		
		const Vector& e = Err;
		const Vector& m0 = RefParam;

		Matrix V = Wd;
		if (NormType == eNormType::L1) {
			for (size_t i = 0; i < nData; i++) {
				const double r = (d[i] - g[i]) / e[i];
				V(i, i) *= 1.0 / std::abs(r);
			}
		}

		Matrix JtV = J.transpose() * V;
		Matrix JtVJ = JtV * J;

		Vector b = JtV * (d - g + J * m) + lambda * (Wr * m0);
		Matrix A = JtVJ + lambda * Wm;
		Vector x = pseudoInverse(A) * b;
		return x;
	}
};

#endif
