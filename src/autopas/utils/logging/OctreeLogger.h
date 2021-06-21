/**
 * @file OctreeLogger.h
 * @author Johannes Spies
 * @date 21.04.2021
 */

#pragma once

#include <string.h>

#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "autopas/containers/octree/OctreeNodeInterface.h"

namespace autopas {
/**
 * Log an octree to a .vtk file
 */
class OctreeLogger {
 public:
  /**
   * Constructor
   */
  explicit OctreeLogger();

  /**
   * Destructor
   */
  ~OctreeLogger();

  /**
   * This function writes the octree to a .vtk file
   * @tparam Particle The enclosed particle type
   * @param root A pointer to the octree root node
   */
  template <typename Particle>
  void logTree(OctreeNodeInterface<Particle> *root) {
    // Load the leaf boxes
    using Position = std::array<double, 3>;
    using Box = std::pair<Position, Position>;
    std::vector<Box> boxes;
    root->appendAllLeafBoxes(boxes);
    auto boxCount = boxes.size();
    auto pointCount = 8 * boxCount;

    // Open the VTK file
    char filename[256] = {0};
    snprintf(filename, sizeof(filename), "octree_%d.vtk", iteration++);
    std::ofstream vtkFile;
    vtkFile.open(filename);

    if (not vtkFile.is_open()) {
      throw std::runtime_error("OctreeLogger::logTree(): Failed to open file \"" + std::string(filename) + "\".");
    }

    // Write the header
    vtkFile << "# vtk DataFile Version 2.0\n"
            << "Octree boxes\n"
            << "ASCII\n"

            << "DATASET UNSTRUCTURED_GRID\n"
            << "\n";

    // Write points
    vtkFile << "POINTS " << pointCount << " float\n";  // Points header
    for (Box box : boxes) {
      Position min = box.first;
      Position max = box.second;

      auto [minX, minY, minZ] = min;
      auto [maxX, maxY, maxZ] = max;

      // Write the points in the order of the VTK_HEXAHEDRON. Each point is
      // written on its own line.

      vtkFile << minX << " " << minY << " " << minZ << "\n";  // 0 ---
      vtkFile << maxX << " " << minY << " " << minZ << "\n";  // 1 +--
      vtkFile << maxX << " " << maxY << " " << minZ << "\n";  // 2 ++-
      vtkFile << minX << " " << maxY << " " << minZ << "\n";  // 3 -+-
      vtkFile << minX << " " << minY << " " << maxZ << "\n";  // 4 --+
      vtkFile << maxX << " " << minY << " " << maxZ << "\n";  // 5 +-+
      vtkFile << maxX << " " << maxY << " " << maxZ << "\n";  // 6 +++
      vtkFile << minX << " " << maxY << " " << maxZ << "\n";  // 7 -++
    }
    vtkFile << "\n";

    // Write cells
    auto cellListSize = pointCount + boxCount;
    vtkFile << "CELLS " << boxCount << " " << cellListSize << "\n";
    for (auto boxIndex = 0; boxIndex < boxCount; ++boxIndex) {
      vtkFile << "8 ";  // Output # of elements in the following line.
      for (auto pointIndex = 0; pointIndex < 8; ++pointIndex) {
        // Generate an index that points to the corresponding point in the points list.
        auto offset = 8 * boxIndex + pointIndex;
        vtkFile << offset;
        if (pointIndex < 7) {
          vtkFile << " ";
        }
      }
      vtkFile << "\n";
    }
    vtkFile << "\n";

    // Write cell types
    vtkFile << "CELL_TYPES " << boxCount << "\n";
    for (Box box : boxes) {
      vtkFile << "12\n";  // Write VTK_HEXAHEDRON type for each cell
    }

    // Cleanup
    vtkFile.close();

    // TODO(johannes): Enclose with macro
    //#ifdef AUTOPAS_LOG_OCTREE
    //#endif
  }

