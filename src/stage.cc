/*
Copyright (c) 2019 - Present, Syoyo Fujita.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Syoyo Fujita nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <algorithm>
#include <atomic>
//#include <cassert>
#include <cctype>  // std::tolower
#include <chrono>
#include <fstream>
#include <map>
#include <sstream>

//
#ifndef __wasi__
#include <thread>
#endif
//
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "tinyusdz.hh"
#include "usdLux.hh"
#include "usdShade.hh"
#include "pprinter.hh"
#include "value-pprint.hh"
#include "str-util.hh"
//
#include "common-macros.inc"

namespace tinyusdz {

#if 0
// For PUSH_ERROR_AND_RETURN
#define PushError(s) \
  if (err) {         \
    (*err) += s;     \
  }
//#define PushWarn(s) if (warn) { (*warn) += s; }
#endif

namespace {

#if 0  // not used yet

///
/// Node represents scene graph node.
/// This does not contain leaf node inormation.
///
class Node {
 public:
  // -2 = initialize as invalid node
  Node() : _parent(-2) {}

  Node(int64_t parent, Path &path) : _parent(parent), _path(path) {}

  int64_t GetParent() const { return _parent; }

  const std::vector<size_t> &GetChildren() const { return _children; }

  ///
  /// child_name is used when reconstructing scene graph.
  ///
  bool AddChildren(const std::string &child_name, size_t node_index) {
    if (_primChildren.count(child_name)) {
      return false;
    }
    //assert(_primChildren.count(child_name) == 0);
    _primChildren.emplace(child_name);
    _children.push_back(node_index);

    return true;
  }

  ///
  /// Get full path(e.g. `/muda/dora/bora` when the parent is `/muda/dora` and
  /// this node is `bora`)
  ///
  // std::string GetFullPath() const { return _path.full_path_name(); }

  ///
  /// Get local path
  ///
  std::string GetLocalPath() const { return _path.full_path_name(); }

  const Path &GetPath() const { return _path; }

  // NodeType GetNodeType() const { return _node_type; }

  const std::unordered_set<std::string> &GetPrimChildren() const {
    return _primChildren;
  }

  void SetAssetInfo(const value::dict &dict) { _assetInfo = dict; }

  const value::dict &GetAssetInfo() const { return _assetInfo; }

 private:
  int64_t
      _parent;  // -1 = this node is the root node. -2 = invalid or leaf node
  std::vector<size_t> _children;                  // index to child nodes.
  std::unordered_set<std::string> _primChildren;  // List of name of child nodes

  Path _path;  // local path
  value::dict _assetInfo;

  // NodeType _node_type;
};
#endif

}  // namespace

///
/// Stage
///


//
// TODO: Move to prim-types.cc
//

namespace {

nonstd::optional<Path> GetPath(const value::Value &v) {
  // Since multiple get_value() call consumes lots of stack size(depends on sizeof(T)?),
  // Following code would produce 100KB of stack in debug build.
  // So use as() instead(as() => roughly 2000 bytes for stack size).
#if 0
  //
  // TODO: Find a better C++ way... use a std::function?
  //
  if (auto pv = v.get_value<Model>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<Scope>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<Xform>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GPrim>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomMesh>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomBasisCurves>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomSphere>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomCube>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomCylinder>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomCapsule>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomCone>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomSubset>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<GeomCamera>()) {
    return Path(pv.value().name, "");
  }

  if (auto pv = v.get_value<LuxDomeLight>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<LuxSphereLight>()) {
    return Path(pv.value().name, "");
  }
  // if (auto pv = v.get_value<LuxCylinderLight>()) { return
  // Path(pv.value().name); } if (auto pv = v.get_value<LuxDiskLight>()) {
  // return Path(pv.value().name); }

  if (auto pv = v.get_value<Material>()) {
    return Path(pv.value().name, "");
  }
  if (auto pv = v.get_value<Shader>()) {
    return Path(pv.value().name, "");
  }
  // if (auto pv = v.get_value<UVTexture>()) { return Path(pv.value().name); }
  // if (auto pv = v.get_value<PrimvarReader()) { return Path(pv.value().name);
  // }
#else

#define EXTRACT_NAME_AND_RETURN_PATH(__ty) if (v.as<__ty>()) { return Path(v.as<__ty>()->name, ""); }

  EXTRACT_NAME_AND_RETURN_PATH(Model)
  EXTRACT_NAME_AND_RETURN_PATH(Scope)
  EXTRACT_NAME_AND_RETURN_PATH(Xform)
  EXTRACT_NAME_AND_RETURN_PATH(GPrim)
  EXTRACT_NAME_AND_RETURN_PATH(GeomMesh)
  EXTRACT_NAME_AND_RETURN_PATH(GeomPoints)
  EXTRACT_NAME_AND_RETURN_PATH(GeomCube)
  EXTRACT_NAME_AND_RETURN_PATH(GeomCapsule)
  EXTRACT_NAME_AND_RETURN_PATH(GeomCylinder)
  EXTRACT_NAME_AND_RETURN_PATH(GeomSphere)
  EXTRACT_NAME_AND_RETURN_PATH(GeomCone)
  EXTRACT_NAME_AND_RETURN_PATH(GeomSubset)
  EXTRACT_NAME_AND_RETURN_PATH(GeomCamera)
  EXTRACT_NAME_AND_RETURN_PATH(GeomBasisCurves)
  EXTRACT_NAME_AND_RETURN_PATH(LuxDomeLight)
  EXTRACT_NAME_AND_RETURN_PATH(LuxSphereLight)
  EXTRACT_NAME_AND_RETURN_PATH(LuxCylinderLight)
  EXTRACT_NAME_AND_RETURN_PATH(LuxDiskLight)
  EXTRACT_NAME_AND_RETURN_PATH(LuxRectLight)
  EXTRACT_NAME_AND_RETURN_PATH(Material)
  EXTRACT_NAME_AND_RETURN_PATH(Shader)
  EXTRACT_NAME_AND_RETURN_PATH(UsdPreviewSurface)
  EXTRACT_NAME_AND_RETURN_PATH(UsdUVTexture)

  // TODO: primvar reader
  //EXTRACT_NAME_AND_RETURN_PATH(UsdPrimvarReader_float);

#undef EXTRACT_NAME_AND_RETURN_PATH


#endif

  return nonstd::nullopt;
}

} // namespace local

Prim::Prim(const value::Value &rhs) {
  // Check if Prim type is Model(GPrim)
  if ((value::TypeId::TYPE_ID_MODEL_BEGIN <= rhs.type_id()) &&
      (value::TypeId::TYPE_ID_MODEL_END > rhs.type_id())) {
    if (auto pv = GetPath(rhs)) {
      path = pv.value();
    }

    data = rhs;
  } else {
    // TODO: Raise an error if rhs is not an Prim
  }
}

Prim::Prim(value::Value &&rhs) {
  // Check if Prim type is Model(GPrim)
  if ((value::TypeId::TYPE_ID_MODEL_BEGIN <= rhs.type_id()) &&
      (value::TypeId::TYPE_ID_MODEL_END > rhs.type_id())) {
    data = std::move(rhs);

    if (auto pv = GetPath(data)) {
      path = pv.value();
    }

  } else {
    // TODO: Raise an error if rhs is not an Prim
  }
}

namespace {

nonstd::optional<const Prim *> GetPrimAtPathRec(const Prim *parent,
                                                const Path &path) {
  //// TODO: Find better way to get path name from any value.
  // if (auto pv = parent.get_value<Xform>)
  if (auto pv = GetPath(parent->data)) {
    if (path == pv.value()) {
      return parent;
    }
  }

  for (const auto &child : parent->children) {
    if (auto pv = GetPrimAtPathRec(&child, path)) {
      return pv.value();
    }
  }

  return nonstd::nullopt;
}

}  // namespace

nonstd::expected<const Prim *, std::string> Stage::GetPrimAtPath(
    const Path &path) {
  if (_dirty) {
    // Clear cache.
    _prim_path_cache.clear();
  } else {
    // First find from a cache.
  }

  if (!path.IsValid()) {
    return nonstd::make_unexpected("Path is invalid.\n");
  }

  if (path.IsRelativePath()) {
    // TODO:
    return nonstd::make_unexpected("Relative path is TODO.\n");
  }

  if (!path.IsAbsolutePath()) {
    return nonstd::make_unexpected(
        "Path is not absolute. Non-absolute Path is TODO.\n");
  }

  // Brute-force search.
  // TODO: Build path -> Node lookup table
  for (const auto &parent : root_nodes) {
    if (auto pv = GetPrimAtPathRec(&parent, path)) {
      return pv.value();
    }
  }

  return nonstd::make_unexpected("Cannot find path <" + path.full_path_name() +
                                 "> int the Stage.\n");
}

namespace {

void PrimPrintRec(std::stringstream &ss, const Prim &prim, uint32_t indent) {
  ss << "\n";
  ss << pprint_value(prim.data, indent, /* closing_brace */ false);

  DCOUT("num_children = " << prim.children.size());
  for (const auto &child : prim.children) {
    PrimPrintRec(ss, child, indent + 1);
  }

  ss << pprint::Indent(indent) << "}\n";
}

}  // namespace

