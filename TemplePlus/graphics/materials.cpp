#include "stdafx.h"

#include "materials.h"
#include "materials_hooks.h"

#include <infrastructure/mdfparser.h>
#include <infrastructure/vfs.h>

class ClipperMaterial : gfx::Material {

};

MdfRenderMaterial::MdfRenderMaterial(gfx::LegacyShaderId id,
                                     const std::string& name,
                                     std::unique_ptr<class gfx::MdfMaterial>&& spec)
	: mId(id), mName(name), mSpec(std::move(spec)) {

	// Resolve texture references based on type
	switch (mSpec->GetType()) {
	case gfx::MdfType::Textured:
		LoadTexturedState(static_cast<gfx::MdfTexturedMaterial*>(mSpec.get()));
		break;
	case gfx::MdfType::General:
		LoadGeneralState(static_cast<gfx::MdfGeneralMaterial*>(mSpec.get()));
		break;
	default:
		break;
	}

}

gfx::TextureRef MdfRenderMaterial::GetPrimaryTexture() {
	return mTextures[0];
}

void MdfRenderMaterial::LoadTexturedState(gfx::MdfTexturedMaterial* material) {
	const auto& texture(material->GetTexture());

	// A textured MDF can have a single texture (but doesnt need to)
	if (texture.empty()) {
		return;
	}

	mTextures[0] = gfx::textureManager->Resolve(texture, true);
	if (!mTextures[0]->IsValid()) {
		logger->warn("Textured shader {} references invalid texture '{}'", mName, texture);
	}
}

void MdfRenderMaterial::LoadGeneralState(gfx::MdfGeneralMaterial* material) {

	Expects(material->samplers.size() <= MaxTextures);

	// A general MDF can reference up to 4 textures
	for (size_t i = 0; i < material->samplers.size(); ++i) {
		const auto& texture = material->samplers[i].filename;
		if (texture.empty())
			continue;

		mTextures[i] = gfx::textureManager->Resolve(texture, true);
		if (!mTextures[i]->IsValid()) {
			logger->warn("General shader {} references invalid texture '{}' in sampler {}",
			             mName, texture, i);
		}
	}

	if (!material->glossmap.empty()) {
		mGlossMap = gfx::textureManager->Resolve(material->glossmap, true);
		if (!mGlossMap->IsValid()) {
			logger->warn("General shader {} references invalid gloss map texture '{}'", 
				mName, material->glossmap);
		}
	}

}

MdfMaterialFactory::MdfMaterialFactory() {

	// Register material placeholders that have special names
	auto head = std::make_shared<MaterialPlaceholder>(0x84000001, MaterialPlaceholderSlot::HEAD, "HEAD");
	mIdRegistry[head->GetId()] = head;
	mNameRegistry[tolower(head->GetName())] = head;

	auto gloves = std::make_shared<MaterialPlaceholder>(0x88000001, MaterialPlaceholderSlot::GLOVES, "GLOVES");
	mIdRegistry[gloves->GetId()] = gloves;
	mNameRegistry[tolower(gloves->GetName())] = gloves;

	auto chest = std::make_shared<MaterialPlaceholder>(0x90000001, MaterialPlaceholderSlot::CHEST, "CHEST");
	mIdRegistry[chest->GetId()] = chest;
	mNameRegistry[tolower(chest->GetName())] = chest;

	auto boots = std::make_shared<MaterialPlaceholder>(0xA0000001, MaterialPlaceholderSlot::BOOTS, "BOOTS");
	mIdRegistry[boots->GetId()] = boots;
	mNameRegistry[tolower(boots->GetName())] = boots;

}

MdfMaterialFactory::~MdfMaterialFactory() {

	// This is sad, but the hooks keep an owned reference to
	// a material
	ClearMaterialsHooksState();

}

gfx::MaterialRef MdfMaterialFactory::GetById(int id) {
	auto it = mIdRegistry.find(id);
	if (it != mIdRegistry.end()) {
		return it->second;
	}
	return nullptr;
}

gfx::MaterialRef MdfMaterialFactory::LoadMaterial(const std::string& name) {

	auto nameLower = tolower(name);

	auto it = mNameRegistry.find(nameLower);
	if (it != mNameRegistry.end()) {
		return it->second;
	}

	try {
		auto mdfContent = vfs->ReadAsString(name);
		gfx::MdfParser parser(name, mdfContent);
		auto mdfMaterial(parser.Parse());

		// Assign ID
		auto id = mNextFreeId++;

		auto result(std::make_shared<MdfRenderMaterial>(id, name, std::move(mdfMaterial)));

		mIdRegistry[id] = result;
		mNameRegistry[nameLower] = result;

		return result;
	} catch (std::exception &e) {
		logger->error("Unable to load MDF file '{}': {}", name, e.what());
		return gfx::Material::GetInvalidMaterial();
	}

}