  /**
   * Convert a list of octree leaves to JSON and write it to an output file. The output list consists of JSON objects
   * containing the following fields:
   * - `"minmax"`\n
   *   A list of numbers specifying the concatenated minimum and maximum coordinate of the leaf box in 3D.
   *   This is how the coordinate list is encoded in JSON: [x1,y1,z1,x2,y2,z2].
   * - `"fn"`\n
   *   A list of min/max coordinates of all greater than or equal face-neighbors of the leaf
   * - `"fnl"`\n
   *   A list of min/max coordinates of all leaves that touch this leaf via a face.
   * - `"en"`\n
   *   A list of min/max coordinates of all greater than or equal edge-neighbors of the leaf
   * - `"enl"`\n
   *   A list of min/max coordinates of all leaves that touch this leaf via a edge.
   * - `"vn"`\n
   *   A list of min/max coordinates of all greater than or equal vertex-neighbors of the leaf
   * - `"vnl"`\n
   *   A list of min/max coordinates of all leaves that touch this leaf via a vertex.
   *
   * @tparam Particle The enclosed particle type
   * @param out A FILE pointer to the file that should contain the JSON data after the operation
   * @param leaves A list of octree leaves that are echoed into the JSON file
   * @return The FILE pointer is just passed through
   */
  template <typename Particle>
  static FILE *leavesToJSON(FILE *out, std::vector<OctreeLeafNode<Particle> *> &leaves) {
    if (out) {
      fprintf(out, "[\n");
      for (int leafIndex = 0; leafIndex < leaves.size(); ++leafIndex) {
        auto leaf = leaves[leafIndex];
        fprintf(out, "{\"minmax\": ");
        outLocationArrayJSON(out, leaf);

        // Print face neighbors
        fprintf(out, ", \"fn\": [");
        bool first = true;
        for (Face *face = getFaces(); *face != O; ++face) {
          auto neighbor = leaf->GTEQ_FACE_NEIGHBOR(*face);
          if (neighbor) {
            if (!first) {
              fprintf(out, ", ");
            }
            first = false;
            outLocationArrayJSON(out, neighbor);
          }
        }

        // Print face neighbor leaves
        fprintf(out, "], \"fnl\": [");
        first = true;
        for (Face *face = getFaces(); *face != O; ++face) {
          auto neighbor = leaf->GTEQ_FACE_NEIGHBOR(*face);
          if (neighbor) {
            auto neighborLeaves = neighbor->getNeighborLeaves(*face);
            for (auto neighborLeaf : neighborLeaves) {
              if (!first) {
                fprintf(out, ", ");
              }
              first = false;
              outLocationArrayJSON(out, neighborLeaf);
            }
          }
        }

        // Print edge neighbors
        fprintf(out, "], \"en\": [");
        first = true;
        for (Edge *edge = getEdges(); *edge != OO; ++edge) {
          auto neighbor = leaf->GTEQ_EDGE_NEIGHBOR(*edge);
          if (neighbor) {
            if (!first) {
              fprintf(out, ", ");
            }
            first = false;
            outLocationArrayJSON(out, neighbor);
          }
        }

        // Print edge neighbor leaves
        fprintf(out, "], \"enl\": [");
        first = true;
        for (Edge *edge = getEdges(); *edge != OO; ++edge) {
          auto neighbor = leaf->GTEQ_EDGE_NEIGHBOR(*edge);
          if (neighbor) {
            auto neighborLeaves = neighbor->getNeighborLeaves(*edge);
            for (auto neighborLeaf : neighborLeaves) {
              if (!first) {
                fprintf(out, ", ");
              }
              first = false;
              outLocationArrayJSON(out, neighborLeaf);
            }
          }
        }

        // Print vertex neighbors
        fprintf(out, "], \"vn\": [");
        first = true;
        for (Vertex *vertex = VERTICES(); *vertex != OOO; ++vertex) {
          auto neighbor = leaf->GTEQ_VERTEX_NEIGHBOR(*vertex);
          if (neighbor) {
            if (!first) {
              fprintf(out, ", ");
            }
            first = false;
            outLocationArrayJSON(out, neighbor);
          }
        }

        // Print vertex neighbor leaves
        fprintf(out, "], \"vnl\": [");
        first = true;
        for (Vertex *vertex = VERTICES(); *vertex != OOO; ++vertex) {
          auto neighbor = leaf->GTEQ_VERTEX_NEIGHBOR(*vertex);
          if (neighbor) {
            auto neighborLeaves = neighbor->getNeighborLeaves(*vertex);

            for (auto neighborLeaf : neighborLeaves) {
              if (!first) {
                fprintf(out, ", ");
              }
              first = false;
              outLocationArrayJSON(out, neighborLeaf);
            }
          }
        }

        fprintf(out, "]}");
        if (leafIndex < (leaves.size() - 1)) {
          fprintf(out, ",");
        }
        fprintf(out, "\n");
      }
      fprintf(out, "]\n");
    } else {
      fprintf(stderr, "ERROR: Dump file is nullptr.\n");
    }

    return out;
  }

 private:
  /**
   * Print the box minimum and maximum coordinates to a given FILE pointer as a JSON list of the form
   * `[min_x, min_y, min_z, max_x, max_y, max_z]`.
   * @tparam Particle The enclosed particle type
   * @param out The FILE pointer
   * @param node An octree node to obtain the box minimum and maximum coordinates from
   */
  template <typename Particle>
  static void outLocationArrayJSON(FILE *out, OctreeNodeInterface<Particle> *node) {
    auto min = node->getBoxMin();
    auto max = node->getBoxMax();
    fprintf(out, "[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f]", min[0], min[1], min[2], max[0], max[1], max[2]);
  }

  /**
   * Count the iterations to give the written octrees unique filenames
   */
  int unsigned iteration = 0;
};
}  // namespace autopas
