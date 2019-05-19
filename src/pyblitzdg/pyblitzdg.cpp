// Copyright (C) 2017-2019  Waterloo Quantitative Consulting Group, Inc.
// See COPYING and LICENSE files at project root for more details.

/**
 * @file pyblitzdg.cpp
 * @brief Python bindings for blitzdg's public API.
 */

#include <boost/python.hpp>
#include "Nodes1DProvisioner.hpp"
#include "Types.hpp"

using namespace boost::python;
using namespace boost::python::numpy;


BOOST_PYTHON_MODULE(pyblitzdg)
{
    using namespace boost::python;
    using namespace blitzdg;

    boost::python::numpy::initialize();

    class_<real_matrix_type>("real_matrix_type", init<index_type>());

    class_<Nodes1DProvisioner, boost::noncopyable>("Nodes1DProvisioner", init<index_type, index_type, real_type, real_type>())
        .def("buildNodes", &Nodes1DProvisioner::buildNodes)
        .def("computeJacobian", &Nodes1DProvisioner::computeJacobian)
        .add_property("numLocalPoints", &Nodes1DProvisioner::get_NumLocalPoints)
        .add_property("xGrid", &Nodes1DProvisioner::get_xGrid_numpy)
        .add_property("Dr", &Nodes1DProvisioner::get_Dr_numpy);
        
    //    .def

}


