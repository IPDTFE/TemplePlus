#define _CRT_SECURE_NO_WARNINGS

#include <infrastructure/meshes.h>
#include <infrastructure/exception.h>
#include <infrastructure/logging.h>
#include <infrastructure/mesparser.h>
#include <graphics/mdfmaterials.h>

#include <MinHook.h>

#include "temple/dll.h"
#include "temple/meshes.h"
#include "temple/aas.h"
#include <gsl/gsl.h>

namespace temple {

	static struct AasFunctions : AddressTable {

		int (*Init)(const LegacyAasConfig* config);
		int (*Exit)();

		int (__cdecl *FreeSubmesh)(AasSubmesh* submesh);

		AasStatus (__cdecl *CreateModelByIds)(int skmId, int skaId, int idleAnimId, const AasAnimParams* animState, AasHandle* aasHandleOut);

		AasStatus (__cdecl *CreateModelByNames)(const char* skmFilename,
		                                        const char* skaFilename,
		                                        int idleType,
		                                        const AasAnimParams* animState,
		                                        AasHandle* aasHandleOut);

		int (__cdecl *GetSubmesh)(AasHandle aasHandle, AasSubmesh** submeshOut, const AasAnimParams* animState, int submeshIdx);

		int (__cdecl *GetSubmeshForParticles)(AasHandle aasHandle, AasSubmesh** submeshOut, const AasAnimParams* animState, int submeshIdx);

		int (__cdecl *GetSubmeshes)(AasHandle aasHandle, int** submeshMaterials, int* pSubmeshCountOut);

		int (__cdecl *AddAddMesh)(AasHandle aasHandle, const char* addMeshName);

		int (__cdecl *ClearAddMeshes)(AasHandle aasHandle);

		int (__cdecl *Advance)(AasHandle aasHandle,
		                       float deltaTime,
		                       float deltaDistance,
		                       float deltaRotation,
		                       const AasAnimParams* params,
		                       AasEventFlag* eventOut);

		AasStatus(__cdecl *Free)(AasHandle aasHandle);
		
		void (__cdecl *FreeAll)();

		int (__cdecl *GetAnimId)(AasHandle aasHandle, int* animIdOut);

		int (__cdecl *GetBoneCount)(AasHandle aasHandle);

		const char*(__cdecl *GetBoneNameById)(AasHandle aasHandle, int boneId);

		int (__cdecl *GetBoneParentId)(AasHandle aasHandle, int boneId);

		int (__cdecl *GetBoneWorldMatrixByName)(AasHandle aasHandle,
		                                        const AasAnimParams* params,
												DirectX::XMFLOAT4X4* worldMatrixOut,
		                                        const char* boneName);

		int (__cdecl *GetBoneWorldMatrixByNameForChild)(AasHandle parentAasHandle,
		                                                AasHandle childAasHandle,
		                                                const AasAnimParams* params,
														DirectX::XMFLOAT4X4* worldMatrixOut,
		                                                const char* boneName);

		float (__cdecl *GetDistPerSec)(AasHandle handle);

		float (__cdecl *GetRotationPerSec)(AasHandle handle);

		/*
		Since this actually sets the anim ID returned by "GetAnimId", this might
		not just check that an anim is supported, but it definetly doesn't "start"
		the animation right away as SetAnimId does.
		*/
		int (__cdecl *HasAnimId)(AasHandle handle, int animId);


		int (__cdecl *HasBone)(AasHandle aasHandle, const char* boneName);

		int (__cdecl *ReplaceSpecialMaterial)(AasHandle aasHandle, int materialId);

		int (__cdecl *SetAnimId)(AasHandle aasHandle, int animId);

		/*
		Not sure what this does, but it only does something if the animated model has
		cloth bones. Otherwise it's a noop. So presumably, it has something to do 
		with cloth simulation.
	*/
		int (__cdecl *SetClothFlagSth)(AasHandle aasHandle);

		/*
	Used by the particle system model renderer before getting the submesh.
	animTime in this case is the particle's lifetime, for which the model is
	to be rendered.
	*/
		int (__cdecl *SetTime)(AasHandle aasHandle, float time, const AasAnimParams* params);

