#pragma once

// A fallen limb touching a car does not necessarily obstruct standing up.
// Query real nearby vehicle collision volumes, without the world sphere
// query's conservative car-radius fallback or a previous contact pointer.
namespace QuestPedGetup {
inline bool Blocked(CPed *ped)
{
	enum { SPHERES=5, MAX_VEHICLES=16 };
	const float radius=0.23f;
	const float heights[SPHERES]={0.28f,0.68f,1.08f,1.48f,1.78f};
	CColSphere spheres[SPHERES];
	for(int i=0;i<SPHERES;i++)
		spheres[i].Set(radius,CVector(0.0f,0.0f,heights[i]-FEET_OFFSET));
	CColModel standing;
	standing.ownsCollisionVolumes=false;
	standing.spheres=spheres;
	standing.numSpheres=SPHERES;
	standing.numLines=standing.numBoxes=standing.numTriangles=0;
	const float low=heights[0]-FEET_OFFSET, high=heights[SPHERES-1]-FEET_OFFSET;
	standing.boundingSphere.Set((high-low)*0.5f+radius,CVector(0.0f,0.0f,(low+high)*0.5f));
	standing.boundingBox.Set(CVector(-radius,-radius,low-radius),CVector(radius,radius,high+radius));
	CMatrix transform;
	transform.SetTranslate(ped->GetPosition());
	CEntity *nearby[MAX_VEHICLES];
	int16 count=0;
	CWorld::FindObjectsKindaColliding(ped->GetPosition(),standing.boundingSphere.radius,
		true,&count,MAX_VEHICLES,nearby,false,true,false,false,false);
	// The bounded list may be incomplete in an exceptional pile-up. Wait rather
	// than declare an untested vehicle clear.
	if(count>=MAX_VEHICLES) return true;
	CColPoint contacts[MAX_COLLISION_POINTS];
	for(int i=0;i<count;i++){
		CEntity *entity=nearby[i];
		if(!entity || !entity->IsVehicle() || !entity->bUsesCollision || entity->bRemoveFromWorld) continue;
		CVehicle *vehicle=(CVehicle*)entity;
		if(vehicle->m_vehType==VEHICLE_TYPE_BIKE) continue;
		CColModel *model=vehicle->GetColModel();
		if(!model) return true;
		if(model->numSpheres<=0 && model->numBoxes<=0 && model->numTriangles<=0) return true;
		if(CCollision::ProcessColModels(transform,standing,vehicle->GetMatrix(),*model,contacts,nil,nil)>0)
			return true;
	}
	return false;
}
}
