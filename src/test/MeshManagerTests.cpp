// Copyright (C) 2017-2018  Waterloo Quantitative Consulting Group, Inc.
// See COPYING and LICENSE files at project root for more details.

#include "MeshManager.hpp"
#include "Types.hpp"
#include <igloo/igloo_alt.h>
#include <boost/algorithm/string.hpp>
#include <whereami.h>
#include <string>
#include <vector>

using boost::algorithm::find_all;
using boost::algorithm::join;
using boost::algorithm::replace_last;
using boost::algorithm::trim_right;
using boost::iterator_range;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace blitzdg {
    namespace MeshManagerTests {
        using namespace igloo;
        Describe(MeshManager_Object) {
            using find_vector_type = vector<iterator_range<string::iterator>>;

            MeshManager * meshManager=nullptr;
            string *ExePath = nullptr;

            void SetUp() {
                meshManager = new MeshManager();
                if (ExePath == nullptr) {
                    // Deal with paths to the test input files.
                    index_type cap = 1024;
                    char * pathBuffer = new char[cap];
                    index_type length = wai_getExecutablePath(pathBuffer, cap, NULL);
                    ExePath = new string();

                    for(index_type i=0; i < length; i++) {
                        *ExePath += pathBuffer[i]; 
                    }
                    trim_right(*ExePath);
                    delete [] pathBuffer;
                }
            }
            void TearDown() {
                delete ExePath;
                delete meshManager;
            }

            void get_VertexFilePath(string & vertFile) {    
                find_vector_type FindVec;

                string PathDelimeter = "/";
                string path(*ExePath);
                replace_last(path, ".exe", "");
                replace_last(path, "/bin/test", "");
                find_all( FindVec, path, "\\" );
                if (FindVec.size() > 0) {
                    PathDelimeter = "\\";
                    replace_last(path, "\\bin\\test", "");
                }

                vector<string> pathVec;
                pathVec.push_back(path);
                pathVec.push_back("input");
                pathVec.push_back("2box.V");

                vertFile = join(pathVec, PathDelimeter);
            }

            void get_EToVFilePath(string & eToVFile) {
                string path(*ExePath);
                find_vector_type FindVec;

                string PathDelimeter = "/";
                replace_last(path, ".exe", "");
                replace_last(path, "/bin/test", "");
                find_all( FindVec, path, "\\" );
                if (FindVec.size() > 0) {
                    PathDelimeter = "\\";
                    replace_last(path, "\\bin\\test", "");
                }

                vector<string> pathVec;
                pathVec.push_back(path);
                pathVec.push_back("input");
                pathVec.push_back("2box.E2V");
                eToVFile = join(pathVec, PathDelimeter);
            }

            It(Reads_Vertex_Files) {
                string vertexFile = "";
                get_VertexFilePath(vertexFile);
                cout << *ExePath << endl;

                cout << "MeshManager Reads Vertex File: " << vertexFile << endl;
                MeshManager & mgr = *meshManager;

                mgr.readVertices(vertexFile);

                Assert::That(mgr.get_NumVerts(), Equals(6));
                Assert::That(mgr.get_Dim(), Equals(2));

                const real_vector_type& verts = mgr.get_Vertices();

                Assert::That(verts(0), Equals(0.0));
                Assert::That(verts(1), Equals(0.0));

                Assert::That(verts(2), Equals(0.5));
                Assert::That(verts(3), Equals(0.0));

                Assert::That(verts(4), Equals(1.0));
                Assert::That(verts(5), Equals(0.0));

                Assert::That(verts(6), Equals(1.0));
                Assert::That(verts(7), Equals(1.0));

                Assert::That(verts(8), Equals(0.5));
                Assert::That(verts(9), Equals(1.0));

                Assert::That(verts(10), Equals(0.0));
                Assert::That(verts(11), Equals(1.0));
            }

            It(Reads_Element_Files) {
                string eToVFile = "";
                get_EToVFilePath(eToVFile);

                cout << "MeshManager Reads Elements Files: " << eToVFile << endl;
                MeshManager & mgr = *meshManager;

                mgr.readElements(eToVFile);

                Assert::That(mgr.get_NumElements(), Equals(2));
                Assert::That(mgr.get_ElementType(), Equals(4));

                const index_vector_type& elements = mgr.get_Elements();

                Assert::That(elements(0), Equals(0));
                Assert::That(elements(1), Equals(1));
                Assert::That(elements(2), Equals(4));
                Assert::That(elements(3), Equals(5));
                Assert::That(elements(4), Equals(1));
                Assert::That(elements(5), Equals(2));
                Assert::That(elements(6), Equals(3));
                Assert::That(elements(7), Equals(4));
            }

            It(Can_Print_Vertices_And_DoesNotThrow) {
                string vertexFile = "";
                get_VertexFilePath(vertexFile);
                cout << "Can_Print_Vertices_And_DoesNotThrow" << endl;
                cout << "MeshManager Reads Vertex File: " << vertexFile << endl;

                MeshManager & mgr = *meshManager;   
                mgr.readVertices(vertexFile);
                cout << "Vertices:" << endl;
                mgr.printVertices();
            }

            It(Can_Print_Elements_And_DoesNotThrow) {
                string eToVFile = "";
                get_EToVFilePath(eToVFile);
                cout << "Can_Print_Elements_And_DoesNotThrow" << endl;
                cout << "MeshManager Reads Element File: " << eToVFile << endl;

                MeshManager & mgr = *meshManager;
                mgr.readElements(eToVFile);
                cout << endl << "Elements" << endl;
                mgr.printElements();
            }

            It(Can_Partition_A_Mesh) {
                cout << "Can_Partition_A_Mesh" << endl;
                string eToVFile = ""; 
                string vertexFile = "" ;
                get_EToVFilePath(eToVFile);
                get_VertexFilePath(vertexFile);

                MeshManager & mgr = *meshManager;
                mgr.readVertices(vertexFile);
                mgr.readElements(eToVFile);

                cout << "K: " << mgr.get_NumElements() << endl;
                cout << "Nv: " << mgr.get_NumVerts() << endl;
                mgr.partitionMesh(2);

                const index_vector_type& epMap = mgr.get_ElementPartitionMap();
                Assert::That(epMap(0), Equals(1));
                Assert::That(epMap(1), Equals(0));

                const index_vector_type& vpMap = mgr.get_VertexPartitionMap();
                Assert::That(vpMap(0), IsGreaterThan(-1));
                Assert::That(vpMap(1), IsGreaterThan(-1));
                Assert::That(vpMap(2), IsGreaterThan(-1));
                Assert::That(vpMap(3), IsGreaterThan(-1));
                Assert::That(vpMap(4), IsGreaterThan(-1));
                Assert::That(vpMap(5), IsGreaterThan(-1));
            } 
        };
    } // namespace MeshManagerTests
} // namespace blitzdg