		AasFunctions() {
			rebase(Init, 0x102640B0);
			rebase(Exit, 0x102645E0);
			rebase(FreeSubmesh, 0x10262500);
			rebase(CreateModelByIds, 0x102641B0);
			rebase(CreateModelByNames, 0x102643A0);
			rebase(GetSubmesh, 0x10263400);
			rebase(GetSubmeshForParticles, 0x102636D0);
			rebase(GetSubmeshes, 0x10263A50);
			rebase(AddAddMesh, 0x10262E30);
			rebase(ClearAddMeshes, 0x10262EC0);
			rebase(Advance, 0x10262C10);
			rebase(Free, 0x10264510);
			rebase(GetAnimId, 0x102627E0);
			rebase(GetBoneCount, 0x10262F40);
			rebase(GetBoneNameById, 0x10262F80);
			rebase(GetBoneParentId, 0x10262FC0);
			rebase(GetBoneWorldMatrixByName, 0x10263000);
			rebase(GetBoneWorldMatrixByNameForChild, 0x102631E0);
			rebase(GetDistPerSec, 0x10263DA0);
			rebase(GetRotationPerSec, 0x10263DE0);
			rebase(HasAnimId, 0x10262690);
			rebase(HasBone, 0x10263A10);
			rebase(ReplaceSpecialMaterial, 0x10263AB0);
			rebase(SetAnimId, 0x10262540);
			rebase(SetClothFlagSth, 0x102633C0);
			rebase(SetTime, 0x10263B90);
			rebase(FreeAll, 0x10264650);
		}
	} functions;

	class AasSubmeshAdapter : public gfx::Submesh {
	public:
		AasSubmeshAdapter(AasHandle parent, const AasAnimParams& params, int submeshId, bool forParticles);
		~AasSubmeshAdapter();

		int GetVertexCount() override {
			return mSubmesh->vertexCount;
		}

		int GetPrimitiveCount() override {
			return mSubmesh->primCount;
		}

		gsl::span<DirectX::XMFLOAT4> GetPositions() override {
			auto data = reinterpret_cast<DirectX::XMFLOAT4*>(mSubmesh->positions);
			return gsl::as_span(data, GetVertexCount());
		}

		gsl::span<DirectX::XMFLOAT4> GetNormals() override {
			auto data = reinterpret_cast<DirectX::XMFLOAT4*>(mSubmesh->normals);
			return gsl::as_span(data, GetVertexCount());
		}

		gsl::span<DirectX::XMFLOAT2> GetUV() override {
			auto data = reinterpret_cast<DirectX::XMFLOAT2*>(mSubmesh->uv);
			return gsl::as_span(data, GetVertexCount());
		}

		gsl::span<uint16_t> GetIndices() override {
			return gsl::as_span(mSubmesh->indices, GetPrimitiveCount() * 3);
		}

	private:
		AasSubmesh* mSubmesh = nullptr;
	};

	AasSubmeshAdapter::AasSubmeshAdapter(AasHandle aasHandle, const AasAnimParams& params, int submeshIdx, bool forParticles) {
		if (!forParticles) {
			functions.GetSubmesh(aasHandle, &mSubmesh, &params, submeshIdx);
		} else {
			functions.GetSubmeshForParticles(aasHandle, &mSubmesh, &params, submeshIdx);
		}
	}

	AasSubmeshAdapter::~AasSubmeshAdapter() {
		if (mSubmesh) {
			free(mSubmesh);
		}
	}

	class AasAnimatedModel : public gfx::AnimatedModel {
	public:

		explicit AasAnimatedModel(AasHandle handle) : mHandle(handle) {
		}

		AasAnimatedModel(AasHandle handle, bool borrowed) : mHandle(handle), mBorrowed(borrowed) {
		}

		~AasAnimatedModel();

		uint32_t GetHandle() const override {
			return mHandle;
		}

