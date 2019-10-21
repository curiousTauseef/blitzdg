// Copyright (C) 2017-2019  Waterloo Quantitative Consulting Group, Inc.
// See COPYING and LICENSE files at project root for more details.

#ifndef __MINGW32__

#include "Types.hpp"
#include "VtkOutputter.hpp"
#include "BlitzHelpers.hpp"
#include <vtk-7.1/vtkXMLUnstructuredGridWriter.h>
#include <vtk-7.1/vtkIndent.h>
#include <vtk-7.1/vtkUnstructuredGrid.h>
#include <vtk-7.1/vtkPoints.h>
#include <vtk-7.1/vtkProperty.h>
#include <vtk-7.1/vtkType.h>
#include <vtk-7.1/vtkTriangle.h>
#include <vtk-7.1/vtkDataSetAttributes.h>
#include <vtk-7.1/vtkSmartPointer.h>
#include <vtk-7.1/vtkCellArray.h>
#include <vtk-7.1/vtkDoubleArray.h>
#include <vtk-7.1/vtkFieldData.h>
#include <vtk-7.1/vtkPointData.h>
#include <vtk-7.1/vtkPolyData.h>
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <boost/python.hpp>

using std::stringstream;
using std::ostream;
using std::ios;
using std::filebuf;
using std::string;
using std::vector;
using boost::python::str;
using boost::python::numpy::ndarray;
using boost::python::stl_input_iterator;

namespace blitzdg {
	VtkOutputter::VtkOutputter(TriangleNodesProvisioner & _NodesProvisioner)
		: GridWriter { vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New() },
			NodesProvisioner { _NodesProvisioner } 
		{
			FileExtension = GridWriter->GetDefaultFileExtension();
		}

		/**
		 * Generates a file name for storing a blitzdg object in vtk format.
		 * @param[in] fieldName Name of field from the PDE (system), e.g., "u".
		 * @param[in] fileNumber An integral intex indicating a logical ordering on the output files. It is usually related to time-level.
		 */
		string VtkOutputter::generateFileName(const string & fieldName, const index_type fileNumber) const {
			stringstream fileNameStrm;
        	fileNameStrm << fieldName << setfill('0') << setw(7) << fileNumber << "." << FileExtension;
        	return fileNameStrm.str();
		}

	/**
     * Writes a blitz array to plain-text file.
     * @param[in] fileName Name of the file (e.g., field0000010.vtu).
     * @param[in] field Two-dimensional blitz array to be written to the file. Usually a 'field' of the PDE (system) being solved.
     * @param[in] delimeter Character that will be used to separate columns. Rows are always separated by line-endings.
     */
    void VtkOutputter::writeFieldToFile(const string & fileName, real_matrix_type field, const string & fieldName) const {

		real_matrix_type x = NodesProvisioner.get_xGrid(), y = NodesProvisioner.get_yGrid();

		index_type K = field.cols();
		index_type Np = field.rows();
		index_type nodeId = 0;

		// If higher order than linear, need to break up the trangles.
		if (Np > 3) {
			real_matrix_type xnew, ynew, fieldnew;
			splitTriangles(x, y, field, xnew, ynew, fieldnew);

			Np = 3;
			K = fieldnew.cols();

			x.resize(Np, K);
			y.resize(Np, K);
			field.resize(Np, K);

			x = xnew;
			y = ynew;
			field = fieldnew;
		}

		vtkSmartPointer<vtkCellArray> cellArray = vtkSmartPointer<vtkCellArray>::New();
		vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();

		vtkSmartPointer<vtkDoubleArray> array = vtkSmartPointer<vtkDoubleArray>::New();

		vtkSmartPointer<vtkUnstructuredGrid> unstructuredGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();

		const char* fieldNameChar = fieldName.c_str();
		array->SetName(fieldNameChar);
		array->SetNumberOfValues(Np*K);

		unstructuredGrid->Allocate(K);

		// '3' because this will only work for linear elements until we interpolate
		// to a finer (uniform) triangular mesh at higher order.
		vtkIdType nodes[3];

		for (index_type k=0; k < K; ++k) {
			for (index_type n=0; n < Np; ++n) {
				points->InsertPoint(nodeId, x(n, k), y(n, k), field(n,k) );
				
				array->SetValue(nodeId, field(n,k));

				nodes[n] = nodeId;
				++nodeId;
			}

			unstructuredGrid->InsertNextCell(VTK_TRIANGLE, 3, nodes);
		}

		unstructuredGrid->SetPoints(points);
		unstructuredGrid->GetPointData()->SetScalars(array);
		unstructuredGrid->GetPointData()->SetActiveScalars(fieldNameChar);

		// Write file
		vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer =
			vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
		writer->SetFileName(fileName.c_str());
#if VTK_MAJOR_VERSION <= 5
		writer->SetInput(unstructuredGrid);
#else
		writer->SetInputData(unstructuredGrid);
#endif
		writer->Write();
    }

	void VtkOutputter::writeFieldsToFiles(std::map<std::string, real_matrix_type>& fields, index_type tstep) {
		for (const auto& kv : fields) {
			const string& fieldName = kv.first;
			const real_matrix_type& field = kv.second;
			string fileName = generateFileName(fieldName, tstep);
			writeFieldToFile(fileName, field, fieldName);
		}
	}

