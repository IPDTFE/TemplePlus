
#pragma once

#include "gamesystem.h"

#include <graphics/math.h>

struct GameSystemConf;

namespace gfx {
	class WorldCamera;
}

namespace particles {
	using PartSysSpecPtr = std::shared_ptr<class PartSysSpec>;
	using PartSysPtr = std::shared_ptr<class PartSys>;
}

class ParticleSysSystem : public GameSystem, public TimeAwareGameSystem {
public:
	using Handle = int;
	using Map = std::unordered_map<Handle, particles::PartSysPtr>;

	static constexpr auto Name = "ParticleSys";
	ParticleSysSystem(gfx::WorldCamera &camera);
	~ParticleSysSystem();

	void AdvanceTime(uint32_t time) override;

	const std::string &GetName() const override;
		
	Handle CreateAt(uint32_t nameHash, XMFLOAT3 pos);

	Map::const_iterator begin() const {
		return mActiveSys.begin();
	}
	
	Map::const_iterator end() const {
		return mActiveSys.end();
	}

	/**
	 * Removes all active particle systems i.e. for changing the map.
	 */
	void RemoveAll();

private:
	std::unordered_map<uint32_t, particles::PartSysSpecPtr> mPartSysByHash;
	std::unordered_map<std::string, particles::PartSysSpecPtr> mPartSysByName;

	uint32_t mLastSimTime = 0;
	int mNextId = 1;
	Map mActiveSys;
	std::unique_ptr<class PartSysExternal> mExternal;
};