		bool AddAddMesh(const ::std::string& filename) override;
		bool ClearAddMeshes() override;
		gfx::AnimatedModelEvents Advance(float deltaTime, float deltaDistance, float deltaRotation, const ::gfx::AnimatedModelParams& params) override;
		gfx::EncodedAnimId GetAnimId() const override;
		int GetBoneCount() const override;
		std::string GetBoneName(int boneId) override;
		int GetBoneParentId(int boneId) override;
		bool GetBoneWorldMatrixByName(const ::gfx::AnimatedModelParams& params, const ::std::string& boneName, DirectX::XMFLOAT4X4* worldMatrixOut) override;
		bool GetBoneWorldMatrixByNameForChild(const gfx::AnimatedModelPtr& child, const gfx::AnimatedModelParams& params, const std::string& boneName, DirectX::XMFLOAT4X4* worldMatrixOut) override;
		float GetDistPerSec() const override;
		float GetRotationPerSec() const override;
		bool HasAnim(::gfx::EncodedAnimId animId) const override;
		void SetTime(const ::gfx::AnimatedModelParams& params, float timeInSecs) override;
		bool HasBone(const ::std::string& boneName) const override;
		void AddReplacementMaterial(gfx::MaterialPlaceholderSlot slot,
			const gfx::MdfRenderMaterialPtr &material) override;
		void SetAnimId(int animId) override;
		void SetClothFlag() override;
		std::vector<int> GetSubmeshes() override;
		std::unique_ptr<gfx::Submesh> GetSubmesh(const ::gfx::AnimatedModelParams& params, int submeshIdx) override;
		std::unique_ptr<gfx::Submesh> GetSubmeshForParticles(const ::gfx::AnimatedModelParams& params, int submeshIdx) override;
		float GetHeight(int scale) override;
		float GetRadius(int scale) override;

		static AasAnimParams Convert(const gfx::AnimatedModelParams& params);
		
	private:
		AasHandle mHandle;
		bool mBorrowed = false;
	};

	AasAnimParams AasAnimatedModel::Convert(const gfx::AnimatedModelParams& params) {
		AasAnimParams result;
		result.flags = 0;
		result.locX = params.x;
		result.locY = params.y;
		result.scale = params.scale;
		result.offsetX = params.offsetX;
		result.offsetY = params.offsetY;
		result.offsetZ = params.offsetZ;
		result.rotation = params.rotation;
		result.rotationYaw = params.rotationYaw;
		result.rotationPitch = params.rotationPitch;
		result.rotationRoll = params.rotationRoll;
		strncpy_s(result.attachedBoneName, params.attachedBoneName.c_str(), sizeof(result.attachedBoneName) - 1);
		result.attachedBoneName[sizeof(result.attachedBoneName) - 1] = '\0';
		result.unknown = 0;
		auto parentAnim = std::static_pointer_cast<AasAnimatedModel>(params.parentAnim);
		if (parentAnim) {
			result.parentAnim = parentAnim->mHandle;
			result.flags = 2;
		} else {
			result.parentAnim = 0;
			result.flags = 1;
		}
		return result;
	}

	AasAnimatedModel::~AasAnimatedModel() {
		if (!mBorrowed) {
			functions.Free(mHandle);
		}
	}

	bool AasAnimatedModel::AddAddMesh(const ::std::string& filename) {
		return functions.AddAddMesh(mHandle, filename.c_str()) == AAS_OK;
	}

	bool AasAnimatedModel::ClearAddMeshes() {
		return functions.ClearAddMeshes(mHandle) == AAS_OK;
	}

	::gfx::AnimatedModelEvents AasAnimatedModel::Advance(float deltaTime, float deltaDistance, float deltaRotation, const ::gfx::AnimatedModelParams& params) {
		AasEventFlag eventsTriggered;
		auto aasParams(Convert(params));
		functions.Advance(mHandle, deltaTime, deltaDistance, deltaRotation, &aasParams, &eventsTriggered);

		return {
			(eventsTriggered & AEF_END) != 0,
			(eventsTriggered & AEF_ACTION) != 0
		};
	}

	::gfx::EncodedAnimId AasAnimatedModel::GetAnimId() const {
		int animId;
		functions.GetAnimId(mHandle, &animId);
		return gfx::EncodedAnimId(animId);
	}

