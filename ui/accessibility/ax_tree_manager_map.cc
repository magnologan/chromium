// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager_map.h"

#include "base/stl_util.h"

namespace ui {

AXTreeManagerMap::AXTreeManagerMap() {}

AXTreeManagerMap::~AXTreeManagerMap() {}

AXTreeManagerMap& AXTreeManagerMap::GetInstance() {
  static base::NoDestructor<AXTreeManagerMap> instance;
  return *instance;
}

void AXTreeManagerMap::AddTreeManager(AXTreeID tree_id,
                                      AXTreeManager* manager) {
  if (tree_id != AXTreeIDUnknown())
    map_[tree_id] = manager;
}

void AXTreeManagerMap::RemoveTreeManager(AXTreeID tree_id) {
  if (tree_id != AXTreeIDUnknown())
    map_.erase(tree_id);
}

AXTreeManager* AXTreeManagerMap::GetManager(AXTreeID tree_id) {
  if (tree_id == AXTreeIDUnknown() || !base::Contains(map_, tree_id))
    return nullptr;

  return map_.at(tree_id);
}

AXTreeManager* AXTreeManagerMap::GetManagerForChildTree(
    const AXNode& parent_node) {
  if (!parent_node.data().HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId)) {
    return nullptr;
  }

  AXTreeID child_tree_id =
      AXTreeID::FromString(parent_node.data().GetStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId));
  AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManager(child_tree_id);

  DCHECK(child_tree_manager &&
         child_tree_manager->GetParentNodeFromParentTreeAsAXNode()->id() ==
             parent_node.id());

  return child_tree_manager;
}

}  // namespace ui
