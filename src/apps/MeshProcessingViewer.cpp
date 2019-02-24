//=============================================================================
// Copyright (C) 2011-2019 The pmp-library developers
//
// This file is part of the Polygon Mesh Processing Library.
// Distributed under the terms of the MIT license, see LICENSE.txt for details.
//
// SPDX-License-Identifier: MIT
//=============================================================================

#include "MeshProcessingViewer.h"

#include <pmp/algorithms/SurfaceSubdivision.h>
#include <pmp/algorithms/SurfaceFeatures.h>
#include <pmp/algorithms/SurfaceSimplification.h>
#include <pmp/algorithms/SurfaceSmoothing.h>
#include <pmp/algorithms/SurfaceFairing.h>
#include <pmp/algorithms/SurfaceRemeshing.h>
#include <pmp/algorithms/SurfaceCurvature.h>
#include <pmp/algorithms/SurfaceGeodesic.h>

#include <imgui.h>

using namespace pmp;

//=============================================================================

MeshProcessingViewer::MeshProcessingViewer(const char* title, int width, int height)
    : MeshViewer(title, width, height)
{
    set_draw_mode("Hidden Line");

    // add help items
    add_help_item("O", "Flip mesh orientation", 5);
}

//----------------------------------------------------------------------------

void MeshProcessingViewer::keyboard(int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    switch (key)
    {
        case GLFW_KEY_O: // change face orientation
        {
            SurfaceMeshGL new_mesh;
            for (auto v : mesh_.vertices())
            {
                new_mesh.add_vertex(mesh_.position(v));
            }
            for (auto f : mesh_.faces())
            {
                std::vector<Vertex> vertices;
                for (auto v : mesh_.vertices(f))
                {
                    vertices.push_back(v);
                }
                std::reverse(vertices.begin(), vertices.end());
                new_mesh.add_face(vertices);
            }
            mesh_ = new_mesh;
            update_mesh();
            break;
        }

        default:
        {
            MeshViewer::keyboard(key, scancode, action, mods);
            break;
        }
    }
}

//----------------------------------------------------------------------------

void MeshProcessingViewer::process_imgui()
{
    MeshViewer::process_imgui();

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Curvature"))
    {
        if (ImGui::Button("Mean Curvature"))
        {
            SurfaceCurvature analyzer(mesh_);
            analyzer.analyze_tensor(1, true);
            analyzer.mean_curvature_to_texture_coordinates();
            mesh_.use_cold_warm_texture();
            update_mesh();
            set_draw_mode("Texture");
        }
        if (ImGui::Button("Gauss Curvature"))
        {
            SurfaceCurvature analyzer(mesh_);
            analyzer.analyze_tensor(1, true);
            analyzer.gauss_curvature_to_texture_coordinates();
            mesh_.use_cold_warm_texture();
            update_mesh();
            set_draw_mode("Texture");
        }
        if (ImGui::Button("Abs. Max. Curvature"))
        {
            SurfaceCurvature analyzer(mesh_);
            analyzer.analyze_tensor(1, true);
            analyzer.max_curvature_to_texture_coordinates();
            mesh_.use_cold_warm_texture();
            update_mesh();
            set_draw_mode("Texture");
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Smoothing"))
    {
        static int iterations = 10;
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Iterations", &iterations, 1, 100);
        ImGui::PopItemWidth();

        if (ImGui::Button("Explicit Smoothing"))
        {
            SurfaceSmoothing smoother(mesh_);
            smoother.explicit_smoothing(iterations);
            update_mesh();
        }

        ImGui::Spacing();

        static float timestep = 0.001;
        float lb = 0.001;
        float ub = 0.1;
        ImGui::PushItemWidth(100);
        ImGui::SliderFloat("TimeStep", &timestep, lb, ub);
        ImGui::PopItemWidth();

        if (ImGui::Button("Implicit Smoothing"))
        {
            Scalar dt = timestep * radius_ * radius_;
            SurfaceSmoothing smoother(mesh_);
            smoother.implicit_smoothing(dt);
            update_mesh();
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Decimation"))
    {
        static int target_percentage = 10;
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Percentage", &target_percentage, 1, 99);
        ImGui::PopItemWidth();

        static int normal_deviation = 135;
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Normal Deviation", &normal_deviation, 1, 135);
        ImGui::PopItemWidth();

        static int aspect_ratio = 10;
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Aspect Ratio", &aspect_ratio, 1, 10);
        ImGui::PopItemWidth();

        if (ImGui::Button("Decimate it!"))
        {
            SurfaceSimplification ss(mesh_);
            ss.initialize(aspect_ratio, 0.0, 0.0, normal_deviation, 0.0);
            ss.simplify(mesh_.n_vertices() * 0.01 * target_percentage);
            update_mesh();
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Subdivision"))
    {
        if (ImGui::Button("Loop Subdivision"))
        {
            SurfaceSubdivision(mesh_).loop();
            update_mesh();
        }

        if (ImGui::Button("Sqrt(3) Subdivision"))
        {
            SurfaceSubdivision(mesh_).sqrt3();
            update_mesh();
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Remeshing"))
    {
        if (ImGui::Button("Adaptive Remeshing"))
        {
            auto bb = mesh_.bounds().size();
            SurfaceRemeshing(mesh_).adaptive_remeshing(
                0.001 * bb,  // min length
                1.0 * bb,    // max length
                0.001 * bb); // approx. error
            update_mesh();
        }

        if (ImGui::Button("Uniform Remeshing"))
        {
            Scalar l(0);
            for (auto eit : mesh_.edges())
                l += distance(mesh_.position(mesh_.vertex(eit, 0)),
                              mesh_.position(mesh_.vertex(eit, 1)));
            l /= (Scalar)mesh_.n_edges();
            SurfaceRemeshing(mesh_).uniform_remeshing(l);
            update_mesh();
        }
    }
}

//----------------------------------------------------------------------------

void MeshProcessingViewer::mouse(int button, int action, int mods)
{
    if (action == GLFW_PRESS &&
        button == GLFW_MOUSE_BUTTON_MIDDLE &&
        modifiers_ == GLFW_MOD_SHIFT)
    {
        double x, y;
        cursor_pos(x, y);
        Vertex v = pick_vertex(x, y);
        if (mesh_.is_valid(v))
        {
            // setup seed
            std::vector<Vertex> seed;
            seed.push_back(v);

            // compute geodesic distance
            SurfaceGeodesic geodist(mesh_, seed);

            // setup texture coordinates for visualization
            geodist.distance_to_texture_coordinates();
            mesh_.use_checkerboard_texture();
            update_mesh();
            set_draw_mode("Texture");
        }
    }
    else
    {
        MeshViewer::mouse(button, action, mods);
    }
}

//=============================================================================
