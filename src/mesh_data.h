#ifndef MESH_DATA_H_
#define MESH_DATA_H_

#include "spider/spider.h"


vec3 plane_vertices[] = {
    {-0.5f,  0.0f,  0.5f},
    { 0.5f,  0.0f,  0.5f},
    { 0.5f,  0.0f, -0.5f},
    {-0.5f,  0.0f, -0.5f},
};

vec2 plane_tex_coords[] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f} 
};

SPTriangle plane_faces[] = {
    {
        .vertex_indices = {0, 1, 2},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {2, 3, 0},
        .tex_coord_indices = {2, 3, 0}
    },
};

const SPMeshInitializerDesc plane = {
    .vertices = {
        .data = plane_vertices,
        .count = SP_ARRAY_LEN(plane_vertices),
    },
    .tex_coords = {
        .data = plane_tex_coords,
        .count = SP_ARRAY_LEN(plane_tex_coords),
    },
    .faces = {
        .data = plane_faces,
        .count = SP_ARRAY_LEN(plane_faces)
    }
};

vec3 cube_vertices[] = {
    {-0.5f,  0.5f,  0.5f}, // LTB 0
    { 0.5f,  0.5f,  0.5f}, // RTB 1
    { 0.5f,  0.5f, -0.5f}, // RTF 2
    {-0.5f,  0.5f, -0.5f}, // LTF 3
    {-0.5f, -0.5f,  0.5f}, // LBB 4
    { 0.5f, -0.5f,  0.5f}, // RBB 5
    { 0.5f, -0.5f, -0.5f}, // RBF 6
    {-0.5f, -0.5f, -0.5f}, // LBF 7
};

vec2 cube_tex_coords[] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
};

SPTriangle cube_faces[] = {
    // Top
    {
        .vertex_indices = {0, 1, 2},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {2, 3, 0},
        .tex_coord_indices = {2, 3, 0}
    },
    // Front
    {
        .vertex_indices = {1, 0, 4},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {4, 5, 1},
        .tex_coord_indices = {2, 3, 0}
    },
    // Left
    {
        .vertex_indices = {0, 3, 7},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {7, 4, 0},
        .tex_coord_indices = {2, 3, 0}
    },
    // Back
    {
        .vertex_indices = {3, 2, 6},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {6, 7, 3},
        .tex_coord_indices = {2, 3, 0}
    },
    // Right
    {
        .vertex_indices = {2, 1, 5},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {5, 6, 2},
        .tex_coord_indices = {2, 3, 0}
    },
    // Bottom
    {
        .vertex_indices = {6, 5, 4},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {4, 7, 6},
        .tex_coord_indices = {2, 3, 0}
    },
};

const SPMeshInitializerDesc cube = {
    .vertices = {
        .data = cube_vertices,
        .count = SP_ARRAY_LEN(cube_vertices),
    },
    .tex_coords = {
        .data = cube_tex_coords,
        .count = SP_ARRAY_LEN(cube_tex_coords),
    },
    .faces = {
        .data = cube_faces,
        .count = SP_ARRAY_LEN(cube_faces)
    }
};

#endif