std::string Stage::ExportToString() const {
  std::stringstream ss;

  ss << "#usda 1.0\n";
  ss << "(\n";
  if (stage_metas.doc.value.empty()) {
    ss << "  doc = \"Exporterd from TinyUSDZ v" << tinyusdz::version_major
       << "." << tinyusdz::version_minor << "." << tinyusdz::version_micro
       << tinyusdz::version_rev << "\"\n";
  } else {
    ss << "  doc = " << to_string(stage_metas.doc) << "\n";
  }

  if (stage_metas.metersPerUnit.authored()) {
    ss << "  metersPerUnit = " << stage_metas.metersPerUnit.GetValue() << "\n";
  }

  if (stage_metas.upAxis.authored()) {
    ss << "  upAxis = " << quote(to_string(stage_metas.upAxis.GetValue()))
       << "\n";
  }

  if (stage_metas.timeCodesPerSecond.authored()) {
    ss << "  timeCodesPerSecond = " << stage_metas.timeCodesPerSecond.GetValue()
       << "\n";
  }

  if (stage_metas.startTimeCode.authored()) {
    ss << "  startTimeCode = " << stage_metas.startTimeCode.GetValue() << "\n";
  }

  if (stage_metas.endTimeCode.authored()) {
    ss << "  endTimeCode = " << stage_metas.endTimeCode.GetValue() << "\n";
  }

  // TODO: Do not print subLayers when consumed(after composition evaluated)
  if (stage_metas.subLayers.size()) {
    ss << "  subLayers = " << stage_metas.subLayers << "\n";
  }

  if (stage_metas.defaultPrim.str().size()) {
    ss << "  defaultPrim = " << tinyusdz::quote(stage_metas.defaultPrim.str())
       << "\n";
  }
  if (!stage_metas.comment.value.empty()) {
    ss << "  doc = " << to_string(stage_metas.comment) << "\n";
  }

  if (stage_metas.customLayerData.size()) {
    ss << print_customData(stage_metas.customLayerData, "customLayerData",
                           /* indent */ 1);
  }

  // TODO: Sort by line_no?(preserve appearance in read USDA)
  for (const auto &item : stage_metas.stringData) {
    ss << "  " << to_string(item) << "\n";
  }

  // TODO: write other header data.
  ss << ")\n";
  ss << "\n";

  for (const auto &item : root_nodes) {
    PrimPrintRec(ss, item, 0);
  }

  return ss.str();
}

}  // namespace tinyusdz
