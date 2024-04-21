// SPDX-License-Identifier: Apache 2.0
// Copyright 2024 - Present, Light Transport Entertainment Inc.
//
#include <sstream>
#include <numeric>
#include <unordered_set>

#include "usd-export.hh"
#include "common-macros.inc"
#include "tiny-format.hh"

namespace tinyusdz {
namespace tydra {

#define PushError(msg) { \
  if (err) { \
    (*err) += msg + "\n"; \
  } \
}

namespace detail {

static void CountNodes(const SkelNode &node, size_t &count) {
  count++;

  for (const auto &child : node.children) {
    CountNodes(child, count);
  } 
}

static bool FlattenSkelNode(const SkelNode &node,
  std::vector<value::token> &joints,
  std::vector<value::token> &jointNames,
  std::vector<value::matrix4d> &bindTransforms,
  std::vector<value::matrix4d> &restTransforms, std::string *err) {

  size_t idx = size_t(node.joint_id);
  if (idx >= joints.size()) {
    if (err) {
      (*err) += "joint_id out-of-bounds.";
    }
    return false;
  }

  joints[idx] = value::token(node.joint_path);
  jointNames[idx] = value::token(node.joint_name);
  bindTransforms[idx] = node.bind_transform;
  restTransforms[idx] = node.rest_transform;

  for (const auto &child : node.children) {
    if (!FlattenSkelNode(child, joints, jointNames, bindTransforms, restTransforms, err)) {
      return false;
    }
  }

  return true;
}


// TODO: skelAnimationSource
static bool ExportSkeleton(const SkelHierarchy &skel, Skeleton *dst, std::string *err) {

  size_t num_joints{0};
  CountNodes(skel.root_node, num_joints);

  std::vector<value::token> joints(num_joints);
  std::vector<value::token> jointNames(num_joints);
  std::vector<value::matrix4d> bindTransforms(num_joints);
  std::vector<value::matrix4d> restTransforms(num_joints);

  // Flatten hierachy.
  if (!FlattenSkelNode(skel.root_node, joints, jointNames, bindTransforms, restTransforms, err)) {
    return false;
  } 

  dst->joints.set_value(joints);

  // TODO: Do not author jointNames when for all i: joints[i] == jointNames[i];
  bool name_is_same = true;
  for (size_t i = 0; i < num_joints; i++) {
    if (joints[i].str() != jointNames[i].str()) {
      name_is_same = false;
      break;
    }
  }

  if (!name_is_same) {
    dst->jointNames.set_value(jointNames);
  }

  dst->bindTransforms.set_value(bindTransforms);
  dst->restTransforms.set_value(restTransforms);

  dst->name = skel.prim_name;

  return true;
}

static bool ExportBlendShape(const ShapeTarget &target, BlendShape *dst, std::string *err) {
  (void)err;

  dst->name = target.prim_name;
  if (target.display_name.size()) {
    dst->metas().displayName = target.display_name;
  }

  if (target.pointIndices.size()) {
    std::vector<int> indices(target.pointIndices.size());
    for (size_t i = 0; i < target.pointOffsets.size(); i++) {
      indices[i] = int(target.pointIndices[i]);
    }
    dst->pointIndices = indices;
  }

  if (target.pointOffsets.size()) {
    std::vector<value::vector3f> offsets(target.pointOffsets.size());
    for (size_t i = 0; i < target.pointOffsets.size(); i++) {
      offsets[i][0] = target.pointOffsets[i][0];
      offsets[i][1] = target.pointOffsets[i][1];
      offsets[i][2] = target.pointOffsets[i][2];
    }
    dst->offsets = offsets;
  }

  if (target.normalOffsets.size()) {
    std::vector<value::vector3f> normalOffsets(target.normalOffsets.size());
    for (size_t i = 0; i < target.normalOffsets.size(); i++) {
      normalOffsets[i][0] = target.normalOffsets[i][0];
      normalOffsets[i][1] = target.normalOffsets[i][1];
      normalOffsets[i][2] = target.normalOffsets[i][2];
    }
    dst->normalOffsets = normalOffsets;
  }

  return true;
}

// TODO: Support BlendShapes target
static bool ExportSkelAnimation(const Animation &anim, SkelAnimation *dst, std::string *err) {
  (void)err;
  dst->name = anim.prim_name;
  if (anim.display_name.size()) {
    dst->metas().displayName = anim.display_name;
  }

  if (anim.channels_map.empty()) {
    // TODO: Warn message
    return true;
  }

  StringAndIdMap joint_idMap;
  for (const auto &channels : anim.channels_map)
  {
    uint64_t joint_id = uint64_t(joint_idMap.size());
    joint_idMap.add(channels.first, uint64_t(joint_id));
  }

  std::vector<value::token> joints(joint_idMap.size());
  for (const auto &channels : anim.channels_map) {
    joints[joint_idMap.at(channels.first)] = value::token(channels.first);
  }
  dst->joints = joints;

  bool no_tx_channel{true};
  bool no_rot_channel{true};
  bool no_scale_channel{true};

  bool all_joints_has_tx_channel{true};
  bool all_joints_has_rot_channel{true};
  bool all_joints_has_scale_channel{true};

  for (const auto &channels : anim.channels_map) {

    bool has_tx_channel;
    bool has_rot_channel;
    bool has_scale_channel;

    has_tx_channel = channels.second.count(AnimationChannel::ChannelType::Translation);
    has_rot_channel = channels.second.count(AnimationChannel::ChannelType::Rotation);
    has_scale_channel = channels.second.count(AnimationChannel::ChannelType::Scale);

    if (has_tx_channel) {
      no_tx_channel = false;
    } else {
      all_joints_has_tx_channel = false;
    }

    if (has_rot_channel) {
      no_rot_channel = false;
    } else {
      all_joints_has_rot_channel = false;
    }

    if (has_scale_channel) {
      no_scale_channel = false;
    } else {
      all_joints_has_scale_channel = false;
    }
  }

  if (!no_tx_channel && !all_joints_has_tx_channel) {
    PUSH_ERROR_AND_RETURN("translation channel partially exists among joints. No joints have animation channel or all joints have animation channels.");
  }

  if (!no_rot_channel && !all_joints_has_rot_channel) {
    PUSH_ERROR_AND_RETURN("rotation channel partially exists among joints. No joints have animation channel or all joints have animation channels.");
  }

  if (!no_scale_channel && !all_joints_has_scale_channel) {
    PUSH_ERROR_AND_RETURN("scale channel partially exists among joints. No joints have animation channel or all joints have animation channels.");
  }

  if (no_tx_channel) {
    // Author static(default) value.
    std::vector<value::float3> translations;
    translations.assign(joints.size(), {1.0f, 1.0f, 1.0f});
    
    dst->translations.set_value(translations);
  } else {

    // All joints should have same timeCode.
    // First collect timeCodes
    std::unordered_set<float> timeCodes;

    for (const auto &channels : anim.channels_map) {

      const auto &tx_it = channels.second.find(AnimationChannel::ChannelType::Translation);
      if (tx_it != channels.second.end()) {
        for (size_t t = 0; t < tx_it->second.translations.samples.size(); t++) {
          timeCodes.insert(tx_it->second.translations.samples[t].t);
        }
      }

    }

    // key: timeCode. value: values for each joints.
    std::map<double, std::vector<value::float3>> ts_txs;
    for (const auto &tc : timeCodes) {
      ts_txs[double(tc)].resize(joints.size()); 
    }

    // Pack channel values
    for (const auto &channels : anim.channels_map) {

      const auto &tx_it = channels.second.find(AnimationChannel::ChannelType::Translation);
      if (tx_it != channels.second.end()) {

        for (size_t t = 0; t < tx_it->second.translations.samples.size(); t++) {
          float tc = tx_it->second.translations.samples[t].t;
          if (!timeCodes.count(tc)) {
            PUSH_ERROR_AND_RETURN(fmt::format("All animation channels have same timeCodes. timeCode {} is only seen in `translation` animation channel {}", tc, channels.first));
          }
          uint64_t joint_id = joint_idMap.at(channels.first);

          std::vector<value::float3> &txs = ts_txs.at(double(tc));
          // just in case
          if (joint_id > txs.size()) {
            PUSH_ERROR_AND_RETURN(fmt::format("Internal error. joint_id {} exceeds # of joints {}", joint_id, txs.size()));
          }
          txs[size_t(joint_id)] = tx_it->second.translations.samples[t].value;
        }
      }
    }

    Animatable<std::vector<value::float3>> ts;
    for (const auto &s : ts_txs) {
      ts.add_sample(s.first, s.second);
    } 

    dst->translations.set_value(ts);
  }

  if (no_rot_channel) {
    // Author static(default) value.
    std::vector<value::quatf> rots;
    value::quatf q;
    q.imag = {0.0f, 0.0f, 0.0f};
    q.real = 1.0f;
    rots.assign(joints.size(), q);

    dst->rotations.set_value(rots);
    
  } else {

    std::unordered_set<float> timeCodes;

    for (const auto &channels : anim.channels_map) {

      const auto &rot_it = channels.second.find(AnimationChannel::ChannelType::Rotation);
      if (rot_it != channels.second.end()) {
        for (size_t t = 0; t < rot_it->second.rotations.samples.size(); t++) {
          timeCodes.insert(rot_it->second.rotations.samples[t].t);
        }
      }

    }

    std::map<double, std::vector<value::quatf>> ts_rots;
    for (const auto &tc : timeCodes) {
      ts_rots[double(tc)].resize(joints.size()); 
    }

    for (const auto &channels : anim.channels_map) {

      const auto &rot_it = channels.second.find(AnimationChannel::ChannelType::Rotation);
      if (rot_it != channels.second.end()) {

        for (size_t t = 0; t < rot_it->second.rotations.samples.size(); t++) {
          float tc = rot_it->second.rotations.samples[t].t;
          if (!timeCodes.count(tc)) {
            PUSH_ERROR_AND_RETURN(fmt::format("All animation channels have same timeCodes. timeCode {} is only seen in `rotation` animation channel {}", tc, channels.first));
          }
          uint64_t joint_id = joint_idMap.at(channels.first);

          std::vector<value::quatf> &rots = ts_rots.at(double(tc));
          value::quatf v;
          v[0] = rot_it->second.rotations.samples[t].value[0];
          v[1] = rot_it->second.rotations.samples[t].value[1];
          v[2] = rot_it->second.rotations.samples[t].value[2];
          v[3] = rot_it->second.rotations.samples[t].value[3];
          rots[size_t(joint_id)] = v;
        }
      }
    }

    Animatable<std::vector<value::quatf>> ts;
    for (const auto &s : ts_rots) {
      ts.add_sample(s.first, s.second);
    } 

    dst->rotations.set_value(ts);
  }

  if (no_scale_channel) {
    // Author static(default) value.
    std::vector<value::half3> scales;
    scales.assign(joints.size(), {value::float_to_half_full(1.0f), value::float_to_half_full(1.0f), value::float_to_half_full(1.0f)});

    dst->scales.set_value(scales);
    
  } else {
    std::unordered_set<float> timeCodes;

    for (const auto &channels : anim.channels_map) {

      const auto &scale_it = channels.second.find(AnimationChannel::ChannelType::Scale);
      if (scale_it != channels.second.end()) {
        for (size_t t = 0; t < scale_it->second.scales.samples.size(); t++) {
          timeCodes.insert(scale_it->second.scales.samples[t].t);
        }
      }

    }

    std::map<double, std::vector<value::half3>> ts_scales;
    for (const auto &tc : timeCodes) {
      ts_scales[double(tc)].resize(joints.size()); 
    }

    for (const auto &channels : anim.channels_map) {

      const auto &scale_it = channels.second.find(AnimationChannel::ChannelType::Scale);
      if (scale_it != channels.second.end()) {

        for (size_t t = 0; t < scale_it->second.scales.samples.size(); t++) {
          float tc = scale_it->second.scales.samples[t].t;
          if (!timeCodes.count(tc)) {
            PUSH_ERROR_AND_RETURN(fmt::format("All animation channels have same timeCodes. timeCode {} is only seen in `scale` animation channel {}", tc, channels.first));
          }
          uint64_t joint_id = joint_idMap.at(channels.first);

          std::vector<value::half3> &scales = ts_scales.at(double(tc));
          value::half3 v;
          v[0] = value::float_to_half_full(scale_it->second.scales.samples[t].value[0]);
          v[1] = value::float_to_half_full(scale_it->second.scales.samples[t].value[1]);
          v[2] = value::float_to_half_full(scale_it->second.scales.samples[t].value[2]);
          scales[size_t(joint_id)] = v;
        }
      }
    }

    Animatable<std::vector<value::half3>> ts;
    for (const auto &s : ts_scales) {
      ts.add_sample(s.first, s.second);
    } 

    dst->scales.set_value(ts);
  }

  dst->name = anim.prim_name;
  if (anim.display_name.size()) {
    dst->metas().displayName = anim.display_name;
  }
  return true;
}

static bool ToGeomMesh(const RenderMesh &rmesh, GeomMesh *dst, std::string *err) {

  std::vector<int> fvCounts(rmesh.faceVertexCounts().size());
  for (size_t i = 0; i < rmesh.faceVertexCounts().size(); i++) {
    fvCounts[i] = int(rmesh.faceVertexCounts()[i]);
  }
  dst->faceVertexCounts.set_value(fvCounts);

  std::vector<int> fvIndices(rmesh.faceVertexIndices().size());
  for (size_t i = 0; i < rmesh.faceVertexIndices().size(); i++) {
    fvIndices[i] = int(rmesh.faceVertexIndices()[i]);
  }
  dst->faceVertexIndices.set_value(fvIndices);

  std::vector<value::point3f> points(rmesh.points.size());
  for (size_t i = 0; i < rmesh.points.size(); i++) {
    points[i][0] = rmesh.points[i][0];
    points[i][1] = rmesh.points[i][1];
    points[i][2] = rmesh.points[i][2];
  }

  dst->points = points;

  dst->name = rmesh.prim_name;
  if (rmesh.display_name.size()) {
    dst->meta.displayName = rmesh.display_name;
  }

  if (!rmesh.normals.empty()) {
    // export as primvars:normals

    const auto &vattr = rmesh.normals;
    std::vector<value::normal3f> normals(vattr.vertex_count());
    const float *psrc = reinterpret_cast<const float *>(vattr.buffer());
    for (size_t i = 0; i < vattr.vertex_count(); i++) {
      normals[i][0] = psrc[3 * i + 0];
      normals[i][1] = psrc[3 * i + 1];
      normals[i][2] = psrc[3 * i + 2];
    }

    GeomPrimvar normalPvar;
    normalPvar.set_name("normals");
    normalPvar.set_value(normals);
    if (vattr.is_facevarying()) {
      normalPvar.set_interpolation(Interpolation::FaceVarying);
    } else if (vattr.is_vertex()) {
      normalPvar.set_interpolation(Interpolation::Vertex);
    } else if (vattr.is_uniform()) {
      normalPvar.set_interpolation(Interpolation::Uniform);
    } else if (vattr.is_constant()) {
      normalPvar.set_interpolation(Interpolation::Constant);
    } else {
      PUSH_ERROR_AND_RETURN("Invalid variability in RenderMesh.normals");
    }

    // primvar name is extracted from Primvar::name
    dst->set_primvar(normalPvar);
  }

  // Primary texcoord only.
  // TODO: Multi-texcoords support
  if (rmesh.texcoords.count(0)) {
    const auto &vattr = rmesh.texcoords.at(0);
    std::vector<value::texcoord2f> texcoords(vattr.vertex_count());
    const float *psrc = reinterpret_cast<const float *>(vattr.buffer());
    for (size_t i = 0; i < vattr.vertex_count(); i++) {
      texcoords[i][0] = psrc[2 * i + 0];
      texcoords[i][1] = psrc[2 * i + 1];
    }

    GeomPrimvar uvPvar;
    uvPvar.set_name(vattr.name);
    uvPvar.set_value(texcoords);
    if (vattr.is_facevarying()) {
      uvPvar.set_interpolation(Interpolation::FaceVarying);
    } else if (vattr.is_vertex()) {
      uvPvar.set_interpolation(Interpolation::Vertex);
    } else if (vattr.is_uniform()) {
      uvPvar.set_interpolation(Interpolation::Uniform);
    } else if (vattr.is_constant()) {
      uvPvar.set_interpolation(Interpolation::Constant);
    } else {
      PUSH_ERROR_AND_RETURN("Invalid variability in RenderMesh.texcoord0");
    }

    dst->set_primvar(uvPvar);
  }

  // TODO: GeomSubset, Material assignment, skel binding, ...

  return true;
}

} // namespace

bool export_to_usda(const RenderScene &scene,
  std::string &usda_str, std::string *warn, std::string *err) {

  (void)warn;

  Stage stage;

  stage.metas().comment = "Exported from TinyUSDZ Tydra.";
  if (scene.meta.upAxis == "X") {
    stage.metas().upAxis = Axis::X;
  } else if (scene.meta.upAxis == "Y") {
    stage.metas().upAxis = Axis::Y;
  } else if (scene.meta.upAxis == "Z") {
    stage.metas().upAxis = Axis::Z;
  }

  // TODO: Construct Node hierarchy

  for (size_t i = 0; i < scene.meshes.size(); i++) {
    GeomMesh mesh;
    if (!detail::ToGeomMesh(scene.meshes[i], &mesh, err)) {
      return false;
    }

    Skeleton skel;
    bool has_skel{false};

    if ((scene.meshes[i].skel_id > -1) && (size_t(scene.meshes[i].skel_id) < scene.skeletons.size())) {
      DCOUT("Export Skeleton");
      if (!detail::ExportSkeleton(scene.skeletons[size_t(scene.meshes[i].skel_id)], &skel, err)) {
        return false;
      }
    
      has_skel = true;
    }

    std::vector<BlendShape> bss;
    if (scene.meshes[i].targets.size()) {

      std::vector<value::token> bsNames;
      Relationship bsTargets;

      for (const auto &target : scene.meshes[i].targets) {
        BlendShape bs;
        if (!detail::ExportBlendShape(target.second, &bs, err)) {
          return false;
        }

        bss.emplace_back(bs);
        bsNames.push_back(value::token(target.first));
        // TODO: Set abs_path
        Path targetPath = Path(mesh.name, "").AppendPrim(target.first);
        bsTargets.targetPathVector.push_back(targetPath);
      }

      mesh.blendShapeTargets = bsTargets;
      mesh.blendShapes = bsNames;

    }

    // Add BlendShape prim under GeomMesh prim.
    Prim meshPrim(mesh);

    if (bss.size()) {
      for (size_t t = 0; t < bss.size(); t++) {
        Prim bsPrim(bss[t]);
        meshPrim.add_child(std::move(bsPrim));
      }
    }

    if (has_skel) {

      SkelRoot skelRoot;
      skelRoot.set_name("skelRoot" + std::to_string(i)); 

      Prim skelPrim(skel);

      Prim skelRootPrim(skelRoot);
      skelRootPrim.add_child(std::move(meshPrim), /* rename_primname_if_require */true);
      skelRootPrim.add_child(std::move(skelPrim), /* rename_primname_if_require */true);

      stage.add_root_prim(std::move(skelRootPrim));

    } else {
      // Put Prims under Scope Prim
      Scope scope;
      scope.name = "scope" + std::to_string(i);

      Prim scopePrim(scope);
      scopePrim.add_child(std::move(meshPrim));

      stage.add_root_prim(std::move(scopePrim));
    }

  }

  for (size_t i = 0; i < scene.animations.size(); i++) {
    SkelAnimation skelAnim;
    if (!detail::ExportSkelAnimation(scene.animations[i], &skelAnim, err)) {
      return false;
    }

    // TODO: Put SkelAnimation under SkelRoot
    Prim prim(skelAnim);
    stage.add_root_prim(std::move(prim));
  }

  usda_str =stage.ExportToString();

  return true;
}


} // namespace tydra
} // namespace tinyusdz