	void VtkOutputter::splitTriangles(const real_matrix_type& x, const real_matrix_type& y, const real_matrix_type& field, real_matrix_type& xnew, real_matrix_type& ynew, real_matrix_type& fieldnew) const {
		index_type Np = field.rows();
		index_type K = field.cols();

		real_vector_type rout(Np), sout(Np);

		const index_type N = NodesProvisioner.get_NumFacePoints() - 1;

		index_type count = 0;

		index_matrix_type counter(N+1,N+1);
		counter = -1; // -1 == 'No Value'

		for (index_type n=0; n < N+1; ++n) {
			for (index_type m=0; m < N+2-(n+1); ++m) {
				rout(count) = -1. + 2.*static_cast<double>(m)/static_cast<double>(N);
				sout(count) = -1. + 2.*static_cast<double>(n)/static_cast<double>(N);

				counter(n,m) = count;
				++count;
			}
		}

		real_matrix_type IM(Np,Np);
		IM = 0.;

		NodesProvisioner.computeInterpMatrix(rout, sout, IM);

		vector<index_vector_type> localE2V;

		index_type numLocalElements =0;
		for (index_type n=0; n < N+1; ++n) {
			for (index_type m=0; m < N+1-(n+1); ++m) {
				index_type v1 = counter(n,m), v2 = counter(n,m+1),
					v3 = counter(n+1, m), v4 = counter(n+1,m+1);

				index_vector_type tri123(3);
				tri123 = v1,v2,v3;
				
				localE2V.push_back(tri123);
				if (v4 >= 0) {
					index_vector_type tri243(3);
					tri243 = v2,v4,v3;
					localE2V.push_back(tri243);
					++numLocalElements;
				}
			
				++numLocalElements;
			}
		}

		vector<index_vector_type> E2Vnew;

		for (index_type k=0; k<K; ++k) {
			index_type shift = k*Np;

			for (index_type l=0; l<numLocalElements; ++l) {
				index_vector_type row(3);
				row(0) = localE2V[l](0) + shift;
				row(1) = localE2V[l](1) + shift;
				row(2) = localE2V[l](2) + shift;
				E2Vnew.push_back(row);
			}
		}

		index_type totalNewElements = numLocalElements*K;

		blitz::firstIndex ii;
		blitz::secondIndex jj;
		blitz::thirdIndex kk;

		real_matrix_type resultx(Np,K), resulty(Np,K), resultField(Np,K);
		resultx = blitz::sum(IM(ii,kk)*x(kk,jj),kk);
		resulty = blitz::sum(IM(ii,kk)*y(kk,jj),kk);
		resultField = blitz::sum(IM(ii,kk)*field(kk,jj),kk);
		

		real_vector_type xVec(Np*K), yVec(Np*K), fieldVec(Np*K);
		fullToVector(resultx, xVec, false);
		fullToVector(resulty, yVec, false);
		fullToVector(resultField, fieldVec, false);

		// Unpack 1D arrays storing EToV and Vertex coordinates
		index_vector_type va(totalNewElements), vb(totalNewElements), vc(totalNewElements);
		for (index_type i=0; i < totalNewElements; ++i) {
				va(i) = E2Vnew[i](0);
				vb(i) = E2Vnew[i](1);
				vc(i) = E2Vnew[i](2);
		}

		// resize arrays for the new linear elements.
		xnew.resize(3, totalNewElements);
		ynew.resize(3, totalNewElements);
		fieldnew.resize(3, totalNewElements);


		for (index_type i=0; i < totalNewElements; ++i) {
			 index_type vai = va(i), vbi = vb(i), vci = vc(i);

				xnew(0,i) = xVec(vai);
				xnew(1,i) = xVec(vbi);
				xnew(2,i) = xVec(vci);

				ynew(0,i) = yVec(vai);
				ynew(1,i) = yVec(vbi);
				ynew(2,i) = yVec(vci);
				
				fieldnew(0,i) = fieldVec(vai);
				fieldnew(1,i) = fieldVec(vbi);
				fieldnew(2,i) = fieldVec(vci);
		}
	}

	void VtkOutputter::writeFieldToFile_numpy(boost::python::str fileName, const boost::python::numpy::ndarray& field, boost::python::str fieldName) const
 	{
		real_matrix_type blitzField(field.shape(0), field.shape(1));
		blitzField = 0.0;
		char * raw = field.get_data();
        std::copy(&raw[0], &raw[field.shape(0)*field.shape(1)*sizeof(real_type)] , reinterpret_cast<char*>(blitzField.data()));

		string fileNameCpp = string(boost::python::extract<const char*>(fileName));
		string fieldNameCpp = string(boost::python::extract<const char*>(fieldName));

		writeFieldToFile(fileNameCpp, blitzField, fieldNameCpp);
	}

	void VtkOutputter::writeFieldsToFiles_numpy(const boost::python::dict& fields, index_type tstep) {

		auto fieldNames = std::vector<const char*>(boost::python::stl_input_iterator<const char *>(fields.keys()),
			stl_input_iterator<const char *>() );

		auto fieldArrays = std::vector<ndarray>(boost::python::stl_input_iterator<ndarray>(fields.values()),
			stl_input_iterator<ndarray>()); 

		index_type count = 0;
		for (const auto& f : fieldNames) {
			const char * fieldNameC = f;
			string fieldNameCpp = fieldNameC;
			string fileName = generateFileName(fieldNameCpp, tstep);
			const ndarray& field = fieldArrays.at(count);
			
			writeFieldToFile_numpy(str(fileName), field, str(fieldNameCpp));
			++count;
		}
	}
}
#endif