	int AasAnimatedModel::GetBoneCount() const {
		return functions.GetBoneCount(mHandle);
	}

	::std::string AasAnimatedModel::GetBoneName(int boneId) {
		return functions.GetBoneNameById(mHandle, boneId);
	}

	int AasAnimatedModel::GetBoneParentId(int boneId) {
		return functions.GetBoneParentId(mHandle, boneId);
	}

	bool AasAnimatedModel::GetBoneWorldMatrixByName(const gfx::AnimatedModelParams& params, 
		const std::string& boneName, 
		DirectX::XMFLOAT4X4* worldMatrixOut) {
		auto aasParams(Convert(params));
		return functions.GetBoneWorldMatrixByName(mHandle, &aasParams, worldMatrixOut, boneName.c_str()) == AAS_OK;
	}

	bool AasAnimatedModel::GetBoneWorldMatrixByNameForChild(const gfx::AnimatedModelPtr& child, const gfx::AnimatedModelParams& params, const std::string& boneName, DirectX::XMFLOAT4X4* worldMatrixOut) {
		auto realChild = std::static_pointer_cast<AasAnimatedModel>(child);
		auto aasParams(Convert(params));
		return functions.GetBoneWorldMatrixByNameForChild(mHandle, realChild->mHandle, &aasParams, worldMatrixOut, boneName.c_str()) == AAS_OK;
	}

	float AasAnimatedModel::GetDistPerSec() const {
		return functions.GetDistPerSec(mHandle);
	}

	float AasAnimatedModel::GetRotationPerSec() const {
		return functions.GetRotationPerSec(mHandle);
	}

	bool AasAnimatedModel::HasAnim(::gfx::EncodedAnimId animId) const {
		return functions.HasAnimId(mHandle, animId) == AAS_OK;
	}

	void AasAnimatedModel::SetTime(const ::gfx::AnimatedModelParams& params, float timeInSecs) {
		auto aasParams(Convert(params));
		functions.SetTime(mHandle, timeInSecs, &aasParams);
	}

	bool AasAnimatedModel::HasBone(const ::std::string& boneName) const {
		return functions.HasBone(mHandle, boneName.c_str()) == AAS_OK;
	}

	void AasAnimatedModel::AddReplacementMaterial(gfx::MaterialPlaceholderSlot slot,
		const gfx::MdfRenderMaterialPtr &material) {

		uint32_t materialId = material->GetId();

		switch (slot) {
		case gfx::MaterialPlaceholderSlot::HEAD:
			materialId |= 0x84000000;
			break;
		case gfx::MaterialPlaceholderSlot::GLOVES:
			materialId |= 0x88000000;
			break;
		case gfx::MaterialPlaceholderSlot::BOOTS:
			materialId |= 0xA0000000;
			break;
		case gfx::MaterialPlaceholderSlot::CHEST:
			materialId |= 0x90000000;
			break;
		}

		functions.ReplaceSpecialMaterial(mHandle, materialId);
	}

	void AasAnimatedModel::SetAnimId(int animId) {
		functions.SetAnimId(mHandle, animId);
	}

	void AasAnimatedModel::SetClothFlag() {
		functions.SetClothFlagSth(mHandle);
	}

	::std::vector<int> AasAnimatedModel::GetSubmeshes() {
		int* materialIds;
		int submeshCount;
		if (functions.GetSubmeshes(mHandle, &materialIds, &submeshCount) != AAS_OK) {
			return {};
		}

		return ::std::vector<int>(materialIds, materialIds + submeshCount);
	}

	::std::unique_ptr<::gfx::Submesh> AasAnimatedModel::GetSubmesh(const ::gfx::AnimatedModelParams& params, int submeshIdx) {
		auto aasParams(Convert(params));
		return std::make_unique<AasSubmeshAdapter>(mHandle, aasParams, submeshIdx, false);
	}

	::std::unique_ptr<::gfx::Submesh> AasAnimatedModel::GetSubmeshForParticles(const ::gfx::AnimatedModelParams& params, int submeshIdx) {
		auto aasParams(Convert(params));
		return std::make_unique<AasSubmeshAdapter>(mHandle, aasParams, submeshIdx, true);
	}

