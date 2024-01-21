#pragma once
#include<vk_types.h>
#include<mesh/mesh_asset.h>
#include<node/node.h>
#include<draw/draw_context.h>
struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};