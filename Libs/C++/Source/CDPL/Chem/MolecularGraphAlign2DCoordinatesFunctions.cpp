/* 
 * MolecularGraphAlign2DCoordinatesFunctions.cpp 
 *
 * This file is part of the Chemical Data Processing Toolkit
 *
 * Copyright (C) 2003 Thomas Seidel <thomas.seidel@univie.ac.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; see the file COPYING. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include "StaticInit.hpp"

#include <cmath>

#include "CDPL/Chem/MolecularGraphFunctions.hpp"
#include "CDPL/Chem/AtomContainerFunctions.hpp"
#include "CDPL/Chem/Bond.hpp"
#include "CDPL/Chem/AtomFunctions.hpp"
#include "CDPL/Chem/BondFunctions.hpp"
#include "CDPL/Chem/BondStereoFlag.hpp"
#include "CDPL/Math/KabschAlgorithm.hpp"


using namespace CDPL; 


namespace
{

    double calcError(const Chem::AtomContainer& ref_atoms, const Math::Vector2DArray& ref_coords, const Math::Matrix3D& xform)
    {
        auto err = 0.0;

        std::size_t num_ref_atoms = ref_atoms.getNumAtoms();
     
        for (std::size_t i = 0; i < num_ref_atoms; i++) {
            auto& ref_atom = ref_atoms.getAtom(i);
            auto& ref_atom_coords = get2DCoordinates(ref_atom);

            auto trans_x = ref_atom_coords[0] * xform(0, 0) + ref_atom_coords[1] * xform(0, 1) + xform(0, 2);
            auto trans_y = ref_atom_coords[0] * xform(1, 0) + ref_atom_coords[1] * xform(1, 1) + xform(1, 2);

            auto d_x = (trans_x - ref_coords[i][0]);
            auto d_y = (trans_y - ref_coords[i][1]);
            
            err += d_x * d_x + d_y * d_y;
        }

        return std::sqrt(err / (2 * num_ref_atoms));
    }
}

bool Chem::align2DCoordinates(MolecularGraph& molgraph, const AtomContainer& ref_atoms, const Math::Vector2DArray& ref_coords, bool fix_bond_stereo)
{
    std::size_t num_ref_atoms = ref_atoms.getNumAtoms();
    
    if (num_ref_atoms == 0)
        return false;

    Math::DMatrix ref_coords_mtx(2, num_ref_atoms);
    Math::DMatrix algnd_coords_mtx(2, num_ref_atoms);
    
    for (std::size_t i = 0; i < num_ref_atoms; i++) {
        auto& ref_atom = ref_atoms.getAtom(i);

        column(ref_coords_mtx, i) = ref_coords[i];
        column(algnd_coords_mtx, i) = get2DCoordinates(ref_atom);
    }

    Math::KabschAlgorithm<double> kabsch_algo;
     
    if (!kabsch_algo.align(algnd_coords_mtx, ref_coords_mtx))
        return false;

    Math::Matrix3D xform1 = kabsch_algo.getTransform();
    auto error1 = calcError(ref_atoms, ref_coords, xform1);

    for (std::size_t i = 0; i < num_ref_atoms; i++) {
        auto& ref_atom = ref_atoms.getAtom(i);
        auto& ref_atom_coords = get2DCoordinates(ref_atom);
        
        column(ref_coords_mtx, i) = ref_coords[i];
        algnd_coords_mtx(0, i) = -ref_atom_coords[0];
        algnd_coords_mtx(1, i) = ref_atom_coords[1];
    }

    if (!kabsch_algo.align(algnd_coords_mtx, ref_coords_mtx))
        return false;

    Math::Matrix3D xform2 = kabsch_algo.getTransform();
    auto error2 = calcError(ref_atoms, ref_coords, xform2);

    if (fix_bond_stereo && (error2 < error1)) {
        for (auto& bond : molgraph.getBonds()) {
            if (!has2DStereoFlag(bond))
                continue;

            switch (get2DStereoFlag(bond)) {

                case BondStereoFlag::UP:
                    set2DStereoFlag(bond, BondStereoFlag::DOWN);
                    continue;
                    
                case BondStereoFlag::REVERSE_UP:
                    set2DStereoFlag(bond, BondStereoFlag::REVERSE_DOWN);
                    continue;
                    
                case BondStereoFlag::DOWN:
                    set2DStereoFlag(bond, BondStereoFlag::UP);
                    continue;
                    
                case BondStereoFlag::REVERSE_DOWN:
                    set2DStereoFlag(bond, BondStereoFlag::REVERSE_UP);
            }
        }
    }
    
    transform2DCoordinates(molgraph, (error1 <= error2 ? xform1 : xform2));

    return true;
}