	float AasAnimatedModel::GetHeight(int scale)
	{
		SetClothFlag();

		gfx::AnimatedModelParams animParams;
		memset(&animParams, 0, sizeof(animParams));		
		animParams.scale = scale / 100.0f;
	
		Advance(0.0f, 0.0f, 0.0f, animParams);

		auto materialIds = GetSubmeshes();

		auto maxHeight = -10000.0f;
		auto minHeight = 10000.0f;

		for (uint32_t i = 0; i < materialIds.size(); i++) {
			auto submesh = GetSubmesh(animParams, i);
			auto positions = submesh->GetPositions();

			for (auto j = 0; j < submesh->GetVertexCount(); j++) {
				auto y = positions[j].y;
				if (y < minHeight) {
					minHeight = y;
				}
				if (y > maxHeight) {
					maxHeight = y;
				}
				if (y < -500 || y > 500) {
					__debugbreak();
				}
			}
		}

		// No idea how they arrived at this value
		const auto defaultHeight = 28.8f;

		if (maxHeight == -10000.0f) {
			maxHeight = defaultHeight;
		} else if (maxHeight <= 0) {
			maxHeight = maxHeight - minHeight;
			if (maxHeight <= 0.01f) {
				maxHeight = defaultHeight;
			}
		}
		
		return maxHeight;
	}

	float AasAnimatedModel::GetRadius(int scale)
	{
		SetClothFlag();

		gfx::AnimatedModelParams animParams;
		memset(&animParams, 0, sizeof(animParams));
		animParams.scale = scale / 100.0f;

		Advance(0.0f, 0.0f, 0.0f, animParams);

		auto materialIds = GetSubmeshes();

		auto maxRadiusSquared = -10000.0f;

		for (uint32_t i = 0; i < materialIds.size(); i++) {
			auto submesh = GetSubmesh(animParams, i);
			auto positions = submesh->GetPositions();

			for (auto j = 0; j < submesh->GetVertexCount(); j++) {
				auto &pos = positions[j];

				// Distance from model origin (squared)
				auto distSq = pos.x * pos.x + pos.z * pos.z;

				if (pos.x > 500 || pos.z > 500) {
					__debugbreak();
				}

				if (distSq > maxRadiusSquared) {
					maxRadiusSquared = distSq;
				}
			}
		}

		// No idea how they arrived at this value
		if (maxRadiusSquared <= 0) {
			return 0;
		} else {
			return sqrtf(maxRadiusSquared);
		}
	}

	AasAnimatedModelFactory* AasAnimatedModelFactory::sInstance = nullptr;
	
	int __stdcall AasAnimatedModelFactory::AasResolveMaterial(const char *filename, int, int) {

		std::string name(filename);

		// Handle material replacement slots
		if (name == "HEAD") {
			return 0x84000000;
		} else if (name == "GLOVES") {
			return 0x88000000;
		} else if (name == "CHEST") {
			return 0x90000000;
		} else if (name == "BOOTS") {
			return 0xA0000000;
		}

		if (sInstance->mConfig.resolveMaterial) {
			return sInstance->mConfig.resolveMaterial(name);
		}
		return 0;
	}

	int AasAnimatedModelFactory::AasFreeModel(temple::AasHandle handle)
	{
		for (auto &listener : sInstance->mListeners) {
			listener(handle);
		}

		return sInstance->mOrgModelFree(handle);
	}

	AasAnimatedModelFactory::AasAnimatedModelFactory(const AasConfig& config) : mConfig(config) {
		Expects(!sInstance);
		sInstance = this;

		LegacyAasConfig legacyConfig;
		legacyConfig.scaleX = config.scaleX;
		legacyConfig.scaleY = config.scaleY;
		legacyConfig.getSkaFilename = [](int meshId, char* filenameOut) -> AasStatus {
			if (sInstance->mConfig.resolveSkaFile) {
				auto filename(sInstance->mConfig.resolveSkaFile(meshId));
				if (!filename.empty()) {
					Expects(filename.size() < MAX_PATH);
					strncpy(filenameOut, filename.c_str(), MAX_PATH);
					return AAS_OK;
				}
			}
			return AAS_ERROR;
		};
		legacyConfig.getSkmFilename = [](int meshId, char* filenameOut) -> AasStatus {
			if (sInstance->mConfig.resolveSkmFile) {
				auto filename(sInstance->mConfig.resolveSkmFile(meshId));
				if (!filename.empty()) {
					Expects(filename.size() < MAX_PATH);
					strncpy(filenameOut, filename.c_str(), MAX_PATH);
					return AAS_OK;
				}
			}
			return AAS_ERROR;
		};
		legacyConfig.runScript = [](const char* script) {
			if (sInstance->mConfig.runScript) {
				sInstance->mConfig.runScript(script);
			}
		};

		if (functions.Init(&legacyConfig)) {
			throw TempleException("Unable to initialize the animation system.");
		}

		// The MDF resolver is not configurable, so we have to set it here manually
		MH_CreateHook(temple::GetPointer<void*>(0x10269430), &AasResolveMaterial, nullptr);
		MH_CreateHook(functions.Free, &AasFreeModel, (void**)&mOrgModelFree);
		MH_EnableHook(nullptr);

		auto meshesMapping = MesFile::ParseFile("art/meshes/meshes.mes");
		mMapping.insert(meshesMapping.begin(), meshesMapping.end());
		logger->debug("Loaded mapping for {} meshes from art/meshes/meshes.mes", 
			meshesMapping.size());

	}

	AasAnimatedModelFactory::~AasAnimatedModelFactory() {
		functions.Exit();
		MH_RemoveHook(temple::GetPointer<void*>(0x10269430));
		MH_RemoveHook(functions.Free);
		sInstance = nullptr;
	}

	gfx::AnimatedModelPtr AasAnimatedModelFactory::FromIds(int meshId,
	                                                       int skeletonId,
	                                                       gfx::EncodedAnimId idleAnimId,
	                                                       const gfx::AnimatedModelParams& params,
														   bool borrow) {

		AasHandle handle;
		auto aasParams(AasAnimatedModel::Convert(params));
		if (functions.CreateModelByIds(meshId, skeletonId, idleAnimId, &aasParams, &handle) != AAS_OK) {
			throw TempleException("Could not load model {} with skeleton {}.", meshId, skeletonId);
		}

		return std::make_shared<AasAnimatedModel>(handle, borrow);

	}

	gfx::AnimatedModelPtr AasAnimatedModelFactory::FromFilenames(const std::string& meshFilename,
	                                                             const std::string& skeletonFilename,
	                                                             gfx::EncodedAnimId idleAnimId,
	                                                             const gfx::AnimatedModelParams& params) {

		AasHandle handle;
		auto aasParams(AasAnimatedModel::Convert(params));
		if (functions.CreateModelByNames(meshFilename.c_str(), skeletonFilename.c_str(), idleAnimId, &aasParams, &handle) != AAS_OK) {
			throw TempleException("Could not load model {} with skeleton {}.", meshFilename, skeletonFilename);
		}

		return std::make_shared<AasAnimatedModel>(handle);

	}

	std::unique_ptr<gfx::AnimatedModel> AasAnimatedModelFactory::BorrowByHandle(AasHandle handle) {
		return std::make_unique<AasAnimatedModel>(handle, true);
	}

	AasFreeListenerHandle AasAnimatedModelFactory::AddFreeListener(AasFreeListener listener)
	{
		mListeners.push_back(listener);
		auto it = mListeners.end();
		--it;
		return it;
	}

	void AasAnimatedModelFactory::RemoveFreeListener(AasFreeListenerHandle it)
	{
		mListeners.erase(it);
	}

	void AasAnimatedModelFactory::InvalidateBuffers(AasHandle handle) {
		for (auto &listener : sInstance->mListeners) {
			listener(handle);
		}
	}

	void AasAnimatedModelFactory::FreeAll()
	{
		functions.FreeAll();
	}

}

